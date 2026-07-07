# ADR-0042: `std::pmr::memory_resource` Adapter — `PoolMemoryResource`

- **Status:** Accepted
- **Date:** 2026-07-07
- **Deciders:** Daniel Polo (maintainer / project architect)
- **Related:** [ADR-0018](0018-stl-allocator-adapter.md) (the STL allocator Adapter whose alternatives explicitly left this "door open"), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§5 (the fixed `block_size` and `alignof(std::max_align_t)` guarantee bounding which requests the pool can serve), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) (the `Pool` the adapter references), [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 (the `std::bad_alloc`-on-exhaustion verb the pool path forwards to), [ADR-0037](0037-new-feature-roadmap-placement.md) (this feature opens roadmap Milestone 9), issue #107, spec [§5.2](../specs/01_spec_cpp_memory_pool.md#52-c17-surface)

## Context

[ADR-0018](0018-stl-allocator-adapter.md) added `PoolAllocator<T>`, the compile-time
*Cpp17Allocator* Adapter. Its alternatives section recorded one option as *deferred, not
rejected*: a `std::pmr::memory_resource` subclass, which "would let one pool back many
`pmr` containers without the template rebind dance … recorded here so the door stays open."
Roadmap Milestone 9 (post-1.0 ergonomics; [ADR-0037](0037-new-feature-roadmap-placement.md))
opens that door.

`PoolAllocator<T>` is a *type* — `std::list<int, PoolAllocator<int>>` bakes the pool into the
container's type, and every distinct element type needs its own rebound specialisation. The
`std::pmr` model is different and complementary: containers are spelled `std::pmr::list<int>`
(their allocator is the type-erased `std::pmr::polymorphic_allocator<T>`), and the storage
policy is chosen at **run time** by handing them a `std::pmr::memory_resource*`. One resource
object can therefore back heterogeneous containers (`std::pmr::list`, `std::pmr::vector`,
`std::pmr::string`, …) without any rebind ceremony.

Four forces shape the design, mirroring ADR-0018's four questions adapted to `pmr`:

1. **What does the resource own?** A `memory_resource` is used through a pointer and copied
   into every `polymorphic_allocator`; the move-only `Pool` (ADR-0010) cannot be its member.
2. **What is the request shape?** `do_allocate(bytes, alignment)` — a single contiguous
   region with an explicit alignment, and **no element count**. The fit test is therefore in
   bytes, not in `(n, sizeof(T))`.
3. **How does `do_deallocate` route without per-pointer bookkeeping?**
4. **How is portability handled**, given `std::pmr` shipped unevenly across standard
   libraries?

## Decision

We add a header-only Adapter,
`it::d4np::memorypool::PoolMemoryResource : public std::pmr::memory_resource`, in
[`pool_memory_resource.hpp`](../../src/main/cpp/it/d4np/memorypool/pool_memory_resource.hpp).

1. **Non-owning back-reference.** It holds a `Pool*` **and** an upstream
   `std::pmr::memory_resource*`. The pool and the upstream are owned elsewhere and must
   out-live the resource and every `polymorphic_allocator` / container bound to it — the same
   lifetime contract `polymorphic_allocator` places on its resource, and `PoolAllocator<T>`
   on its `Pool`.

2. **Deterministic size/alignment routing — no per-pointer bookkeeping.** A request routes to
   the pool **iff** one block can satisfy it:

   ```cpp
   bytes <= pool.block_size() && alignment <= alignof(std::max_align_t)
   ```

   Pool-eligible requests are served by `Pool::try_allocate`, throwing `std::bad_alloc` on
   exhaustion (ADR-0016 §2). Every other request (over-sized or over-aligned) is delegated to
   the **upstream** resource. Because `do_deallocate` receives the same `(bytes, alignment)`
   the matching `do_allocate` got, and `block_size()` is invariant for the pool's lifetime,
   the predicate evaluates identically at allocate and deallocate — every pointer is freed by
   exactly the path that produced it, with zero bookkeeping. This is the same insight that
   makes `PoolAllocator<T>` correct (ADR-0018 §2), re-expressed in `pmr`'s byte/alignment
   interface.

3. **Configurable upstream.** The constructor takes
   `std::pmr::memory_resource* upstream = std::pmr::get_default_resource()`, so the resource
   composes into a `pmr` chain like the standard pool resources — and a test can inject
   `std::pmr::null_memory_resource()` to prove a request was routed upstream (it throws)
   rather than served by the pool.

4. **Equality by identity.** `do_is_equal` is `true` iff `other` is also a
   `PoolMemoryResource` bound to the **same** pool and the **same** upstream, so a
   `polymorphic_allocator` may deallocate through either.

5. **Availability gate.** The whole facility is compiled only when
   `__cpp_lib_memory_resource >= 201603L` (detected after a `__has_include(<memory_resource>)`
   probe) and exposed through `PBR_MEMORY_POOL_HAS_PMR`. Where `std::pmr` is absent the header
   is a harmless no-op — the same "gate the optional surface" posture as the diagnostics gate
   (ADR-0019).

The adapter is header-only and purely additive: no change to the frozen C ABI or the existing
C++ types, so this is a SemVer **MINOR** (a `v1.2.0` candidate).

## Alternatives Considered

- **Do nothing — tell users to wrap `PoolAllocator<T>` themselves.** Rejected. It leaves the
  ADR-0018 door shut: `std::pmr` containers cannot use the pool at all without a resource, and
  hand-rolling one per project is exactly the reusable component this library should provide.
- **Fall back to the upstream on pool *exhaustion* too.** Rejected, for the same reason
  ADR-0018 rejected it for `PoolAllocator`: a pool-eligible pointer could then have come from
  either the pool or the upstream, so `do_deallocate` could no longer decide the path from
  `(bytes, alignment)` alone — forcing a side table or an ownership probe on every free.
  Exhaustion of a pool-eligible request therefore throws `std::bad_alloc` (ADR-0016 §2).
- **Owning resource (hold the `Pool` by value).** Rejected. `Pool` is move-only and a
  `memory_resource` is referenced/pointed-to, not copied by value into a container; the
  non-owning back-reference is the only shape that fits the `pmr` model, and it matches
  `PoolAllocator`.
- **No upstream parameter — hardcode `get_default_resource()` (or global `operator new`).**
  Rejected. An explicit, defaulted upstream is idiomatic `pmr` (resources compose into
  chains), costs nothing at the call site, and makes the routing testable by injecting
  `null_memory_resource`.
- **Route in `do_deallocate` by asking the pool "is this pointer mine?"** Rejected. There is
  no public ownership predicate, and `memory_pool_free` treats a foreign pointer as a silent
  no-op (ADR-0012) — which would leak an upstream pointer of a pool-eligible size. The
  deterministic size route is the only coherent option.
- **Assume C++17 conformance; don't gate on availability.** Rejected. libc++ shipped
  `<memory_resource>` well after C++17, and freestanding profiles omit it; a feature-test gate
  keeps the header portable and the CI matrix (ADR-0005) green everywhere.

## Consequences

**Positive**

- A single `PoolMemoryResource` backs any number of heterogeneous `std::pmr` containers and
  any `polymorphic_allocator`, with no per-type rebind — the ergonomic win ADR-0018 foresaw.
- Deterministic routing gives correct `do_deallocate` with zero per-pointer overhead; the
  adapter adds only a couple of compile-time-foldable comparisons and one `block_size()` load
  on the pool path.
- Header-only and additive: zero library-ABI impact and zero metadata-budget impact
  (ADR-0015 untouched). It is a clean, documented **MINOR**.

**Negative**

- `std::pmr` dispatches through a virtual call on every (de)allocation. That cost is inherent
  to the `pmr` model and is precisely why `PoolAllocator<T>` (inlineable, non-virtual) remains
  the zero-overhead default; `PoolMemoryResource` is the *ergonomics* option, chosen with eyes
  open.
- As with `PoolAllocator`, a single container can hold storage from two sources over its
  lifetime (pool for fitting node allocations, upstream for over-sized buffers). Correct, but
  worth documenting.
- The non-owning back-reference puts a hard lifetime contract on the caller: the pool **and**
  the upstream must out-live the resource and everything bound to it.

**Testing / tooling / documentation (landing in the same PR)**

- [`pool_memory_resource_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_memory_resource_test.cpp)
  — a dedicated doctest binary (CTest `pool_memory_resource`), gated like the test file so it
  builds even where `std::pmr` is absent; it runs under the existing ASan/UBSan/TSan matrix.
- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) — §5.2 lists
  `PoolMemoryResource`; §7 maps it to this ADR; §7.1 no longer defers it. (The spec is a
  translated source but is already `stale` from the prior spec PRs, so this edit adds no new
  i18n debt.)
- [`ROADMAP.md`](../../ROADMAP.md) — Milestone 9 is created (ADR-0037) with this as item 9.1.
- **Deferred to the `v1.2.0` release PR** to keep this feature PR off the *translated* docs
  surface (editing them trips the `i18n-freshness` gate): the
  [`docs/patterns/README.md`](../patterns/README.md) **Adapter** row refinement — the Adapter
  pattern is already catalogued (row 5, ADR-0018) and `PoolMemoryResource` is a second
  application of it, fully documented here and in the spec §7 map — and the `README.md`
  Milestone-9 table row (a release-time refresh per the M8.8 pattern). The release PR refreshes
  both and re-syncs the `zh-Hans` / `ja` translations in one pass.
- [`docs/doxygen/Doxyfile`](../doxygen/Doxyfile) `PREDEFINED` gains
  `__cpp_lib_memory_resource=201603L` so the gated class is documented on the API site.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — an *Added* entry.

## References

- ISO C++17 [mem.res.class] — the `std::pmr::memory_resource` interface (`do_allocate`,
  `do_deallocate`, `do_is_equal`).
- ISO C++17 [mem.poly.allocator.class] — `std::pmr::polymorphic_allocator` and its resource
  lifetime contract.
- [ADR-0018](0018-stl-allocator-adapter.md) — the sibling `PoolAllocator<T>` Adapter and the
  deterministic-routing rationale reused here.
- [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 — the `std::bad_alloc`
  exhaustion verb the pool path forwards to.
- cppreference — `std::pmr::memory_resource`, `std::pmr::polymorphic_allocator`,
  `std::pmr::null_memory_resource`, `std::pmr::get_default_resource`.
