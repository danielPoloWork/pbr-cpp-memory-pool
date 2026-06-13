# ADR-0025: Decorator for an Instrumented Pool Variant

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) (the `Pool` this decorates), [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 (the `allocate` / `try_allocate` verbs the decorator forwards and instruments), [ADR-0017](0017-typed-pool-design.md) / [ADR-0018](0018-stl-allocator-adapter.md) (the prior compose-over-`Pool` header-only types this follows), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the metadata accessor the decorator passes through), [ROADMAP](../../ROADMAP.md) §6.1 (this item) → §6.2 (the **Observer** for lifecycle events) → §6.3 (the zero-overhead-when-disabled verification).

## Context

Milestone 6 adds optional observability "without touching the hot path of release builds" (ROADMAP §6 goal). Item 6.1 is the **Decorator**: an instrumented pool variant that tracks allocation statistics (counters, occupancy, failures) on top of a plain `Pool`. Three questions shape it:

1. **What form does the Decorator take**, given `Pool` is a concrete, move-only RAII type with no virtual interface?
2. **What does it track** — the roadmap lists "counters, allocation histogram, optional logging"?
3. **How is "zero overhead when disabled" achieved** (the §6.3 contract)?

## Decision

### 1. Decorator by composition, header-only, wrapping `Pool`

`it::d4np::memorypool::InstrumentedPool` (new header [`instrumented_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp)) **owns a `Pool` by composition** and re-exposes its surface — `allocate` / `try_allocate` / `deallocate`, plus the `native_handle` / `block_size` / `metadata_bytes` pass-throughs — wrapping each allocation verb with counter updates before/after forwarding. This is the **Decorator** pattern in its idiomatic-C++ (composition) form: the classic GoF Decorator shares an abstract interface via inheritance, but `Pool` is a concrete, move-only value type with no virtual surface (ADR-0010), so a wrapping value type that mirrors the surface is the correct realisation — exactly as `TypedPool` (ADR-0017) and `PoolAllocator` (ADR-0018) compose over `Pool` rather than inheriting. Header-only: zero object code, zero per-pool metadata in the library.

It adopts a pool by move (`explicit InstrumentedPool(Pool&&)`) and offers `static make(block_size, block_count)` / `make_dynamic(block_size, block_count, growth_factor)` factories mirroring `Pool` — so it decorates fixed *and* dynamic pools uniformly.

### 2. Counters are atomic; the type stays movable via a hand-written move

The instrumentation state is five `std::atomic<std::size_t>` counters, updated with `relaxed` ordering (statistics need atomicity, not inter-thread ordering):

| Counter | Meaning |
|---------|---------|
| `allocations_` | successful allocations |
| `deallocations_` | `deallocate` calls with a non-null block |
| `allocation_failures_` | `allocate`/`try_allocate` that found the pool exhausted |
| `live_` | currently outstanding blocks (incremented on alloc, decremented on dealloc) |
| `peak_live_` | high-water mark of `live_`, updated by a relaxed compare-exchange max on each allocation |

Atomic counters make the decorator **safe to wrap a thread-safe (`MUTEX`/`LOCKFREE`) pool and drive it concurrently**; under contention the `live_`/`peak_live_` pair is an approximate (eventually-consistent) high-water mark, which is the right fidelity for a diagnostic and is documented as such. `std::atomic` is not movable, so `InstrumentedPool` provides a **hand-written move constructor / move-assignment** that `load()`s each counter and re-seeds the moved-to atomics (copy stays deleted, like `Pool`) — keeping the type movable so the `make` factories can return it by value.

### 3. No allocation histogram — peak occupancy is the fixed-block substitute

A size histogram is **degenerate for a fixed-block pool**: every allocation is exactly `block_size`, so the histogram collapses to a single bucket. The meaningful occupancy signal is therefore `peak_live_` — the high-water mark of outstanding blocks — which is precisely what capacity planning needs ("how many blocks did this pool ever need at once?"). A true multi-bucket histogram (e.g. of occupancy-over-time) is recorded as a rejected alternative; it adds bucketing machinery for little value here.

### 4. "Optional logging" is an on-demand summary; event-stream logging is the Observer (M6.2)

The decorator exposes a copyable `PoolStats` snapshot via `stats()` and a `write_summary(std::ostream&)` for on-demand logging, rather than a per-operation logging callback. A per-event hook on every `alloc`/`free` would add a branch (and a `std::function`) to the instrumented path and overlaps squarely with the **Observer** pattern that M6.2 adds for lifecycle events (exhaustion, growth, destruction). Keeping M6.1 to *state* (counters) and M6.2 to *events* (notifications) avoids duplicating the event surface.

### 5. Zero overhead when disabled = opt-in by type

"Instrumentation disabled" means *using `Pool` directly* — `InstrumentedPool` is a separate type a caller opts into by wrapping. A program that never wraps pays exactly nothing: `Pool`'s hot path is untouched, no counter, no branch, no atomic. This is cleaner than a compile-time macro gate (as used for diagnostics, ADR-0019) because the Decorator's whole nature is opt-in composition; M6.3 verifies the plain-`Pool` path is byte-identical and that overhead lives only inside `InstrumentedPool`.

## Alternatives Considered

- **GoF virtual-interface Decorator** (`IPool` abstract base, `Pool` and `InstrumentedPool` both derive). Rejected — it would force a vtable and a heap-/indirection-based surface onto the hot path of *every* pool, including undecorated ones, to serve an opt-in diagnostic. Composition over the concrete `Pool` keeps the common path zero-cost (consistent with ADR-0017/0018).
- **Template Decorator `InstrumentedPool<PoolLike>`** decorating `Pool` or `TypedPool` generically. Deferred — the generality isn't needed yet, and a concrete wrapper over `Pool` is simpler; `TypedPool` instrumentation can be added later by the same pattern.
- **Non-atomic counters.** Rejected — the project ships thread-safe pools (ADR-0020); a decorator with plain counters would data-race when wrapping one used concurrently. Relaxed atomics are cheap and make the decorator universally safe.
- **Compile-time-gated instrumentation (a `PBR_MEMORY_POOL_INSTRUMENT` macro like the diagnostics gate).** Rejected — the Decorator is naturally opt-in by *type*; a separate wrapper is more flexible (per-pool, not per-build) and needs no gate to be zero-cost when unused.
- **Per-operation `std::function` logging callback.** Deferred to M6.2 — event notification is the Observer's job; a per-op callback here would duplicate it and tax the instrumented path.
- **Allocation-size histogram.** Rejected — degenerate for a fixed-block pool (§3); `peak_live_` is the useful occupancy statistic.

## Consequences

**Positive**

- Opt-in instrumentation with **zero cost to undecorated pools** — the §6.3 contract holds by construction.
- Atomic counters make the decorator safe over any thread-safety policy; the hand-written move keeps it factory-returnable.
- `peak_live_` gives the capacity-planning signal a fixed-block pool actually needs; `PoolStats` + `write_summary` cover on-demand logging.
- Composition over `Pool` keeps the type header-only and consistent with `TypedPool` / `PoolAllocator`.

**Negative**

- Under heavy concurrent use the `live_`/`peak_live_` high-water mark is approximate (the counters are individually atomic but not jointly snapshotted) — acceptable and documented for a diagnostic.
- The Decorator wraps `Pool`, not `TypedPool`; instrumenting a typed pool needs the deferred template generalisation.
- `deallocations_` counts `deallocate` *calls* with a non-null block, not necessarily *successful* frees (the pool silently no-ops foreign pointers — ADR-0012), so a misbehaving caller can skew it; documented.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0025.
- [`docs/patterns/README.md`](../patterns/README.md) — **Decorator** moves to *Adopted* (status `Implemented`).
- [`ROADMAP.md`](../../ROADMAP.md) §6.1 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M6.1)* entry.

[`instrumented_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp) lands `InstrumentedPool` + `PoolStats`; [`instrumented_pool_test.cpp`](../../src/test/cpp/it/d4np/memorypool/instrumented_pool_test.cpp) lands its dedicated doctest binary (CTest `instrumented_pool`). The zero-overhead verification is M6.3.

## References

- E. Gamma et al., *Design Patterns* — Decorator (attach responsibilities to an object dynamically); here realised by composition over a concrete value type.
- [ADR-0017](0017-typed-pool-design.md) §1 / [ADR-0018](0018-stl-allocator-adapter.md) §1 — the compose-over-`Pool`, header-only precedent.
- ISO C++17 [atomics] — `std::atomic` relaxed RMW and the compare-exchange max idiom for `peak_live_`.
