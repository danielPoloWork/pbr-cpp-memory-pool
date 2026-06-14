# ADR-0022: Dynamic-Growth Policy and Chunk-Linking Strategy

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** spec §2.2 ("request a new contiguous block if configured in dynamic mode" — the optional dynamic-growth requirement this ADR answers), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1/§6 (the single contiguous backing and the implicit free list this ADR generalises to multiple chunks), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the `O(1)` range check whose cost this ADR changes in dynamic mode), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the per-pool budget the chunk-list head grows, and the per-block-zero guarantee this ADR preserves), [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) (the *compile-time* knob this ADR deliberately does **not** mirror), [ROADMAP](../../ROADMAP.md) §5.1 (this item) → §5.2 (the **Composite** chunk-list representation) → §5.3 (the implementation behind the flag) → §5.4 (tests + benchmarks).

## Context

Spec §2.2 makes dynamic growth **optional**: when a pool is exhausted, *if configured in dynamic mode*, it acquires a new contiguous chunk instead of failing. The default is fixed-size (the v0.4.0 behaviour — `alloc` returns `NULL` / throws on exhaustion). Milestone 5 adds the dynamic mode; this ADR fixes the two design decisions it rests on, before any code (M5.2 = the Composite representation, M5.3 = the implementation):

1. **How much does the pool grow each time** — geometric or linear?
2. **How are the chunks linked**, and what does that cost the existing O(1) alloc/free and the ADR-0012 safety check?

A third question — *how is growth configured* — is answered at the decision level here and implemented in M5.3.

## Decision

### 1. Growth is opt-in, **runtime**, per-pool — not a compile-time knob

Unlike thread safety (ADR-0020), which is a whole-library *performance* property selected at compile time because a runtime branch would tax **every** allocation, dynamic growth is a per-pool **behavioural** choice and its decision point is the **exhaustion path** — the rare slow path, not the steady-state hot path. A pool that is not exhausted never evaluates the growth branch. Therefore growth is a runtime, per-pool flag (set at creation, default off), so different pools in one program can choose differently, and the steady-state O(1) `alloc` / `free` are untouched. The fixed-mode default keeps the v0.4.0 behaviour bit-for-bit. The C creation surface for enabling it (the spec §5 signatures are frozen, ADR-0009) is decided in M5.3; the C++ `PoolBuilder` gains a `with_growth(...)` setter.

### 2. **Geometric** growth (configurable factor, default ×2) — linear is rejected

On exhaustion a dynamic pool acquires a new contiguous chunk so the total capacity multiplies by a growth factor `F` (default 2): the new chunk supplies `current_total_block_count × (F − 1)` blocks (for `F = 2`, the new chunk equals the current total — classic doubling). `F > 1` is required; the `size_t`-overflow guard from ADR-0009 §3 applies to the new chunk's byte size (M5.3).

The decisive reason is **not** the amortized-O(1) argument familiar from `std::vector` (though it holds) — it is the **chunk count**. The chunk count directly bounds two O(chunks) operations introduced by multi-chunk pools (see §3): the ADR-0012 foreign-pointer check on `free`, and `destroy`. Geometric growth keeps the chunk count at **O(log N)** in the total block count; **linear** growth (each chunk a fixed size) makes it **O(N / chunk_size)** — linear in capacity — which would degrade `free`'s safety check and `destroy` to O(N). Geometric is therefore chosen to protect the pool's performance contract, and linear is rejected for breaking it.

### 3. One shared implicit free list across a **singly-linked list of chunks**

Chunks are linked in a singly-linked list of per-chunk descriptors `{backing, block_count, next}`; the original pool is the first chunk. The **implicit free list is a single logical list threaded across all chunks** — a freed block's next-link may point into any chunk — so:

- `alloc` is still **O(1)**: pop the shared free-list head. On a `NULL` head, a *fixed* pool returns failure; a *dynamic* pool runs the growth slow path (allocate + initialise a chunk, link it, then pop).
- `free` is still **O(1)** for the actual push (prepend to the shared head). Its **ADR-0012 foreign-pointer/range validation** becomes **O(chunks) = O(log N)** in dynamic mode, because it must find which chunk owns the block. In fixed mode (one chunk) it stays O(1) exactly as today.
- `destroy` walks the chunk list, releasing each backing — **O(chunks) = O(log N)**.

The chunk descriptors are **per-chunk** metadata (a few small allocations, O(log N) total), so ADR-0015's **per-block overhead stays zero** — no header is stolen from any block. The per-pool metadata grows by a chunk-list head pointer plus the growth config (factor, mode); that interacts with the ADR-0015 128-byte budget (tight in combination with the MUTEX policy's embedded `std::mutex`) and M5.3 must keep the `static_assert` green or renegotiate per ADR-0015 §4.

The chunk-list structure realised this way is the **Composite** pattern — its data-structure design and implementation are M5.2's subject (its own ADR); this ADR fixes only that it is a forward-linked, append-only list with a shared free list.

### 4. Chunks are append-only and never moved

A new chunk is *added*; existing chunks are never reallocated or relocated. This is non-negotiable: the entire value of a pool is **stable block addresses** — outstanding pointers must never be invalidated. This rules out a `realloc`/vector-style "grow by moving to a bigger buffer" model. Each chunk is individually contiguous (spec §2.2's "contiguous block"); the pool is contiguous *per chunk*, not globally.

## Alternatives Considered

- **Linear growth (fixed-size chunks).** Rejected. Predictable memory, but O(N / chunk_size) chunks degrade the `free` safety check and `destroy` to O(N) and bloat the chunk list. Geometric's O(log N) chunk count is the whole point.
- **Compile-time growth knob (à la ADR-0020).** Rejected. Growth's decision point is the exhaustion slow path, so a runtime per-pool flag costs nothing in steady state — and per-pool flexibility is genuinely useful (a program may want one fixed and one growable pool). Compile-time would forfeit that for no fast-path benefit.
- **`realloc`/move-the-backing growth (vector model).** Hard-rejected. It invalidates every outstanding pointer — the antithesis of a pool. Chunks must be additive and immovable (§4).
- **Per-block chunk header for O(1) chunk lookup in `free`.** Rejected. It would make the `free` safety check O(1) again, but at the cost of per-block metadata — violating ADR-0015's zero-per-block guarantee, the pool's headline efficiency property. The O(log N) chunk-walk is the accepted price.
- **Per-chunk descriptor stored inline at the head of each chunk's backing.** Considered, deferred to M5.2. It saves the small separate descriptor allocations but complicates block alignment and the block-count math; M5.2 weighs it as an implementation detail of the Composite.
- **Geometric "new chunk = initial_size × Fⁿ" vs "new chunk doubles the running total".** Both are O(log N); this ADR fixes the running-total-doubling form (`new total = current × F`) as canonical and records the per-chunk-exponential form as an equivalent variant M5.3 may choose.
- **Returning blocks to the OS by freeing emptied chunks (shrink-on-idle).** Out of scope for M5. Growth is monotonic this milestone; shrinking is a possible later enhancement (it would reintroduce the address-stability and concurrent-safety questions §4 sidesteps). Recorded so the omission is explicit.

## Consequences

**Positive**

- The default fixed-size pool is unchanged (v0.4.0 behaviour bit-for-bit); dynamic mode is purely additive and opt-in.
- Geometric growth bounds chunk count to O(log N), keeping `free`'s safety check and `destroy` sub-linear and the chunk list short.
- The shared single free list preserves O(1) `alloc` and O(1) `free`-push across chunks; only the *validation* in `free` and `destroy` scale with the (small) chunk count.
- Per-block overhead stays zero (ADR-0015) — chunk descriptors are per-chunk, O(log N) total.
- Sets up M5.2 (Composite) with a settled policy: append-only forward-linked chunks, one shared free list, geometric sizing.

**Negative**

- In dynamic mode `free` is no longer strictly O(1) for its safety check (it is O(log N)); fixed mode is unaffected. Documented as the dynamic-mode trade-off.
- The per-pool metadata grows (chunk-list head + growth config), pressuring the ADR-0015 128-byte budget when combined with the MUTEX policy's `std::mutex`; M5.3 carries the obligation to keep the `static_assert` green or renegotiate.
- Geometric growth can over-allocate (up to ~`F`× the high-water mark of live blocks). This is the standard amortized-growth space/time trade-off; a future shrink-on-idle (rejected above) is the mitigation if it ever matters.
- A growth allocation can fail (OOM); the exhaustion path must then fall back to the fixed-mode failure semantics (`NULL` / `std::bad_alloc`) — M5.3 specifies the exact behaviour.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0022.
- [`ROADMAP.md`](../../ROADMAP.md) §5.1 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M5.1)* entry.

No source changes land in this PR — M5.1 is the policy decision. The Composite chunk-list representation (M5.2), the `memory_pool_create`-side configuration surface and the growth slow path (M5.3), and the exhaustion-and-grow tests/benchmarks (M5.4) follow.

## References

- spec §2.2 — the optional dynamic-growth requirement and the "new contiguous block" wording.
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1 — the implicit free list this ADR threads across chunks; §3 — the `size_t`-overflow guard the growth arithmetic reuses.
- [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) — the range check whose cost moves from O(1) to O(log N) in dynamic mode.
- [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) — per-block zero (preserved) and the per-pool 128-byte budget (pressured).
- *Introduction to Algorithms* (Cormen et al.), amortized analysis of geometric (doubling) growth — the O(log N) chunk-count and amortized-O(1) result.
