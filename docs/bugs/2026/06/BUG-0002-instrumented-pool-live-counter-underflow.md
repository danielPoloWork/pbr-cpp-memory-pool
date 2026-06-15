---
id: BUG-0002
title: InstrumentedPool::deallocate underflows live_ on a foreign or double-freed pointer
status: fixed
severity: medium
reporter: third-party
discovered: 2026-06-15
affected-versions: ">=0.6.0,<1.1.2"
fixed-in: v1.1.2
---

# BUG-0002: InstrumentedPool::deallocate underflows live_ on a foreign or double-freed pointer

## Summary

`InstrumentedPool::deallocate()` unconditionally decremented the unsigned `live_`
counter for any non-null pointer, even though the core `memory_pool_free` silently
ignores foreign / misaligned pointers (ADR-0012). A foreign pointer (or a
double-free) therefore drove `live_` below zero, **wrapping to `SIZE_MAX`** and
corrupting `stats()` / `write_summary()`.

## Environment

- **Affected versions:** `>=0.6.0,<1.1.2`.
- **Configuration:** any; triggered by a caller passing a non-null pointer that the
  core treats as a no-op (out-of-range / misaligned / already-freed).

## Reproduction

```cpp
InstrumentedPool pool{Pool(64, 4)};
void* a = pool.try_allocate();
pool.deallocate(a);          // live_ -> 0
int stack_var = 0;
pool.deallocate(&stack_var); // foreign: core no-op, but live_ decremented -> SIZE_MAX
// pool.stats().live_ == SIZE_MAX
```

## Expected vs. actual

- **Expected:** `live_` reflects outstanding blocks and never underflows; a free the
  core rejects must not corrupt the diagnostic.
- **Actual:** `live_` underflowed to `SIZE_MAX`; the summary reported absurd values.

## Root cause

The counter update was gated only on `block != nullptr`, not on whether the core
actually accepted the free — and the core gives no acceptance signal. `live_` being
`std::size_t`, the over-decrement wrapped.

Related documentation defect: the C header (`memory_pool.h`) claimed the Decorator
*addresses* double-free. It does not — it counts deallocations but cannot
distinguish a double-free from a legitimate free. The header wording was corrected
in the same change (the chosen scope is to fix the counter and tell the truth, not
to implement double-free detection — that would be a separate feature).

## Impact

Corrupted diagnostics (not memory corruption — the core stays safe). Medium: it
surfaces only on caller misuse, but the wrong-by-`SIZE_MAX` counter is badly
misleading, and the header over-promised.

## Fix / workaround

`deallocate()` now decrements `live_` with a clamp-at-zero `compare_exchange_weak`
loop, so a rejected/foreign/double free can never underflow it. `deallocations_`
still counts every non-null call (its documented meaning). The header's double-free
note was rewritten to stop promising Decorator detection.

## References

- Fixing change: branch `fix/instrumented-pool-correctness` — see the `CHANGELOG` `Fixed` entry.
- [`instrumented_pool.hpp`](../../../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp) — `deallocate`.
- [`memory_pool.h`](../../../../src/main/cpp/it/d4np/memorypool/memory_pool.h) — `memory_pool_free` double-free note.
- [ADR-0012](../../../adr/0012-foreign-pointer-and-out-of-range-pointer-policy.md) (foreign-pointer no-op) · [ADR-0025](../../../adr/0025-decorator-for-instrumented-pool.md) (Decorator).
