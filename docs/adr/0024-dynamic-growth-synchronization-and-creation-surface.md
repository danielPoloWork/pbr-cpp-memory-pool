# ADR-0024: Dynamic-Growth Synchronization, Creation Surface, and the Lock-Free Deferral

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) (the growth policy this implements), [ADR-0023](0023-composite-chunk-list-representation.md) (the chunk list growth populates), [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) (the policies whose synchronization growth piggybacks on, and the lock-free policy this defers growth for), [ADR-0021](0021-template-method-allocation-skeleton.md) §2 (the `pop_head` hook where growth lives), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) §4 (the budget renegotiation this performs), spec §2.2 / §5 (the optional growth requirement and the frozen C signatures), [ROADMAP](../../ROADMAP.md) §5.3 (this item) → §5.4 (the exhaustion-and-grow tests/benchmarks).

## Context

[ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) chose geometric growth over an append-only chunk list with a shared free list; [ADR-0023](0023-composite-chunk-list-representation.md) landed the dormant Composite representation. Milestone 5.3 turns growth on. Implementing it forces three decisions ADR-0022 left to "M5.3":

1. **Where and how is growth synchronized?** Growth mutates shared state (the free-list head and the chunk list) — under the thread-safe policies it must be serialized, and the lock-free case is hard.
2. **What is the creation surface?** The spec §5 signatures are frozen (ADR-0009); enabling dynamic mode needs a new entry point.
3. **The budget.** Adding the growth factor to `struct memory_pool` pushes the MUTEX variant over the ADR-0015 128-byte budget (it is already at exactly 128 after ADR-0023).

## Decision

### 1. Growth runs inside `pop_head`, under the policy's own synchronization

The exhaustion point is `Policy::pop_head` returning empty (ADR-0021 §2). Growth therefore lives **inside `pop_head`**, so it is covered by whatever synchronization that policy already holds — no new lock for the common cases:

- **`SingleThreadedPolicy`** (NONE): on an empty head, if the pool is dynamic, call `grow_pool` (plain) and pop.
- **`MutexPolicy`** (MUTEX): `pop_head` already holds the pool mutex across the head read-modify-write; `grow_pool` runs **under that same lock**, so chunk-list append and free-list seeding are serialized for free.

`grow_pool` is `noexcept`: it allocates a chunk's backing (over-aligned `::operator new`) and a `Chunk` descriptor (`new Chunk`), and on `std::bad_alloc` from either it releases what it got and returns `false` — the caller then falls back to the fixed-mode **failure** semantics (`NULL` in C, `std::bad_alloc` from `Pool::allocate`). Growth being a slow-path event, the `try`/`catch` cost is irrelevant.

The new chunk's `block_count` is `current_total × (growth_factor − 1)` (so the total multiplies by the factor — ADR-0022 §2), where `current_total` is computed by walking the chunk list (O(chunks), only at the rare growth event); the ADR-0009 §3 `size_t`-overflow guard applies to the new chunk's byte size. The chunk's slots are initialised into the (currently empty) shared free list and the chunk is linked at the head of `overflow_`.

### 2. Lock-free dynamic growth is **deferred**; `LOCKFREE` + dynamic is rejected at creation

`LockFreePolicy` holds no lock, and safe concurrent growth there is a substantial problem: `memory_pool_free`'s foreign-pointer check walks the chunk list, which would race a concurrent append unless the chunk links become atomic (acquire/release) **and** growers coordinate through a dedicated grow-lock. That is a research-grade addition, cannot be validated by the existing TSan job (LOCKFREE is excluded from it — ADR-0020/M4.4), and is out of Milestone 5's scope — the same judgement that deferred per-thread caches (ADR-0020 §4).

Therefore, in a `LOCKFREE`-built library, **`memory_pool_create_dynamic` returns `NULL`** (and `Pool::make_dynamic` / `PoolBuilder` with a growth factor return `std::nullopt`). The contract is explicit: a caller gets a clear failure, never a pool that silently never grows. Fixed-mode lock-free pools — the common lock-free use — are fully supported. Full lock-free dynamic growth is a candidate for a future milestone.

### 3. Creation surface: a new C function + a `Pool::make_dynamic` / `PoolBuilder` knob

The frozen spec §5 `memory_pool_create(block_size, block_count)` stays fixed-mode (it sets `grow_factor_ = 0`). Dynamic mode gets a new, additive, ANSI-C-C89-compatible function:

```c
memory_pool_t* memory_pool_create_dynamic(size_t block_size, size_t block_count, size_t growth_factor);
```

`growth_factor < 2` is rejected (`NULL`) — a factor must actually grow. The function reuses `memory_pool_create` for the first chunk, then sets `grow_factor_`. On the C++ side, `static std::optional<Pool> Pool::make_dynamic(block_size, block_count, growth_factor)` mirrors `Pool::make`, and `PoolBuilder` gains `with_growth_factor(std::size_t)` (its const `build()` routes to the dynamic path when a factor ≥ 2 is set, per ADR-0011's "Builder absorbs growth knobs" intent). The dynamic flag is a **runtime, per-pool** value (ADR-0022 §1) — `struct memory_pool` carries one `std::size_t grow_factor_` (0 = fixed).

### 4. Budget renegotiated to 192 bytes (ADR-0015 §4)

`grow_factor_` adds 8 bytes; the MUTEX `struct memory_pool` (exactly 128 after ADR-0023) becomes 136, over the ADR-0015 128-byte budget. Per the ADR-0015 §4 renegotiation protocol — and as ADR-0022/ADR-0023 anticipated — the per-pool budget is raised to **192 bytes**: NONE 56, LOCKFREE 72, MUTEX 136, all comfortably under, with headroom for future per-pool fields. The compile-time `static_assert` and the runtime budget test move to 192 in lockstep. Per-block overhead remains **zero**; the O(chunks) overflow descriptors are unchanged (ADR-0023).

## Alternatives Considered

- **A dedicated grow-lock for all policies (full lock-free dynamic growth).** Rejected for M5.3 — it makes the chunk-list links atomic and adds a grow mutex (bloating every pool and the budget), and its correctness can't be TSan-verified given the LOCKFREE/TSan exclusion. Deferred, not refused forever.
- **Growth in `alloc_skeleton` (outside the policy) instead of `pop_head`.** Rejected — it would mutate the head/chunk list outside the policy's lock, racing concurrent ops. Keeping growth inside `pop_head` reuses the policy's synchronization (ADR-0021 §2's reason for putting the exhaustion test there).
- **Silently treat `LOCKFREE` + dynamic as fixed (never grow, no error).** Rejected — a pool that silently ignores its growth configuration is a latent surprise. An explicit `NULL`/`nullopt` is the honest contract.
- **Overload `memory_pool_create` with a third parameter / a config struct.** Rejected — spec §5 froze the two-argument signature (ADR-0009); a new named function is additive and keeps the frozen one intact.
- **`grow_factor_` as a smaller integer (e.g. `uint16_t`) to dodge the budget bump.** Rejected — alignment padding would eat the saving, and 192 is the right honest budget for the Composite + thread-safety + growth combination; under-sizing the field to avoid a documented renegotiation is false economy.

## Consequences

**Positive**

- Dynamic growth works under the NONE and MUTEX policies with no new lock — growth reuses the policy's existing synchronization, and the slow-path `try`/`catch` is free in steady state.
- The fixed-mode default and all fixed pools (any policy) are unchanged; dynamic mode is additive and opt-in.
- The LOCKFREE + dynamic contract is explicit (`NULL`/`nullopt`), not a silent misbehaviour.
- The budget renegotiation (192) is the documented ADR-0015 §4 path, with headroom.

**Negative**

- `LOCKFREE` + dynamic is unsupported this milestone — a real, documented gap. Mitigated by full fixed-mode lock-free support and a clear failure contract; full support is a future-milestone candidate.
- A dynamic pool's per-pool metadata is O(chunks) (the overflow descriptors) and `free`'s safety check is O(log N) — the ADR-0022 §3 trade-offs, now live.
- Growth allocates on the exhaustion path, so a dynamic `alloc` can occasionally incur a chunk allocation (amortized O(1) per ADR-0022, but not worst-case O(1)).

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0024.
- [`ROADMAP.md`](../../ROADMAP.md) §5.3 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M5.3)* entry.

[`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) lands `grow_factor_`, `grow_pool`, the `pop_head` growth (NONE/MUTEX), `memory_pool_create_dynamic`, `Pool::make_dynamic`, the `PoolBuilder` knob, and the 192-byte budget. [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) / [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) declare the new surface; [`c_consumer_min.c`](../../src/test/c/it/d4np/memorypool/c_consumer_min.c) exercises it under C89/C99. Comprehensive exhaustion-and-grow tests and benchmarks are M5.4.

## References

- spec §2.2 — the optional dynamic-growth requirement; §5 — the frozen C signatures.
- [ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) §2/§3 — geometric sizing and the shared-free-list chunk list.
- [ADR-0021](0021-template-method-allocation-skeleton.md) §2 — why the exhaustion (and now growth) decision lives inside `pop_head`.
- [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) §4 — the budget renegotiation protocol exercised here (128 → 192).
- R. K. Treiber (1986); the chunk-list-append-vs-concurrent-read hazard that motivates the lock-free deferral.
