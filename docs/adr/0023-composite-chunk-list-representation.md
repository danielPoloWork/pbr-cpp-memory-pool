# ADR-0023: Composite Chunk-List Representation

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) (the growth policy and chunk-linking strategy this ADR realises as a data structure), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1/§6 (the implicit free list now shared across chunks, and the struct field list this extends), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the range check that now walks the chunk list), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the per-pool budget the new `overflow_` head pressures, and the per-block-zero guarantee preserved), [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) (the MUTEX policy whose embedded `std::mutex` makes the budget tight), [ROADMAP](../../ROADMAP.md) §5.2 (this item) ← §5.1 (ADR-0022) → §5.3 (the growth slow path that populates the list).

## Context

[ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) settled the dynamic-growth *policy*: geometric sizing, an append-only forward-linked list of chunks, one shared implicit free list. Milestone 5.2 introduces the **Composite** data structure that policy needs — *before* the growth trigger and configuration surface (M5.3) — so the representation lands behavior-preserving and reviewable on its own. With no growth trigger yet, a pool in M5.2 always has exactly one chunk, and every operation must behave bit-for-bit as v0.4.0.

The design question is how to represent "the original pool is the first chunk, plus a list of overflow chunks" concretely, given two constraints measured on the existing code: `struct memory_pool` is **40 / 56 / 120 bytes** under `NONE` / `LOCKFREE` / `MUTEX`, and the ADR-0015 budget is **128 bytes** — so the MUTEX variant has only 8 bytes of headroom.

## Decision

### 1. The pool is the **first chunk inline**; overflow chunks are linked leaf nodes

`struct memory_pool` keeps its ADR-0009 §6 fields (`backing_`, `head_`, `block_size_`, `block_count_`, `alignment_`) — these *are* the pool's first chunk — and gains a single `Chunk* overflow_` head. Overflow chunks are separate descriptor nodes:

```cpp
struct Chunk {
    void* backing_;
    std::size_t block_count_;
    Chunk* next_;
};
```

The Composite is: the `memory_pool` root composes itself (the inline first chunk) with a forward-linked list of `Chunk` leaves. `overflow_` is `nullptr` in fixed mode — the only mode until M5.3 — so the pool is exactly one chunk and the list machinery is dormant.

**Why inline-first-chunk rather than a uniform list** (where even the first chunk is a separate `Chunk` and the struct holds only a `Chunk* chunks_` head): the uniform form would move `backing_`/`block_count_` out of the struct (changing ADR-0009 §6) and, more importantly, would make `memory_pool_metadata_bytes` either *under-report* (the struct shrinks 8 bytes while a 24-byte descriptor it doesn't count appears) or, if it counts the descriptor, **exceed the 128-byte budget under MUTEX** (112 + 24 = 136). The inline-first-chunk keeps `backing_`/`block_count_` in the struct, so `metadata_bytes` stays honest and simple, and adds only an 8-byte pointer — measured to land the MUTEX struct at **exactly 128** (≤ budget), so **M5.2 needs no budget renegotiation**. (M5.3's growth-config fields will need one — ADR-0015 §4 — as ADR-0022 anticipated.)

### 2. One shared implicit free list across all chunks

The `head_` and the in-slot next-links form a single logical free list whose links may point into any chunk. So `alloc` (pop head) and the `free` push stay **O(1)** regardless of chunk count. Only two operations gain a chunk walk:

- **`memory_pool_free`'s foreign-pointer check** (ADR-0012): `is_block_in_range` probes the inline first chunk, then each overflow chunk, via a shared `block_in_chunk` helper. O(chunks) = O(log N) in dynamic mode; O(1) in fixed mode (`overflow_` null → first chunk only).
- **`memory_pool_destroy`**: walks the overflow list, releasing each chunk's backing and descriptor, then the first backing. O(chunks).

### 3. Overflow descriptors are per-chunk metadata; per-block stays zero

Each overflow `Chunk` is a small separate allocation (`new Chunk` / `delete`). `memory_pool_metadata_bytes` returns `sizeof(memory_pool) + (overflow chunk count) × sizeof(Chunk)` — honest, O(chunks). In fixed mode the sum is zero, so it returns exactly `sizeof(memory_pool)`, **unchanged from v0.4.0**, and the M2.10 invariants (≤ budget; O(1) in `block_count`; equal for a 1024- vs 1,000,000-block pool) all still hold. The compile-time `static_assert` continues to gate the *fixed* part (`sizeof(memory_pool) <= 128`). Per-block overhead remains **zero** — descriptors are per-chunk, never per-block.

### 4. Behavior-preserving in M5.2

With `overflow_` always null this milestone, `create` / `alloc` / `free` / `destroy` / the range check / `metadata_bytes` all reduce to their v0.4.0 single-backing paths. The full test suite (including the M4 concurrent stress under MUTEX/LOCKFREE) passes unchanged — that is the regression gate. M5.3 adds the only new behavior: on exhaustion, allocate a `Chunk`, initialise its slots into the shared free list, and link it.

## Alternatives Considered

- **Uniform chunk list (first chunk also a separate `Chunk`; struct holds only a list head).** Rejected — purer Composite, but it changes ADR-0009 §6, and forces `metadata_bytes` to either under-report or bust the MUTEX budget (§1). The inline-first-chunk sidesteps both for the price of slightly non-uniform iteration (the first chunk is special-cased in two helpers).
- **Inline per-chunk descriptor (store the `Chunk` header at the start of each overflow chunk's backing).** Considered (ADR-0022 flagged it). Rejected for M5.2 — it saves the small separate allocations but complicates block alignment and the per-chunk block-count math (the header steals slot space), and the O(log N) descriptor count makes the separate-allocation overhead negligible anyway.
- **Per-chunk free lists (each chunk keeps its own list).** Rejected — `alloc` would then have to find a non-empty chunk (O(chunks) or extra bookkeeping), losing the O(1) pop. One shared list keeps `alloc` O(1); the cost lands only on the rarer `free`-validation and `destroy`.
- **Raise the ADR-0015 budget now (to e.g. 192) and use the uniform list.** Rejected for M5.2 — unnecessary churn when the inline form fits at 128. The renegotiation is deferred to M5.3, where the growth-config fields genuinely force it.

## Consequences

**Positive**

- The representation the dynamic-growth slow path needs is in place and reviewed in isolation, with zero behavior change (the regression gate is the existing suite).
- `alloc` and the `free` push stay O(1) across any number of chunks (shared free list); only the `free` *validation* and `destroy` scale with the small chunk count.
- `metadata_bytes` stays honest and the M2.10 invariants hold; per-block overhead stays zero (ADR-0015).
- ADR-0009 §6's struct fields are preserved; the change is one added pointer.

**Negative**

- Two helpers special-case the inline first chunk before walking `overflow_` — slightly less uniform than a pure list. Documented; the trade is honesty + budget fit.
- The MUTEX struct is now at exactly the 128-byte budget — **zero headroom**. M5.3's growth-config fields will exceed it and must renegotiate per ADR-0015 §4.
- `memory_pool_free`'s safety check is no longer strictly O(1) in dynamic mode (O(log N)); fixed mode is unaffected (ADR-0022 §3 trade-off, realised here).

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0023.
- [`docs/patterns/README.md`](../patterns/README.md) — **Composite** moves to *Adopted* (status `Implemented`).
- [`ROADMAP.md`](../../ROADMAP.md) §5.2 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M5.2)* entry.

[`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) lands `struct Chunk`, the `overflow_` head, the `block_in_chunk` / chunk-walking `is_block_in_range`, and the chunk-walking `destroy` / `metadata_bytes`. No header, test, or build-config change — behavior is byte-identical, so the existing CTest suite is the gate.

## References

- E. Gamma et al., *Design Patterns* — Composite (compose objects into tree/list structures so clients treat individual and composed objects uniformly); here the pool composes its inline first chunk with a list of overflow chunk leaves under one shared free list.
- [ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) §3 — the shared-free-list, append-only, O(log N)-chunk strategy this ADR represents.
- [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) — the per-pool budget (now at 128 under MUTEX) and the per-block-zero guarantee (preserved).
