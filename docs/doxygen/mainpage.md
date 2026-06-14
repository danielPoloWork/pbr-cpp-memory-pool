# pbr-cpp-memory-pool {#mainpage}

A fixed-block-size, **O(1)** memory pool with a C++17 implementation behind an
ANSI C (C89-compatible) public surface and **zero external dependencies**.

This site is the **generated API reference** — the per-symbol contract
(parameters, return values, preconditions, thrown exceptions) extracted from the
Doxygen comments in the public headers. It is one of the two documentation
formats the project maintains; the rationale for the split is
[ADR-0013](https://github.com/danielPoloWork/pbr-cpp-memory-pool/blob/master/docs/adr/0013-doxygen-for-api-markdown-for-narrative.md)
(Doxygen for the API contract, Markdown for the narrative), and the pipeline that
publishes this site is
[ADR-0027](https://github.com/danielPoloWork/pbr-cpp-memory-pool/blob/master/docs/adr/0027-doxygen-html-site-and-publication-pipeline.md).

## Where to start

- **C public API** — the four-function contract from spec §5 lives in
  `memory_pool.h`: `memory_pool_create`, `memory_pool_alloc`,
  `memory_pool_free`, `memory_pool_destroy`.
- **C++ wrapper** — `it::d4np::memorypool::Pool` (RAII, move-only) and its
  construction surface (`Pool::make`, `PoolBuilder`) are in `memory_pool.hpp`.
- **Typed allocation** — `it::d4np::memorypool::TypedPool<T>` in
  `typed_pool.hpp` adds object-lifetime verbs over a typed pool.
- **STL integration** — `it::d4np::memorypool::PoolAllocator<T>` in
  `pool_allocator.hpp` is a *Cpp17Allocator* adapter for standard containers.
- **Observability** — `it::d4np::memorypool::InstrumentedPool` in
  `instrumented_pool.hpp` is an opt-in Decorator with counters and a
  `PoolObserver` lifecycle-event hook.
- **Diagnostics** — `it::d4np::memorypool::FreeListView` in
  `free_list_iterator.hpp` is a read-only iterator over the implicit free list,
  compiled in only when diagnostics are enabled.

## Beyond the API reference

The narrative documentation — the specification, the Architecture Decision
Records, the design-patterns catalogue, the roadmap, and the release notes — is
authored in Markdown and rendered on GitHub:

- [Project README](https://github.com/danielPoloWork/pbr-cpp-memory-pool/blob/master/README.md)
- [Specification](https://github.com/danielPoloWork/pbr-cpp-memory-pool/blob/master/docs/specs/01_spec_cpp_memory_pool.md)
- [Architecture Decision Records](https://github.com/danielPoloWork/pbr-cpp-memory-pool/tree/master/docs/adr)
- [Design-patterns catalogue](https://github.com/danielPoloWork/pbr-cpp-memory-pool/blob/master/docs/patterns/README.md)
- [Roadmap](https://github.com/danielPoloWork/pbr-cpp-memory-pool/blob/master/ROADMAP.md)

Licensed under the [MIT License](https://github.com/danielPoloWork/pbr-cpp-memory-pool/blob/master/LICENSE).
