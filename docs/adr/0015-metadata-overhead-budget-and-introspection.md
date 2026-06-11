# ADR-0015: Metadata-overhead budget and introspection contract

- **Status:** Accepted
- **Date:** 2026-06-11
- **Deciders:** Daniel Polo (maintainer), Claude (architect agent)
- **Related:** spec §3.2, ROADMAP §2.10, [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1 + §6 (the implicit free list that drives per-block metadata to zero, and the five-field `struct memory_pool`), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §1 (the Pimpl boundary that hides the struct from consumers and forces an introspection accessor to live in the public API).

## Context

Spec §3.2 says *"L'uso di metadati interni per tracciare i blocchi liberi (es. tramite una lista concatenata interna o un bitmask) deve essere minimo"* — metadata overhead must be minimal. The wording is intentionally qualitative and leaves three concrete questions open:

1. **What counts as "metadata"?** The bytes that aren't user-visible block storage. Concretely: the five-field `struct memory_pool` (per-pool fixed cost) plus any auxiliary state that scales with `block_count` (per-block variable cost).

2. **How small is "minimal"?** ADR-0009 §1 already settled the per-block cost: the implicit free list stores next-free links **inside the free blocks' own bytes**, so external per-block metadata is **0 bytes** by construction. Per-pool cost is a small constant — currently `sizeof(struct memory_pool)` = five 8-byte fields = 40 bytes on every Tier-1 64-bit host (4 × 4 bytes + 4 = 20 bytes on hypothetical 32-bit, none in our Tier-1 set).

3. **How is the contract enforced over time?** Spec wording without a number is not a regression gate. A future M5 dynamic-growth variant might add a chunk-list pointer; an M6 instrumented variant might add counters. The contract must say "metadata stays within budget X" and CI must assert it, otherwise the qualitative guarantee silently erodes over milestones.

ROADMAP §2.10 frames this as *"Metadata-overhead measurement and budget: instrumented test reports bytes of pool-internal metadata as a function of `block_count`; result documented in an ADR and asserted as a CI lower bound (spec §3.2)"*. Three deliverables: a measurement (the function the test calls), a documented result (this ADR), a CI gate (the test that fails if the budget is exceeded).

The Pimpl boundary fixed by [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §1 means consumers cannot `sizeof(memory_pool_t)` directly — the type is opaque on the consumer side, defined only in `memory_pool.cpp`. So the measurement cannot be a compile-time fact at the call site; it must come through the public API. This ADR's central decision is what shape that public-API addition takes.

## Decision

We add a single public introspection function to the C API, fix a 128-byte upper-bound budget on per-pool metadata, and gate the budget through both a compile-time `static_assert` and a runtime doctest CHECK.

### 1. Metadata cost model

- **Per-block metadata: 0 bytes.** The implicit free list (ADR-0009 §1) stores the next-free link in the free block's own first `sizeof(void*)` bytes. Allocated blocks carry no metadata. Free blocks borrow `sizeof(void*)` of their own storage for the link, but those bytes are *unused* by the consumer when the block is free, so they don't count as external overhead.
- **Per-pool metadata: `sizeof(struct memory_pool)`.** Currently 40 bytes on every Tier-1 64-bit host — five pointer/size_t-shaped fields (`backing_`, `head_`, `block_size_`, `block_count_`, `alignment_` per ADR-0009 §6), no padding required because all five fields are 8-byte aligned. The metadata struct is allocated by the call to `memory_pool_create` and released by `memory_pool_destroy`, matching the backing buffer's lifetime.
- **Asymptotic profile:** O(1) in `block_count` and O(1) in `block_size`. Total memory footprint of a configured pool is `block_size × block_count + sizeof(struct memory_pool)`; the second term is the metadata cost we report. Overhead percentage = `metadata / (vended + metadata)`; for the README's canonical 64-byte × 1024-block configuration that is `40 / (65536 + 40) = 0.061 %`. For a pool with one million 64-byte blocks the overhead is `40 / (64 000 000 + 40) ≈ 6 × 10⁻⁵ %`.

### 2. Public-API introspection

Add to [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h):

```c
/**
 * Report the per-pool metadata overhead in bytes (spec §3.2).
 *
 * Returns the size of pool-internal bookkeeping — currently the
 * `struct memory_pool` itself, per ADR-0009 §6 and ADR-0015. The value
 * is O(1) in both `block_count` and `block_size`: a pool with one
 * million blocks reports the same number as a pool with one. Per-block
 * metadata is zero by construction (implicit free list, ADR-0009 §1).
 *
 * @param pool Pool to inspect, or `NULL`.
 * @return Number of metadata bytes, or 0 if @p pool is `NULL`.
 */
size_t memory_pool_metadata_bytes(const memory_pool_t* pool);
```

Per-pool not class-level: future M5 dynamic-growth variants may carry chunk-list state whose size depends on growth history, and per-pool reporting absorbs that without re-shaping the API. NULL is a defined input that returns 0 (no metadata), matching the rest of the API's NULL-tolerance posture.

The C++ side gains a thin `noexcept` forwarder in [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp):

```cpp
[[nodiscard]] std::size_t metadata_bytes() const noexcept {
    return memory_pool_metadata_bytes(handle_);
}
```

Member function (not free function) so the call reads `pool.metadata_bytes()` against an RAII `Pool` value. `noexcept` because the underlying C function makes no allocation. `[[nodiscard]]` because callers must use the value — discarding it is almost certainly a bug.

### 3. Budget

The CI-asserted upper bound on per-pool metadata is **128 bytes**, hard-coded as a `constexpr` in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) and enforced via a `static_assert`:

```cpp
static_assert(sizeof(memory_pool) <= 128U,
              "ADR-0015: per-pool metadata budget exceeded — update the ADR before raising");
```

The current size is 40 bytes; the budget gives 88 bytes of headroom — room for ~11 additional 8-byte fields before the contract requires renegotiation. The slack is intentional: future milestones may add a chunk-list pointer (M5), a stats handle (M6), or a thread-safety strategy slot (M4); each costs a pointer. Renegotiating the budget on every milestone would be churn; renegotiating once if the headroom is genuinely exceeded is honest.

The runtime test in [`pool_smoke_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp) re-asserts the same budget against the value returned by `memory_pool_metadata_bytes`, plus three orthogonal invariants:

- `metadata_bytes(NULL) == 0` (defined no-op),
- `metadata_bytes(live) > 0` (sanity — a live pool reports its struct),
- `metadata_bytes(small_pool) == metadata_bytes(huge_pool)` (O(1) in `block_count` — pull a 1024-block pool and a 1 000 000-block pool, verify they report the same number).

The duplication (compile-time static_assert + runtime CHECK) is deliberate: the static_assert catches a budget regression at compile time on every build cell of the CI matrix, while the runtime CHECK gates the **reported value** against the same budget — the two could diverge in a buggy future implementation that, e.g., returns a hard-coded constant from `memory_pool_metadata_bytes` while the underlying struct grows past 128.

### 4. Renegotiating the budget

If a future ADR materially changes the metadata cost — for example, M5's dynamic-growth chunk-list pointer that crosses the 128-byte line — the budget number in this ADR is updated **in the same PR that introduces the change**. The new ADR carries a `Status: Superseded by ADR-XXXX` reference; this ADR's `Status` stays `Accepted` (the renegotiation isn't a full supersession, only a number update). The compile-time `static_assert` and the runtime CHECK are updated in lockstep.

Renegotiation must be explicit. A milestone PR that quietly nudges `sizeof(memory_pool)` past 128 fails the static_assert and stops the build — that is the entire point of the gate.

## Alternatives Considered

- **No introspection function.** Just static_assert the budget in `memory_pool.cpp` and leave runtime callers without a way to query the cost. Rejected because (a) the runtime test cannot then verify the value is also under budget (it can only verify the struct's compile-time size, but that's not what the API promises to consumers), (b) production consumers want to report their app's memory footprint in their own diagnostics and benefit from a stable accessor, and (c) future variants (M5 dynamic growth) may have per-pool metadata that varies at runtime — only an accessor can report it accurately.

- **Per-block metadata via bitmap or allocation table.** A bitmask of `(block_count + 7) / 8` bytes tracking which slots are allocated would let `memory_pool_free` detect double-free without the M2.7 range-check workaround for foreign pointers. Rejected because the implicit free list (ADR-0009 §1) already achieves zero per-block external metadata, and the bitmap re-introduces O(block_count) memory overhead that spec §3.2 explicitly tells us to avoid. Double-free detection stays deferred to Milestone 6's Decorator variant (per [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) §4), where the cost is opt-in.

- **Class-level constant (`MEMORY_POOL_METADATA_BYTES` macro or constexpr).** Expose the metadata cost as a compile-time constant the consumer can read without holding a pool. Rejected because (a) it locks every consumer's assumption to the current struct size at consumer-compile time, breaking ABI stability across milestones, (b) it cannot represent M5's variable per-pool metadata, and (c) the function-call cost is negligible against allocator work the consumer is already doing.

- **Tighter budget (e.g., 48 bytes, equality assertion).** Set the budget to the exact current size and force a deliberate update at every field addition. Rejected because the resulting churn (one ADR update per field added) outweighs the safety benefit; 128 bytes is generous but not so large that it hides growth.

- **Looser budget (e.g., 256 or 512 bytes).** Set the budget so high that no foreseeable milestone could ever hit it. Rejected because the budget is also a *documentation device* — it tells future contributors what "minimal" means concretely. A 512-byte ceiling implies metadata cost up to ~12 % overhead on a single-block 64-byte pool; that's not "minimal" by any honest read of spec §3.2.

- **Runtime-only gate (no compile-time static_assert).** Rely on the doctest CHECK alone. Rejected because the static_assert catches the regression on every cell of the build matrix, including release builds that do not run tests (`PBR_MEMORY_POOL_BUILD_TESTS=OFF` is a legitimate consumer configuration). The static_assert is a 1-line cost for matrix-wide coverage.

- **Compile-time-only gate (no runtime CHECK).** Rely on the static_assert alone, skip the doctest cases. Rejected because the contract documented in this ADR is that `memory_pool_metadata_bytes` reports a number under budget — not that the struct is under budget. Those could diverge if the function were ever changed to report something other than `sizeof(struct memory_pool)` (for example, summing the struct plus an external chunk list in M5). The runtime CHECK pins the *reported value* under the same budget.

## Consequences

- **Public API gains one function.** `memory_pool_metadata_bytes(const memory_pool_t*)` is a permanent addition; consumers may rely on it indefinitely. The function is held to the same ANSI C C89 compatibility contract as the existing four functions (spec §3.3, [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3) — `const` qualifier, `size_t` return, opaque pointer parameter; all C89 constructs.

- **C++ wrapper gains one method.** `Pool::metadata_bytes() const noexcept` joins the existing forwarders on `it::d4np::memorypool::Pool`. `sizeof(Pool)` is unchanged (the wrapper is still a single `memory_pool_t*` per ADR-0010 §1) — the method is a thin call-through.

- **Compile-time budget gate.** The `static_assert(sizeof(memory_pool) <= 128U, ...)` in `memory_pool.cpp` fires on every CI cell that builds the library. The 14-cell build matrix (ADR-0005 §4) therefore becomes the budget's enforcement surface.

- **Runtime budget gate.** The new TEST_CASEs in `pool_smoke_test.cpp` run under every CTest cell — debug, release, ASan, UBSan on Linux × {GCC, Clang}, Windows × MSVC, macOS × Apple Clang. A regression that, e.g., returned a wrong number from `memory_pool_metadata_bytes` while the struct stayed under budget would still trip the runtime CHECK.

- **No new dependency.** The introspection function returns `sizeof()` of an internally-defined struct; no allocation, no system call, no FFI dependency.

- **Spec Coverage Map row §3.2 closes.** Prior status was ⏳ (pending). With M2.10 landing, §3.2 has a measured value, a documented budget, and a CI gate — full coverage. The row flips to ✅ in the same PR.

- **Limitation: 128-byte budget is a coarse instrument.** It rules out *gross* growth (doubling, tripling) but does not catch a field-by-field creep that respects the budget. A future Milestone 7.6 acceptance audit (per ROADMAP §7.6) walks the field list deliberately to verify nothing snuck in; this ADR is the lower-resolution machinery between such audits.

- **Limitation: per-block metadata is reported as 0, but free blocks borrow `sizeof(void*)` of their own bytes for the next-link.** That is *zero external* metadata — the bytes were allocated to the pool and the consumer cannot use them whether the block is free or not (when free, the link uses them; when allocated, the consumer overwrites them). For accounting purposes the 0-bytes-per-block claim is honest and matches the ADR-0009 §1 design. A future contributor reading this ADR should not interpret "0 per block" as "no bytes touched per block".

## References

- [Spec §3.2](../specs/01_spec_cpp_memory_pool.md) — *L'uso di metadati interni... deve essere minimo.*
- [ROADMAP §2.10](../../ROADMAP.md) — the milestone item this ADR fulfils.
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1 + §6 — the implicit free list (which is why per-block metadata is 0) and the five-field struct (which is what per-pool metadata measures).
- [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §1 — the Pimpl boundary that hides the struct from consumers and motivates the public introspection accessor.
- [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) §4 — the deferral of double-free detection to M6 Decorator, which is what makes the bitmap alternative unnecessary at this milestone.
- *The C++ Programming Language* 4th ed. — Stroustrup, §32.4 on implicit object lifetime and the well-defined nature of writing through a `void**` lvalue into raw `::operator new` storage.
