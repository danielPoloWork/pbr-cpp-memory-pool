# ADR-0019: Read-Only Free-List Diagnostic Iterator — Compile-Time Gating, C-Encapsulated Traversal, and LegacyForwardIterator Surface

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1 (the implicit free-list layout the traversal walks — next-link in the first `sizeof(void*)` bytes of each free slot, ascending-address initialisation order), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) (the Pimpl boundary the diagnostic accessors must not breach), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the range check this iterator complements for diagnostics), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) / [ADR-0018](0018-stl-allocator-adapter.md) §3 (the C introspection-accessor precedent these follow), [ROADMAP](../../ROADMAP.md) §3.4 (this ADR's roadmap item).

## Context

Milestone 3.4 adds the **Iterator** pattern: a read-only walk of the pool's implicit free list for diagnostics and tests — counting free blocks, verifying free-list integrity, asserting LIFO/ascending invariants, and inspecting fragmentation. The roadmap is explicit that it must be **disabled in release builds unless explicitly enabled**: a free-list walk is `O(free_count)`, it has no place on the allocation hot path, and exposing the internal layout in production builds would invite misuse.

Three questions shape the design:

1. **How is "disabled in release unless explicitly enabled" expressed** without fragmenting the ABI or risking link mismatches?
2. **How does the iterator traverse the free list** without breaching the ADR-0010 Pimpl boundary or re-encoding the ADR-0009 §1 layout contract in a public C++ header?
3. **What iterator surface** does it present — what category, what value type, what range entry point?

## Decision

### 1. One compile-time gate: `PBR_MEMORY_POOL_DIAGNOSTICS`

A single macro gates the **entire** diagnostic surface — both the C accessors (declarations in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h), definitions in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp)) and the C++ iterator header. Its default is build-type driven, and any explicit definition wins:

```c
#ifndef PBR_MEMORY_POOL_DIAGNOSTICS
#  ifdef NDEBUG
#    define PBR_MEMORY_POOL_DIAGNOSTICS 0   /* release: compiled out */
#  else
#    define PBR_MEMORY_POOL_DIAGNOSTICS 1   /* debug: available */
#  endif
#endif
```

The macro lives in `memory_pool.h` so every translation unit — the C library source, the C++ iterator header, the C consumer — sees the same value. The "explicit enable" channel is a CMake option, `PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS` (default `OFF`), which when `ON` attaches `PBR_MEMORY_POOL_DIAGNOSTICS=1` as a **PUBLIC** compile definition on `pbr_memory_pool`, so the library and every target that links it agree.

This project always builds the library and its consumers from source in one CMake configure, so the macro is uniform across a build tree and there is no cross-TU disagreement and no prebuilt-ABI mismatch to worry about. Gating both the declarations and the definitions (rather than always compiling the C functions) gives the cleanest story: in a release build the diagnostic symbols simply do not exist, matching the roadmap's intent literally.

### 2. Traversal is encapsulated behind three C accessors — the layout stays in the `.cpp`

The free-list head and the next-link layout live behind the Pimpl boundary (`struct memory_pool` is defined only in `memory_pool.cpp`). Rather than re-encode "the next pointer is in the first `sizeof(void*)` bytes" in a public C++ header — which would couple the header to ADR-0009 §1 and duplicate the contract — the traversal is exposed through three tiny C accessors that keep the layout knowledge inside the implementation TU:

```c
const void* memory_pool_debug_free_list_head(const memory_pool_t* pool);
const void* memory_pool_debug_free_list_next(const memory_pool_t* pool, const void* current);
size_t      memory_pool_debug_free_count(const memory_pool_t* pool);
```

All three are `NULL`-tolerant (head/next return `NULL`, count returns `0`), `const`-correct, and held to the same ANSI-C C89 contract as the rest of the public C API (ADR-0018 §3 precedent), exercised by [`c_consumer_min.c`](../../src/test/c/it/d4np/memorypool/c_consumer_min.c) under the C89/C99 jobs. `next` reads the link with the same `static_cast<void* const*>` idiom the initialiser uses, so the const-correct read provably does not cast away `const`. `free_count` is the bundled convenience walk; the test suite cross-checks it against `std::distance(begin, end)` so the iterator and the count validate each other.

### 3. A LegacyForwardIterator over `const void*` slot addresses, entered through a view

[`free_list_iterator.hpp`](../../src/main/cpp/it/d4np/memorypool/free_list_iterator.hpp) (header-only, gated) defines:

- `FreeListIterator` — a **LegacyForwardIterator** whose `value_type` is `const void*` (the address of a free slot). Because the "current" slot address is the iterator's own state, `operator*` returns a reference to that member (`reference = const value_type&`), which keeps it a genuine forward iterator rather than an input-only proxy. `operator++` advances via `memory_pool_debug_free_list_next`; equality compares the current address; the default-constructed iterator (`current_ == nullptr`) is the end sentinel.
- `FreeListView` — a lightweight range with `begin()` / `end()`, constructible from a `const memory_pool_t*` or, for ergonomics, from a `Pool&` (forwarding through `Pool::native_handle`). It is the entry point: `for (const void* slot : FreeListView{pool}) { ... }`.

The iteration order is the free-list link order: ascending addresses for a fresh pool (ADR-0009 §1), then LIFO-perturbed as blocks are allocated and freed — which is exactly what makes it useful for diagnostics. Being a forward iterator, it composes with `std::distance`, `std::find`, `std::count_if`, and range-`for`.

## Alternatives Considered

- **Always compile the C accessors; gate only the C++ header.** Rejected. It leaves diagnostic symbols in the release library, which is neither what the roadmap asks for nor necessary — the build-from-source model means gating both has no link-mismatch downside. One macro, one story.
- **Re-encode the next-link layout in the C++ iterator header** (read `*static_cast<void* const*>(slot)` directly in `operator++`). Rejected. It duplicates the ADR-0009 §1 contract across the Pimpl boundary; a future layout change (e.g. an XOR-linked or offset-encoded list) would silently break the header. The C accessor keeps the single source of truth in the implementation TU.
- **A runtime flag instead of a compile-time macro.** Rejected. A runtime toggle cannot remove the code or the symbols from a release build, and it adds a branch the roadmap explicitly wants gone. The requirement is compile-time absence, not runtime suppression.
- **Expose the iterator on the stable C API (a C iterator handle).** Rejected. The Iterator pattern here is a C++-side diagnostic convenience; a C iterator would mean heap-allocating cursor state or a clumsy out-parameter protocol. The three stateless C accessors are sufficient, and the C++ iterator builds the ergonomic surface on top.
- **A mutable / writable iterator.** Hard-rejected. Mutating the free list through an iterator would corrupt the allocator invariants; the diagnostic surface is read-only by construction (`const void*` throughout, `const`-qualified accessors).
- **Input-iterator with by-value `operator*` (proxy return).** Rejected in favour of the member-reference form, so the type satisfies LegacyForwardIterator and multi-pass algorithms behave as users expect.

## Consequences

**Positive**

- Tests and diagnostics can assert free-list invariants directly (count, ordering, in-range, membership) instead of inferring them through alloc/free side effects — a stronger, more legible test surface for this and future milestones (dynamic growth in M5 will reuse it to verify chunk linking).
- Zero release-build footprint: with `NDEBUG` defined and the option `OFF`, the entire surface — C symbols included — is compiled out; the allocation hot path is untouched in every configuration.
- The Pimpl boundary and the ADR-0009 §1 layout contract each keep a single source of truth; the iterator depends on them only through the encapsulating C accessors.
- The iterator is a standard-conforming LegacyForwardIterator, so it composes with the `<algorithm>` / range-`for` machinery for free.

**Negative**

- The static library's symbol set now varies with build type / the diagnostics option. This is benign under the project's build-from-source model but would matter for a prebuilt-binary distribution; ADR-0004 §5's eventual packaging work must decide whether release artifacts ship diagnostics. Recorded here as the renegotiation point.
- Test code that exercises the iterator must guard with `#if PBR_MEMORY_POOL_DIAGNOSTICS` so the same binary still builds (with a trivial placeholder case) under a release CTest run. The cost is a little preprocessor scaffolding in one test file.
- The diagnostic walk reads each free slot's link, so on a huge pool it touches `free_count` cache lines — fine for diagnostics, never to be used on a hot path. The gating makes that misuse hard by default.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0019.
- [`docs/patterns/README.md`](../patterns/README.md) — **Iterator** moves to *Adopted* (status `Implemented`).
- [`ROADMAP.md`](../../ROADMAP.md) §3.4 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M3.4)* entry.

[`free_list_iterator.hpp`](../../src/main/cpp/it/d4np/memorypool/free_list_iterator.hpp) lands the iterator and view; [`free_list_iterator_test.cpp`](../../src/test/cpp/it/d4np/memorypool/free_list_iterator_test.cpp) lands its dedicated doctest binary (registered with CTest as `free_list_iterator`). The C accessors land in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) / [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp); the `PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS` CMake option lands in the top-level [`CMakeLists.txt`](../../CMakeLists.txt).

## References

- ISO C++17 [forward.iterators] — the LegacyForwardIterator requirements (multi-pass guarantee, `reference` as `value_type&`, default-constructible, equality-comparable).
- ISO C++17 [iterator.traits] — the member typedefs (`iterator_category`, `value_type`, `difference_type`, `pointer`, `reference`).
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1 — the implicit free-list layout and ascending-address initialisation order the traversal walks.
- [ADR-0018](0018-stl-allocator-adapter.md) §3 — the C introspection-accessor precedent (`memory_pool_block_size`) these diagnostic accessors follow.
