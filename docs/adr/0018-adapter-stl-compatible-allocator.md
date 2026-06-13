# ADR-0018: Adapter — STL-Compatible Allocator over the Pool

- **Status:** Accepted
- **Date:** 2026-06-12
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0003](0003-design-patterns-policy.md) (every pattern adoption justified in an ADR), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §5/§6 (the alignment guarantee and backing extents the routing predicate reads), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the O(1) range + alignment check promoted to a public predicate here), [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 (the throwing `allocate` the Cpp17Allocator requirements demand), [ADR-0017](0017-typed-pool-design.md) (the sibling typed surface; the alignment ceiling is shared), [`docs/patterns/README.md`](../patterns/README.md) (catalogue updated in the same PR), [ROADMAP](../../ROADMAP.md) §3.3 (this ADR's roadmap item) + §3.5 (the container test battery that consumes this adapter).

## Context

Milestone 3.3 adopts the **Adapter** pattern: `it::d4np::memorypool::PoolAllocator<T>`, an allocator satisfying the C++17 *Cpp17Allocator* requirements ([allocator.requirements]) whose allocation substrate is the fixed-block pool. The adaptation gap is real on three axes:

1. **Interface shape.** Containers talk to allocators through `std::allocator_traits` — `value_type`, `allocate(n)` / `deallocate(p, n)`, rebinding, propagation traits, equality. The pool talks `memory_pool_alloc(pool)` / `memory_pool_free(pool, block)` — one block at a time, no count parameter, no type.
2. **Cardinality.** A fixed-block pool vends exactly one uniform slot per call. Containers ask for `n` contiguous elements: node-based containers (`std::list`, `std::map`, `std::set`) ask for `n == 1` essentially always; `std::vector` and `std::deque` ask for `n > 1` as they grow. A pure pool-backed `allocate(n > 1)` is unimplementable without contradicting the fixed-block contract.
3. **Rebinding.** `std::list<int, PoolAllocator<int>>` never allocates `int`s — it rebinds to `PoolAllocator<ListNode<int>>` and allocates nodes. The same pool handle must serve every rebound instantiation, whose `sizeof` the pool cannot know at creation time.

Additionally, routing a pointer back on `deallocate` requires answering *"did this come from the pool or from the fallback?"* — exactly the O(1) range + alignment predicate ADR-0012 already implements privately for `memory_pool_free`'s no-op guard.

## Decision

### 1. Hybrid node-allocator semantics: pool fast path, heap fallback

`PoolAllocator<T>::allocate(n)` serves from the pool **iff** `n == 1` and `sizeof(T) <= memory_pool_block_size(handle)`; every other request — bulk (`n != 1`), oversized `T`, or a pool-exhausted single-element request — falls back to `::operator new`. `deallocate(p, n)` routes by **provenance, not bookkeeping**: if the new public predicate `memory_pool_owns(handle, p)` reports the pointer as a pool slot it goes back via `memory_pool_free`, otherwise via `::operator delete`.

The allocator is therefore **total**: it never fails because of pool capacity or pool block-size limits, only on genuine OOM (`std::bad_alloc`, per ADR-0016's throwing-verb convention which [allocator.requirements] mandates). The pool is a *fast path for node-shaped allocations* — its home turf — not a capacity contract. Node-based containers draw their nodes from the pool until it is exhausted and degrade gracefully to the heap after; `std::vector`'s contiguous arrays go to the heap from the start, by design.

The practical sizing rule is documented on the class: create the pool's `block_size` for the **node type**, not the element type (64 bytes comfortably covers every Tier-1 `std::list` / `std::map` node for small elements); a pool sized too small simply routes everything to the heap — correct, observable through `memory_pool_owns`, never UB.

### 2. Two new public C introspection functions back the routing

```c
size_t memory_pool_block_size(const memory_pool_t* pool); /* 0 on NULL    */
int    memory_pool_owns(const memory_pool_t* pool, const void* block);
                                                          /* 1 iff in-range slot boundary */
```

`memory_pool_owns` is the ADR-0012 `is_block_in_range` check **promoted to public API** — same `std::uintptr_t` comparison, same O(1) cost, `0` on `NULL` pool / `NULL` block. It reports *address ownership* (the address lies inside the pool's backing at a slot boundary), **not** allocation state — it cannot distinguish a live slot from a free one, exactly like ADR-0012's policy. `memory_pool_block_size` exposes the configured slot size (`0` on `NULL`). Both are held to the C89 contract (ROADMAP §1.10) and exercised by `c_consumer_min.c`; both fit the existing NULL-tolerance posture of the C surface.

### 3. Non-owning handle; pointer-identity propagation traits

`PoolAllocator<T>` holds a single non-owning `memory_pool_t*` — `sizeof(PoolAllocator<T>) == sizeof(void*)`, trivially copyable, constructible from a `Pool&` (or a raw handle for C interop). **The pool must outlive every container using the allocator**; the same lifetime rule every arena/monotonic allocator in industry practice imposes (`std::pmr` has the identical contract).

The propagation traits follow pointer identity:

| Trait | Value | Rationale |
|-------|-------|-----------|
| `propagate_on_container_copy_assignment` | `true_type` | pointer copy is free; keeps source and target containers deallocation-correct against their own memory |
| `propagate_on_container_move_assignment` | `true_type` | enables O(1) container move-assign (no element-wise reallocation) |
| `propagate_on_container_swap` | `true_type` | enables O(1) `swap`; avoids the UB the standard reserves for swapping containers with non-swapping unequal allocators |
| `is_always_equal` | `false_type` | two allocators are interchangeable iff they reference the same pool |

Equality is handle identity, across rebound types: `PoolAllocator<T> == PoolAllocator<U>` iff same `memory_pool_t*`. Rebound copies share the handle; the per-type fit decision happens at `allocate` time against `memory_pool_block_size` (Decision 1), which is what makes rebinding sound on a pool whose slot size is fixed at creation.

The alignment ceiling is shared with ADR-0017: `static_assert(alignof(T) <= alignof(std::max_align_t))` — both the pool slots (ADR-0009 §5) and the plain `::operator new` fallback guarantee exactly fundamental alignment.

## Alternatives Considered

- **Strict pool-only allocator (`bad_alloc` on `n != 1` or exhaustion).** Rejected as the default. It turns a container's internal growth pattern into a runtime failure the caller cannot reason about locally (`std::list::sort`'s temporaries, `unordered_map` bucket arrays…), and M3.5's `std::vector` battery would be untestable. A strict variant remains a candidate knob for the M6 instrumentation wave if a use case emerges; the hybrid's routing is observable today via `memory_pool_owns`.
- **Per-rebind pool instances (each `PoolAllocator<U>` creates a `U`-sized pool).** Rejected. Allocator copies must be cheap and equality-meaningful ([allocator.requirements] requires copies to be interchangeable); spawning pools inside an allocator makes copies stateful and expensive, wrecks `is_always_equal`-adjacent reasoning, and hides resource acquisition inside container plumbing.
- **Owning allocator (`shared_ptr<Pool>`).** Rejected. Doubles the allocator's footprint, adds atomic refcount traffic to a hot path, and inverts the arena idiom — allocator users (containers) would collectively own the arena, making pool lifetime emergent instead of architectural. The `std::pmr` non-owning precedent is the industry-settled answer.
- **Bookkeeping-based deallocate routing (per-allocation tag or side table).** Rejected. A side table is O(N) metadata against spec §3.2 / ADR-0015; a header tag per allocation breaks the fixed-slot layout and the ADR-0009 §5 alignment economics. The ADR-0012 range check already answers provenance in O(1) with zero storage — it only needed to be public.
- **Tracking-and-asserting `n` on deallocate.** Rejected. `memory_pool_owns` already proves provenance; trusting `n` adds nothing (the standard guarantees `deallocate(p, n)` receives the same `n` as the matching `allocate`), and the pool path ignores `n` structurally since only `n == 1` is ever pool-served.
- **`std::pmr::memory_resource` adapter instead of a classic allocator.** Deferred, not rejected. `std::pmr` is C++17 and a natural second adapter (a `memory_resource` view over the pool would compose with every `pmr` container at zero extra design cost), but the roadmap item and the M3.5 battery name the classic allocator-template shape. A `pmr` view is a candidate for the Milestone 7 polish wave; recorded in the patterns catalogue as a consideration, not silently dropped.

## Consequences

**Positive**

- The pool composes with the entire standard container ecosystem through `std::allocator_traits` — the Adapter's textbook payoff. Node-based containers get O(1) pool-backed node churn with zero container-code changes.
- Provenance-based routing makes the hybrid **observable and testable**: the M3.3/M3.5 tests prove "list nodes really came from the pool" by exhausting the pool through the container and watching `memory_pool_owns` flip on the next probe.
- The two new C functions are generally useful introspection (capacity planning, debug assertions in consumer code), not adapter-private plumbing — and they keep the C89 contract.
- Graceful degradation under exhaustion means adopting the allocator is never a correctness risk — worst case is heap-speed allocation, the exact behaviour the container had before.

**Negative**

- The heap fallback can mask a mis-sized pool (everything silently routes to `::operator new`). Mitigation: `memory_pool_owns` makes the routing observable; the M6 Decorator/Observer wave is the natural home for a fallback counter that turns "silently" into "measurably".
- `deallocate` pays the O(1) owns-check (a handful of integer ops) on every call — the same cost class `memory_pool_free` already pays for ADR-0012, now incurred once more in the adapter layer.
- The pool-must-outlive-containers lifetime rule is the caller's burden, enforced by documentation rather than the type system (the cost of the non-owning design; the owning alternative was rejected above).

**Required documentation updates landing in the same PR as this ADR**

- [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) — Doxygen for the two new functions, C89-held.
- [`docs/patterns/README.md`](../patterns/README.md) — Adapter row added to *Adopted / Planned* (status `Implemented`); candidate-table annotation.
- [`docs/adr/README.md`](README.md) — index row for ADR-0018.
- [`ROADMAP.md`](../../ROADMAP.md) §3.3 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M3.3)* entry.

[`pool_allocator.hpp`](../../src/main/cpp/it/d4np/memorypool/pool_allocator.hpp), [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp), [`pool_allocator_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_allocator_test.cpp), [`pool_smoke_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp) (C-surface cases for the two new functions), and [`c_consumer_min.c`](../../src/test/c/it/d4np/memorypool/c_consumer_min.c) land the implementation and tests.

## References

- ISO C++17 [allocator.requirements] — the Cpp17Allocator contract this adapter satisfies.
- [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) — the O(1) provenance predicate promoted to `memory_pool_owns`.
- [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 — throwing `allocate` per the allocator requirements.
- `std::pmr::polymorphic_allocator` / `std::pmr::monotonic_buffer_resource` — the non-owning arena-lifetime precedent.
- Boost.Pool `pool_allocator` — prior art for hybrid pool/heap allocator semantics.
