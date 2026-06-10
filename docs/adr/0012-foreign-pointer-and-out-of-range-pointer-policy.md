# ADR-0012: Foreign-Pointer and Out-of-Range Pointer Policy in `memory_pool_free`

- **Status:** Accepted
- **Date:** 2026-06-11
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §6 (the `struct memory_pool` fields used by the range check), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §2 (the C++ wrapper that inherits this policy through forwarding), [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §6.1 (the correctness test scenarios that drive this decision), [ROADMAP](../../ROADMAP.md) §2.7 (this ADR's roadmap item) + §6 (Milestone 6 instrumentation that extends this policy with optional logging).

## Context

The public C contract in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) currently states:

> Passing a pointer not previously returned by `::memory_pool_alloc` on the same `pool` is undefined behaviour; the policy for detecting and reporting such misuse is set in Milestone 2.7.

That clause was deferred because the *detection* requires fields on `struct memory_pool` (the backing pointer, the slot size, the slot count) that only landed in [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §6, and the *reporting* (abort? assert? log? silent ignore?) is a behaviour decision rather than a layout decision. With the M2.3 / M2.4 implementation now in place, [ROADMAP](../../ROADMAP.md) §2.7 is the moment to lock it down.

A foreign-pointer pass into `memory_pool_free` has three sub-shapes that share one root cause — *the caller handed us a pointer the pool does not own*:

1. **Out-of-range below**: `block < backing`. The user passed an address that lies before the pool's contiguous allocation. Common cause: stack pointer, foreign heap pointer, or arithmetic on a previously freed pool's address.
2. **Out-of-range above**: `block >= backing + (block_count * block_size)`. The user passed an address that lies after the pool's contiguous allocation. Common cause: same as above, or pointer arithmetic past the end of a legitimate block.
3. **In-range but misaligned**: `backing <= block < backing + (block_count * block_size)` AND `(block - backing) % block_size != 0`. The user passed an address that points into the pool's backing buffer but not at a slot boundary. Common cause: pointer arithmetic on a legitimate block (`alloc()` returns a slot, the user advances it by `sizeof(T)` then frees the advanced address).

Without a policy, each of these silently corrupts the free list — `*static_cast<void**>(block) = pool->head_;` writes through whatever pointer the caller handed us. For an out-of-range pointer this is an out-of-bounds write to unrelated memory (ASan / UBSan would catch it during M2.8 if Valgrind doesn't first); for an in-range misaligned pointer it writes inside the pool but at a slot boundary it doesn't legitimately own, corrupting the chain.

The policy question has three independent dimensions:

- **Detection cost**: the range check is three pointer comparisons + one modulo; the alignment check is the modulo. Both are `O(1)`, both fit in a handful of integer instructions, both run on the same fast path as the actual `pop_head` / `push_head`. Cost is negligible vs the rest of the function.
- **What "report" means**: silent no-op? Log via the Decorator that lands in Milestone 6? Throw across the C ABI (forbidden — see ADR-0005 §3)? Assert (debug-mode only)? Abort (always)? Return an error code (signature change — non-starter)?
- **Cross-allocation pointer comparison**: C++17 says comparing `<` between two pointers from different allocations is unspecified behaviour ([expr.rel]/4). The naïve range check `block < backing` is therefore UB when `block` is genuinely foreign. The fix is to compare `std::uintptr_t` values rather than pointers, which is portable and well-defined — at the cost of one `reinterpret_cast<std::uintptr_t>` per call (triggering `cppcoreguidelines-pro-type-reinterpret-cast`, requiring a narrow NOLINT).

The `free()` precedent in the C standard library is permissive — `free(NULL)` is a documented no-op; `free` on a non-`malloc`'d pointer is UB in the standard but typical implementations don't crash immediately. The `memory_pool_free` contract should be at least as forgiving as `free()` for the symmetry-with-C reason and for the spec's spirit ("fixed-block pool, no exception across the ABI").

## Decision

We adopt a **silent no-op via range + alignment check** policy:

```cpp
void memory_pool_free(memory_pool_t* pool, void* block) {
    if (pool == nullptr) { return; }
    if (block == nullptr) { return; }
    if (!is_block_in_range(pool, block)) { return; }   // foreign — silent no-op
    *static_cast<void**>(block) = pool->head_;
    pool->head_ = block;
}
```

The helper `is_block_in_range` lives in the anonymous namespace of `memory_pool.cpp`:

```cpp
bool is_block_in_range(const memory_pool* pool, const void* block) noexcept {
    // Compare as std::uintptr_t to avoid UB on cross-allocation pointer
    // comparison ([expr.rel]/4). NOLINT for the reinterpret_cast is
    // narrow and justified — this is exactly the use case the standard
    // documents as the portable form for "is this pointer in this range".
    const auto base_addr = reinterpret_cast<std::uintptr_t>(pool->backing_);
    const auto block_addr = reinterpret_cast<std::uintptr_t>(block);
    const std::uintptr_t end_addr = base_addr + (pool->block_size_ * pool->block_count_);
    if (block_addr < base_addr) { return false; }
    if (block_addr >= end_addr) { return false; }
    if (((block_addr - base_addr) % pool->block_size_) != 0U) { return false; }
    return true;
}
```

Three points become normative:

1. **Detection is mandatory.** Every call to `memory_pool_free` runs the range + alignment check before touching the free list. The `O(1)` overhead (~5 instructions on every Tier-1 platform) is the spec §3.2 "minimal metadata overhead" property pushed into the runtime path; it does not add per-block storage.

2. **Reporting is silent no-op.** A foreign or misaligned pointer causes `memory_pool_free` to return without modifying the pool state. The pool's `head_`, the free-list chain, and every block's contents are bit-identical to the state before the call.

3. **`reinterpret_cast<std::uintptr_t>` is the portable comparison primitive.** Comparing `void*` values directly across allocations is `[expr.rel]/4` unspecified behaviour; comparing the corresponding `std::uintptr_t` values is well-defined. The `cppcoreguidelines-pro-type-reinterpret-cast` violation is suppressed via a narrow `NOLINT` annotation at the two cast sites, with the same justification used in `pool_smoke_test.cpp`'s alignment check.

## Alternatives Considered

- **Keep UB on foreign pointers — no detection.** Rejected. The spec §6.1 explicitly lists "foreign-pointer / out-of-pool-range pointer policy" as a correctness scenario that must be tested. A "test" against UB is not a meaningful test (any outcome satisfies "undefined behaviour"). The detection is what makes the contract testable.
- **Assert (debug-only) on foreign pointer.** Considered. The C `assert()` macro is enabled when `NDEBUG` is not defined — i.e., in the project's `debug` CMake preset, but not in `release`. The benefit is a loud signal during development; the cost is that the M2.7 correctness tests (which deliberately pass foreign pointers) would abort the test binary in debug builds. We rejected the assert because the silent no-op + logging-in-M6 path serves the same "loud-in-development" use case without the test-suite mutilation — see Milestone 6 (Observability & Decorators).
- **Throw across the C ABI on foreign pointer.** Hard-rejected. ADR-0005 §3 forbids exceptions crossing the C ABI boundary; this rule is the foundation of the ANSI C compatibility verification (ROADMAP §1.10).
- **Always abort (`std::abort`) on foreign pointer.** Rejected. Forces every consumer of the C ABI to deal with abort termination even in production scenarios where the foreign-pointer pass might be benign (e.g., double-free racing with a graceful shutdown path). The silent-noop default is friendlier and matches the `free(NULL)` C precedent.
- **Add a return value (`bool` or error code) to `memory_pool_free`.** Hard-rejected. Spec §5 freezes the signature as `void memory_pool_free(memory_pool_t*, void*);`. Changing it is an ABI break with no upside given that consumers already cannot do anything useful with the return value when they pass a foreign pointer by mistake (they don't know they did).
- **Maintain a per-pool allocated-block set / bitmap for exact detection.** Rejected. A bitmap (or set) of allocated indices would catch the *double-free* case (passing a pointer that is *in range, aligned, and currently free* — i.e., already on the free list) that the chosen policy does not. But the per-pool storage cost is `ceil(block_count / 8)` bytes for a bitmap, or `O(N)` for a set; both violate spec §3.2 "minimal metadata overhead per block". The double-free case is left to M6's optional Decorator if needed — the *Observability & Decorators* milestone is the natural home for opt-in instrumentation that costs storage.
- **`std::less<const void*>` for the range comparison instead of `uintptr_t`.** Considered. `std::less` over pointers is documented to provide a consistent total order across the entire address space ([comparisons]/p4), which sidesteps the `[expr.rel]/4` UB on direct `<`. But computing the modulo `(block - backing) % block_size` after the comparison still requires either pointer subtraction (UB when `block` is foreign) or a `uintptr_t` cast — so we'd end up casting anyway. Picking `uintptr_t` for the whole check keeps the helper consistent and the NOLINT count to one per cast site rather than scattered NOLINTs across a mixed `std::less` + `uintptr_t` form.

## Consequences

**Positive**

- The spec §6.1 correctness scenarios become end-to-end testable. M2.7 lands a test case per failure shape (out-of-range below, out-of-range above, in-range misaligned, foreign heap pointer, stack pointer) that verifies the pool's `head_` and free-list chain are bit-identical before and after the offending call. The ASan / UBSan cells observe no out-of-bounds writes — the check rejects the foreign pointer before the free-list push.
- The C ABI stays exception-free (ADR-0005 §3 preserved). The C++ `Pool::deallocate` wrapper inherits the policy through its forward to `memory_pool_free` — no policy duplication, no surface divergence.
- The `free(NULL)`-style permissive idiom keeps the API maximally forgiving. Mixed code paths that may pass NULL, stale, or in-range-but-already-freed pointers (a subset of which is the double-free case the bitmap alternative would have detected) do not crash on the misuse — they no-op. The "loud in development" signal is reserved for Milestone 6's optional Decorator.
- The detection runs in `O(1)` with ~5 integer instructions on every Tier-1 platform. The microbenchmark (M2.9) will need to measure whether the overhead is observable against the malloc/free baseline; if it is, [ADR-0012](.) can later add a compile-time toggle (`PBR_MEMORY_POOL_FOREIGN_POINTER_CHECK=OFF`) for the absolute fast path. This ADR does not pre-empt that decision.

**Negative**

- The silent no-op policy means a programmer error (foreign pointer pass) goes undetected from the pool's perspective. The expectation is that ASan / UBSan in the development CI cells catches the upstream defect (the pointer arithmetic that produced the foreign pointer in the first place); for production code, M6's Decorator provides the opt-in logging surface. We accept this trade-off as the cost of permissive symmetry with `free()`.
- The double-free case (in-range, aligned, currently-on-the-free-list pointer) is NOT detected. A double-free causes the pool's `head_` to be its own predecessor, creating a cycle that future `alloc` calls would expose by handing the same slot twice. M2.7's tests do NOT exercise this scenario; M6's instrumentation may.
- One additional `reinterpret_cast<std::uintptr_t>` site (with NOLINT) on every `memory_pool_free` call. `cppcoreguidelines-pro-type-reinterpret-cast` would otherwise flag it; the narrow NOLINT with an inline comment pointing at this ADR is the project's standing pattern (same shape as the alignment check in `pool_smoke_test.cpp`).

**Required documentation updates landing in the same PR as this ADR**

- [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) — Doxygen on `memory_pool_free` drops the *"the policy for detecting and reporting such misuse is set in Milestone 2.7"* qualifier and documents the new silent-noop-with-detection contract.
- [`docs/adr/README.md`](README.md) — index row for ADR-0012.
- [`ROADMAP.md`](../../ROADMAP.md) §2.7 → `[x]` with the inline summary.
- [`ROADMAP.md`](../../ROADMAP.md) Spec Coverage Map — §6.1 (Correctness — exhaustion, null inputs, foreign / out-of-range pointers) ⏳ → ✅.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased`/`Changed (M2.7)` — entry recording the policy and the new tests.

[`src/main/cpp/it/d4np/memorypool/memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) and [`src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp) gain the helper, the extended guard, and the five new `TEST_CASE`s.

## References

- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §6.1 — the correctness scenarios this ADR makes testable.
- [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 — the no-exceptions-across-C-ABI rule this ADR honours.
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §6 — the `struct memory_pool` field list the range check reads.
- [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §2 — the wrapper that inherits this policy through `Pool::deallocate`'s forward to `memory_pool_free`.
- ISO C++17 [expr.rel]/4 — cross-allocation pointer `<` is unspecified behaviour; the `uintptr_t` cast is the portable workaround.
- [Milestone 6](../../ROADMAP.md) — where opt-in Decorator-based logging of detected foreign pointers will live.
