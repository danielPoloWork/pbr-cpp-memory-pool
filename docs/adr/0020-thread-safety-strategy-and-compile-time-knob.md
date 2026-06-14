# ADR-0020: Thread-Safety Strategy and Compile-Time Configuration Knob

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** spec §2.4 (the optional, macro-configurable thread-safety requirement this ADR answers), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1/§6 (the implicit free list and the `struct memory_pool` field list the strategy mutates), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the 128-byte per-pool metadata budget a mutex member must fit), [ADR-0019](0019-free-list-diagnostic-iterator.md) §1 (the `PBR_MEMORY_POOL_*` compile-time-gate-macro precedent this knob follows), [ROADMAP](../../ROADMAP.md) §4.1 (this ADR's item) → §4.2 (the **Template Method** skeleton that hosts the strategy) → §4.3 (the implementation) → §4.4 (TSan + stress) → §4.5 (the comparative benchmark).

## Context

Spec §2.4: *"The pool must support concurrent access by multiple threads without memory corruption (optional, or configurable via a compile-time macro to maximize single-thread performance)."* — thread safety is **optional**, **macro-configurable**, and the **single-thread fast path must be preserved**.

The v0.3.0 hot path is a non-atomic LIFO on `pool->head_`: `alloc` pops the head, `free` pushes it (ADR-0009 §1). Two threads racing on `head_` corrupt the free list. Making that safe forces three coupled decisions, which this ADR settles before any code is written (the implementation is M4.2/M4.3):

1. **Which synchronization strategies** does the pool offer — and which does it actually implement now?
2. **How is the strategy selected** such that the single-threaded build pays *literally nothing* (no atomic, no branch, no indirect call)?
3. **What does choosing a strategy cost** in `struct memory_pool` layout (ADR-0015 budget) and in portability (lock-free), and what is explicitly deferred?

## Decision

### 1. The **Strategy** pattern, bound at compile time (policy-based), not at run time

Thread safety is modelled as the GoF **Strategy** pattern: the allocation/deallocation algorithm delegates its *synchronization* steps to an interchangeable policy. But the classical realization — a runtime `virtual` strategy object — is **rejected here** because a virtual call (or even a runtime `if (mode)` branch) on every `alloc`/`free` violates spec §2.4's "preserve the single-thread fast path." Instead the Strategy is bound **at compile time** (policy-based design / static Strategy): a policy *type* is selected once, and the pool's operations are written against that type so the single-threaded policy inlines to exactly the v0.3.0 instruction sequence with zero added cost.

The policy seam and the algorithm skeleton that calls it are the subject of M4.2 (**Template Method**); this ADR fixes only that the seam exists, is compile-time, and is a Strategy.

### 2. Three strategies; `PBR_MEMORY_POOL_THREAD_SAFETY` selects one at compile time

| Macro value | Policy | Mechanism | Use |
|-------------|--------|-----------|-----|
| `…_NONE` (default, 0) | `SingleThreadedPolicy` | No synchronization — the v0.3.0 path verbatim. | Single-threaded; maximum speed. |
| `…_MUTEX` (1) | `MutexPolicy` | A `std::mutex` (held across the O(1) head pop/push). | Portable, always-correct thread safety. |
| `…_LOCKFREE` (2) | `LockFreePolicy` | Treiber-stack CAS loop on an atomic, ABA-tagged head. | Low-contention scalability where the platform supports the required atomic. |

The knob mirrors the ADR-0019 gate-macro pattern: the default leaves the macro at `…_NONE`, an explicit definition (driven by a CMake option `PBR_MEMORY_POOL_THREAD_SAFETY=…`) wins, and the value is **fixed for the whole library at its build time** — thread safety is a property of the compiled `pbr_memory_pool`, not a per-pool or per-call flag (a per-pool flag would re-introduce the hot-path branch §2.4 forbids). A consumer needing thread safety rebuilds the library with the macro set; the default build is single-threaded with no overhead.

### 3. The lock-free strategy carries an ABA obligation, recorded now

A naïve Treiber stack on `std::atomic<void*> head_` is subject to **ABA**: thread A reads `head = X` and its successor `next`; before A's CAS, other threads pop `X`, pop more, and push `X` back, so A's `compare_exchange(X, next)` wrongly succeeds and corrupts the list. The `LockFreePolicy` therefore uses a **tagged head** — `{void* ptr; uintptr_t tag}` updated by a double-width CAS (`std::atomic<TaggedHead>`), the tag incremented on every pop so a recycled pointer never compares equal to a stale snapshot. On 64-bit targets this needs a 128-bit atomic (DWCAS); where the platform/toolchain cannot provide a lock-free 128-bit `compare_exchange`, `LockFreePolicy` is **not lock-free in fact** (the library degrades to a locked implementation of the same atomic) and that is documented at build time. Solving the DWCAS/availability mechanics is M4.3; this ADR fixes only that the lock-free path *must* defeat ABA and *may* be platform-conditional.

### 4. Per-thread caches are deferred, and the Strategy seam is why that is cheap

Per-thread (magazine / thread-local-cache) allocation — the tcmalloc/jemalloc approach — is the most scalable option but a substantially larger subsystem (thread-local storage lifetime, refill/flush batching, per-thread memory reserve accounting). It is **deferred**, not adopted, in Milestone 4. Crucially, it is an *additional* policy layered on the same Strategy seam, so adding it later (its own ADR) is non-breaking — which is itself a reason to choose Strategy over hard-coding a single locking scheme.

### 5. Metadata-budget interaction is flagged for M4.3

`MutexPolicy` adds a `std::mutex` to `struct memory_pool` (≈ 40 bytes on Linux, ≈ 80 on Windows); `LockFreePolicy` adds a 16-byte tagged head in place of the plain `void* head_`. The ADR-0015 per-pool budget is 128 bytes and the base struct is 40, so `…_MUTEX` is tight on Windows (≈ 120 bytes) but within budget; `…_LOCKFREE` is comfortably within. M4.3 must keep the `static_assert(sizeof(memory_pool) <= 128)` green in every configuration or renegotiate the budget per ADR-0015 §4. Recorded here so the implementation does not discover it late.

## Alternatives Considered

- **Runtime (virtual) Strategy — a polymorphic `SyncPolicy*` in each pool.** Rejected. The virtual call on every `alloc`/`free`, plus the per-pool pointer, violates spec §2.4's zero-overhead single-thread mandate. Compile-time binding gives the same interchangeability with none of the cost.
- **Per-pool runtime thread-safety flag (a `bool` in `struct memory_pool`).** Rejected for the same reason — it puts a branch on the hot path even for single-threaded pools, and the spec explicitly asks for a *compile-time macro*.
- **Always-on locking (no strategy, a `std::mutex` unconditionally).** Rejected. It is correct but taxes the single-threaded common case (the project's headline benchmark is single-threaded), directly contradicting §2.4. Optionality is a hard requirement.
- **Lock-free only (drop the mutex policy).** Rejected. Lock-free-with-ABA-defeat depends on a lock-free 128-bit CAS that not every Tier-1 target guarantees; the `MutexPolicy` is the always-correct, always-portable fallback and the conservative recommendation for most consumers.
- **Mutex only (drop the lock-free policy).** Rejected. It would forgo the didactic and practical value of demonstrating a correct Treiber-stack allocator — a core reason this reference project exists — and the comparative benchmark (M4.5) needs both to be meaningful.
- **Per-thread caches in Milestone 4.** Deferred (see §4) — out of scope for "make it correct and configurable"; the seam keeps it a future, non-breaking addition.
- **Naïve (untagged) lock-free Treiber stack.** Rejected — ABA-unsafe for a recycling fixed-block pool; correctness is non-negotiable (§2.4 "without memory corruption").

## Consequences

**Positive**

- The single-threaded build is byte-for-byte the v0.3.0 hot path: the `SingleThreadedPolicy` inlines away, so the headline benchmark and the "preserve the fast path" mandate are honoured by construction.
- Three clearly-separated policies give consumers a correctness/portability/scalability choice via one macro, and the comparative benchmark (M4.5) has well-defined arms.
- The Strategy seam makes per-thread caches (and any future policy) a non-breaking later addition — the pattern earns its place rather than being decoration.
- Sets up M4.2's **Template Method** cleanly: the skeleton (validate → sync-acquire → pop/push → sync-release) has exactly the hook points the policies fill.

**Negative**

- Compile-time selection means thread safety is a whole-library build property; a consumer that wants both a fast single-threaded pool and a thread-safe pool in the *same* program must build (and name) two library configurations. This is the price §2.4's "compile-time macro" wording asks for, and is documented.
- `MutexPolicy` pressures the ADR-0015 128-byte budget on Windows; M4.3 carries the obligation to keep the `static_assert` green or renegotiate.
- The lock-free policy's lock-freedom is platform-conditional (DWCAS availability); the library must report honestly when it degrades to a locked implementation.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0020.
- [`docs/patterns/README.md`](../patterns/README.md) — **Strategy** moves to *Adopted / Planned* (status `Planned`; implementation lands in M4.2/M4.3).
- [`ROADMAP.md`](../../ROADMAP.md) §4.1 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M4.1)* entry.

No source changes land in this PR — M4.1 is the decision; the policy classes, the `PBR_MEMORY_POOL_THREAD_SAFETY` macro, and the CMake option are implemented in M4.2 (skeleton) and M4.3 (policies).

## References

- spec §2.4 — the optional, macro-configurable thread-safety requirement.
- R. K. Treiber, *Systems Programming: Coping with Parallelism* (1986) — the lock-free stack the `LockFreePolicy` is based on.
- ISO C++17 [atomics] — `std::atomic::compare_exchange_weak`, lock-free property, and the double-width-CAS requirement for the ABA-tagged head.
- A. Alexandrescu, *Modern C++ Design* — policy-based design, the compile-time realization of Strategy this ADR adopts.
- [ADR-0019](0019-free-list-diagnostic-iterator.md) §1 — the compile-time gate-macro mechanism `PBR_MEMORY_POOL_THREAD_SAFETY` mirrors.
