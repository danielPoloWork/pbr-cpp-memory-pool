# ADR-0021: Template Method Allocation Skeleton with Thread-Safety Hook Points

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) (the compile-time thread-safety **Strategy** this skeleton hosts), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1/§6 (the implicit free list the hooks mutate and the immutable fields the skeleton reads), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the foreign-pointer guard the skeleton keeps outside the synchronized region), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the per-pool budget — unchanged in M4.2), [ROADMAP](../../ROADMAP.md) §4.2 (this item) ← §4.1 (ADR-0020) → §4.3 (the policy implementations + macro).

## Context

[ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) decided that thread safety is a compile-time **Strategy**: a policy type (`SingleThreadedPolicy` / `MutexPolicy` / `LockFreePolicy`) supplies the synchronization, selected at build time so the single-threaded path pays nothing. Milestone 4.2 builds the *frame* those policies plug into — the **Template Method** allocation skeleton — and ships the first (no-op) policy. The actual mutex/lock-free policies and the `PBR_MEMORY_POOL_THREAD_SAFETY` selector are Milestone 4.3.

The design question is the **hook granularity**: where exactly does the invariant algorithm end and the interchangeable synchronization begin? Get this wrong and the seam fits the lock-based policies but not the lock-free one (whose control flow is a CAS retry loop, not a critical section).

## Decision

### 1. Two hooks: a synchronized `pop_head` and `push_head`; the skeleton owns everything else

The `alloc` / `free` algorithm is split into an invariant **skeleton** (the Template Method) and exactly two policy **hook points**:

```cpp
// Policy contract (compile-time; each policy is a struct of static methods):
//   void* Policy::pop_head(memory_pool* p)             // synchronized: detach & return the head block, or nullptr if empty
//   void  Policy::push_head(memory_pool* p, void* blk) // synchronized: prepend blk to the free list

template <typename SyncPolicy>
void* alloc_skeleton(memory_pool* pool) noexcept {
    if (pool == nullptr) { return nullptr; }      // race-free guard (immutable handle)
    return SyncPolicy::pop_head(pool);            // hook: synchronized pop
}

template <typename SyncPolicy>
void free_skeleton(memory_pool* pool, void* block) noexcept {
    if (pool == nullptr)  { return; }             // race-free guards…
    if (block == nullptr) { return; }
    if (!is_block_in_range(pool, block)) { return; }  // ADR-0012, reads only immutable fields
    SyncPolicy::push_head(pool, block);           // hook: synchronized push
}
```

The skeleton owns the **race-free guards** — the null-`pool`, null-`block`, and foreign-pointer/out-of-range checks (ADR-0012). These read only fields that are immutable after `memory_pool_create` (`backing_`, `block_size_`, `block_count_`), so they need no synchronization and are deliberately kept *outside* the policy's critical region: a foreign pointer is rejected without ever taking a lock. Each policy owns only the **synchronized head mutation** — the one place `head_` is read-modified-written.

### 2. The exhaustion check lives inside `pop_head`, not the skeleton

`pop_head` returns `nullptr` on an empty pool — the `head_ == nullptr` test is *inside* the hook, not in the skeleton. This is the decision that makes the seam fit all three policies: a lock-free `pop_head` must re-read `head_` on every iteration of its CAS retry loop, so the emptiness test has to be part of that loop, not a one-shot check the skeleton did beforehand. Putting it in the hook lets `SingleThreadedPolicy` and `MutexPolicy` test once while `LockFreePolicy` tests per-iteration — same contract, three faithful realizations.

### 3. M4.2 ships one policy, hard-wired; the policies are internal to the TU

`SingleThreadedPolicy` (no synchronization — the v0.3.0 head pop/push verbatim) is the only policy in M4.2, bound through `using ActivePolicy = SingleThreadedPolicy;`. `memory_pool_alloc` / `memory_pool_free` become one-line delegations to `alloc_skeleton<ActivePolicy>` / `free_skeleton<ActivePolicy>`. The policy and skeleton live in the **anonymous namespace** of [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp): they are an internal compile-time detail, so the public C ABI and the C++ wrapper are unchanged, and `struct memory_pool` gains no field (the ADR-0015 budget is untouched in M4.2). Behavior is **byte-identical to v0.3.0** — `SingleThreadedPolicy`'s static methods and the skeleton templates all inline away, leaving exactly the previous instruction sequence. Milestone 4.3 adds `MutexPolicy` / `LockFreePolicy` beside `SingleThreadedPolicy` and turns `ActivePolicy` into a `PBR_MEMORY_POOL_THREAD_SAFETY`-selected alias — **without touching the skeleton**.

## Alternatives Considered

- **Acquire/release (RAII critical-section guard) hooks** — `Policy::Guard g{pool}` around an inline pop/push in the skeleton. Rejected: a lock-free allocator is *not* a critical section; its correctness comes from a CAS retry loop, which an enter/leave guard cannot express. "Synchronized `pop_head`/`push_head`" operations *can* — each policy realizes them its own way (plain, locked, or CAS-loop). This is the pivotal choice.
- **Exhaustion check in the skeleton** (test `head_ == nullptr` before calling the hook). Rejected: it would be stale by the time a lock-free `pop_head` ran, and the lock-free loop must re-test anyway. The check belongs in the hook (§2).
- **Foreign-pointer guard inside the policy** (under the lock). Rejected: it reads only immutable fields, so taking the lock for it is pure waste, and it would bloat every policy with duplicated guard code. Keeping it in the skeleton is DRY and lock-free-for-foreign-pointers.
- **Full Strategy — the policy does everything, including the guards.** Rejected: the guards are invariant across policies; hoisting them into the skeleton (Template Method) keeps each policy down to the two lines that actually differ, and prevents three copies of the ADR-0012 check drifting apart.
- **Runtime Template Method (virtual hook methods).** Rejected by ADR-0020 — the zero-overhead single-threaded mandate forbids a virtual call on the hot path. Hooks are compile-time (static policy methods + template skeleton).
- **A separate skeleton per policy family** (one for lock-based, one for lock-free). Rejected: the two-hook contract (§1–§2) already unifies all three policies under one skeleton; the lock-free retry loop lives *inside* `pop_head`, so no second skeleton is needed.

## Consequences

**Positive**

- The single-threaded build is unchanged at the instruction level — the headline benchmark and the §2.4 "preserve the fast path" mandate hold by construction; the existing test suite (alloc/free/exhaustion/foreign-pointer) is the behavioral regression gate for the refactor.
- The two-hook contract is proven to fit all three ADR-0020 policies *before* the concurrent code is written, so M4.3 adds policies without reshaping the algorithm.
- The ADR-0012 guard is computed once, lock-free, for every policy — a small correctness-and-performance win that falls out of the Template Method split.
- No public surface, struct layout, or metadata-budget change in M4.2 — the riskiest part (concurrency) is isolated to M4.3 behind TSan (M4.4).

**Negative**

- The indirection (skeleton template + policy struct) is more machinery than a single inline function for the single-threaded case; it is justified only because M4.3/M4.4 need the seam. The compiler inlines it to nothing, so the cost is purely source-reading overhead, mitigated by this ADR.
- The `pop_head`/`push_head` contract bakes in an assumption that synchronization granularity is "one head mutation"; a future per-thread-cache policy (ADR-0020 §4, deferred) batches many mutations and may need a richer hook set — recorded here as the renegotiation point if that milestone lands.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0021.
- [`docs/patterns/README.md`](../patterns/README.md) — **Template Method** moves to *Adopted* (status `Implemented`).
- [`ROADMAP.md`](../../ROADMAP.md) §4.2 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M4.2)* entry.

[`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) lands `SingleThreadedPolicy`, the `alloc_skeleton` / `free_skeleton` templates, and the one-line delegations. No header, test, or build-config change — behavior is byte-identical, so the existing CTest suite is the gate.

## References

- E. Gamma et al., *Design Patterns* — Template Method (invariant skeleton + overridable steps) and Strategy (interchangeable algorithm), here composed at compile time.
- [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) §2–§3 — the three policies and the ABA-tagged lock-free loop the `pop_head` hook must accommodate.
- [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) — the foreign-pointer guard the skeleton keeps outside the synchronized region.
