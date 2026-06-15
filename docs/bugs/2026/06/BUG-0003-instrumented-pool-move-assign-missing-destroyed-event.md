---
id: BUG-0003
title: InstrumentedPool move-assignment does not notify destroyed for the replaced pool
status: fixed
severity: low
reporter: third-party
discovered: 2026-06-15
affected-versions: ">=0.6.0,<1.1.2"
fixed-in: v1.1.2
---

# BUG-0003: InstrumentedPool move-assignment does not notify destroyed for the replaced pool

## Summary

`InstrumentedPool::operator=(InstrumentedPool&&)` released the destination's pool
and overwrote its observer list without emitting `PoolEvent::destroyed` for the
pool being replaced. Observers attached to the destination's prior contents
silently lost their subject — asymmetric with the destructor, which does notify.

## Environment

- **Affected versions:** `>=0.6.0,<1.1.2`.
- **Configuration:** any; triggered by move-assigning onto an `InstrumentedPool`
  that already has registered observers.

## Reproduction

```cpp
RecordingObserver obs;
InstrumentedPool dest{Pool(64, 4)};
dest.add_observer(obs);
dest = InstrumentedPool{Pool(64, 4)};  // replaces dest's pool
// before the fix: obs.destroyed_ == 0 (the replaced pool went away silently)
```

## Expected vs. actual

- **Expected:** the pool being replaced is going away, so its observers should see
  `destroyed` — the same guarantee the destructor gives.
- **Actual:** no `destroyed` event was delivered on move-assignment.

## Root cause

`operator=` overwrote `pool_` and `observers_` without first notifying the existing
observers. Not a leak (the moved-into `Pool` is properly released), but a
lifecycle-event asymmetry.

## Impact

Low / cosmetic: observers relying on a balanced `destroyed` per subject miss one on
move-assignment. No memory-safety consequence.

## Fix / workaround

`operator=` now calls `notify(PoolEvent::destroyed)` at the top of the
self-assignment-guarded block, before the pool and observer list are overwritten —
symmetric with the destructor. Covered by a new move-assignment observer test.

## References

- Fixing change: branch `fix/instrumented-pool-correctness` — see the `CHANGELOG` `Fixed` entry.
- [`instrumented_pool.hpp`](../../../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp) — `operator=`, `~InstrumentedPool`.
- [ADR-0026](../../../adr/0026-observer-for-pool-lifecycle-events.md) (Observer lifecycle events).
