---
id: BUG-0004
title: Unguarded size_t overflow in grow_pool growth-size computation
status: fixed
severity: low
reporter: third-party
discovered: 2026-06-15
affected-versions: ">=0.5.0,<1.1.2"
fixed-in: v1.1.2
---

# BUG-0004: Unguarded size_t overflow in grow_pool growth-size computation

## Summary

In the dynamic-growth slow path, `grow_pool` computed
`add = total * (grow_factor_ - 1)` **before** any overflow check, then validated
`add * block_size_` with `would_overflow_product`. The first multiplication could
itself wrap `size_t`, so the subsequent guard validated an already-wrapped `add`.

## Environment

- **Affected versions:** `>=0.5.0,<1.1.2` (dynamic growth landed in Milestone 5).
- **Configuration:** dynamic pool (`grow_factor_ >= 2`); `NONE` or `MUTEX`
  (`LOCKFREE` never grows, ADR-0024 §2, so `grow_pool` is not compiled there).

## Reproduction

Not reachable at runtime: `total` is the accumulated live block count, so
`total * (grow_factor_ - 1)` only overflows when `total` approaches
`SIZE_MAX / (grow_factor_ - 1)` — far more blocks than any machine can back. The
defect is found by inspection, not by a test that can actually allocate that much.

## Expected vs. actual

- **Expected:** every product that feeds an allocation size is overflow-checked
  before use, as `memory_pool_create` does for `block_size * block_count`.
- **Actual:** `total * (grow_factor_ - 1)` was computed unchecked; only the downstream
  `add * block_size_` product was guarded — against a possibly-wrapped `add`.

## Root cause

A missing overflow guard on the growth-count multiplication, inconsistent with the
meticulous overflow handling on the create path (ADR-0009 §3).

## Impact

Low / latent: RAM is exhausted long before `total` reaches the overflow boundary, so
in practice growth fails benignly first. It is a correctness/consistency gap, not a
reachable fault.

## Fix / workaround

Added `if (would_overflow_product(total, grow_factor_ - 1)) return false;` before
computing `add`, mirroring the create-path guard — on overflow the pool falls back to
fixed-mode exhaustion (returns `false`), exactly as for the existing guards. No test
is added because the condition is not runtime-reachable through the public API.

## References

- Fixing change: branch `fix/grow-pool-overflow-guard` — see the `CHANGELOG` `Fixed` entry.
- [`memory_pool.cpp`](../../../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) — `grow_pool`.
- [ADR-0009](../../../adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) (overflow guards) · [ADR-0022](../../../adr/0022-dynamic-growth-policy-and-chunk-linking.md) / [ADR-0024](../../../adr/0024-dynamic-growth-synchronization-and-creation-surface.md) (dynamic growth).
