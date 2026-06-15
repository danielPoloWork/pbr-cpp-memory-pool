---
id: BUG-0001
title: Data race on InstrumentedPool::last_growths_ under concurrent use
status: fixed
severity: high
reporter: third-party
discovered: 2026-06-15
affected-versions: ">=0.6.0,<1.1.2"
fixed-in: v1.1.2
---

# BUG-0001: Data race on InstrumentedPool::last_growths_ under concurrent use

## Summary

`InstrumentedPool::notify_if_grew()` read **and wrote** the non-atomic member
`last_growths_` on the hot allocation path, while the class contract explicitly
permits driving the decorator concurrently over a thread-safe pool — a data race
(C++ memory-model UB).

## Environment

- **Affected versions:** `>=0.6.0,<1.1.2` (`InstrumentedPool` landed in Milestone 6).
- **Configuration:** `PBR_MEMORY_POOL_THREAD_SAFETY = MUTEX` **and** a dynamic
  (growing) pool, driven from multiple threads. Not reachable under `LOCKFREE`
  (dynamic growth is disabled there, ADR-0024 §2, so the growth counter stays 0)
  nor under `NONE` (single-threaded by contract).

## Reproduction

Wrap a dynamic `MUTEX` pool in an `InstrumentedPool` and allocate from several
threads so the pool grows repeatedly. `notify_if_grew()` — called from
`allocate()` / `try_allocate()` — does `if (growths > last_growths_) { last_growths_ = growths; … }`
on a plain `std::size_t`, concurrently. ThreadSanitizer flags the race; the
pre-fix `concurrency_stress_test` did not exercise `InstrumentedPool` at all.

## Expected vs. actual

- **Expected:** the contract says the decorator "is safe to wrap a thread-safe
  (`MUTEX`/`LOCKFREE`) pool and drive it concurrently".
- **Actual:** concurrent allocations race on `last_growths_` — undefined behaviour
  (a data race is UB regardless of the "approximate metrics" caveat, which only
  excuses the *value*, not the race).

## Root cause

`last_growths_` was a plain `std::size_t` while every other counter was a
`std::atomic`. The growth-detection read-modify-write was therefore unsynchronized.

## Impact

UB in a Tier-1 supported configuration (`MUTEX` + dynamic growth + concurrency +
instrumentation). High severity: the unsafe access is on the allocation hot path.

## Fix / workaround

Made `last_growths_` a `std::atomic<std::size_t>`; `notify_if_grew()` now advances
it with a relaxed `compare_exchange_weak`, so the winning thread notifies `grew`
exactly once per observed growth and concurrent callers neither race nor
double-notify. The move ctor/assignment load/store it atomically. Added an
`InstrumentedPool`-over-dynamic-pool concurrency case to `concurrency_stress_test`
(run under the MUTEX ThreadSanitizer CI job).

## References

- Fixing change: branch `fix/instrumented-pool-correctness` — see the `CHANGELOG` `Fixed` entry.
- [`instrumented_pool.hpp`](../../../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp) — `notify_if_grew`, `last_growths_`.
- [ADR-0025](../../../adr/0025-decorator-for-instrumented-pool.md) (Decorator) · [ADR-0026](../../../adr/0026-observer-for-pool-lifecycle-events.md) (Observer / growth event) · [ADR-0024](../../../adr/0024-dynamic-growth-synchronization-and-creation-surface.md) (LOCKFREE never grows).
