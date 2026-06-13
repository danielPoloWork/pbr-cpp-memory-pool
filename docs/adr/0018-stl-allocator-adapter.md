# ADR-0018: STL-Compatible Allocator Adapter — Non-Owning Back-Reference, Deterministic Pool/Fallback Routing, and Propagation Traits

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§5 (the fixed `block_size` and the `alignof(std::max_align_t)` guarantee that bound which requests the pool can serve), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) (the `Pool` the adapter references), [ADR-0011](0011-factory-method-and-builder-for-pool-construction.md) (the construction surface the adapter does **not** own), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the `memory_pool_free` range check the adapter relies on for the pool path), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the introspection accessor pattern the new `memory_pool_block_size` mirrors), [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 (the `std::bad_alloc`-on-exhaustion verb the adapter forwards to), [ROADMAP](../../ROADMAP.md) §3.3 (this ADR's roadmap item) + §3.5 (the container integration tests this adapter unblocks).

## Context

Milestone 3.3 adds the **Adapter** pattern: an STL-compatible allocator, `it::d4np::memorypool::PoolAllocator<T>`, satisfying the *Cpp17Allocator* requirements so that standard containers can draw their storage from a pool. The didactic value is twofold — it is the canonical structural Adapter (bridging the pool's `void*`-block interface to the `std::allocator_traits` contract every container expects), and it surfaces the central tension of a **fixed-block** allocator behind a **variable-size** interface.

That tension is the crux of the design. A standard container calls `allocator::allocate(n)` for `n` contiguous `value_type` objects. A node-based container (`std::list`, `std::map`, `std::set`, `std::unordered_*`) allocates exactly one node at a time — `n == 1` — which is the pool's natural unit. A contiguous container (`std::vector`, `std::deque`'s blocks) requests `n > 1`, which a single fixed block cannot satisfy. The adapter must be a *correct, conforming allocator for every container* while only being *fast for the single-block case*.

Four questions need answers before the template is written:

1. **What does the allocator own?** A *Cpp17Allocator* must be copyable and is freely copied/rebound by containers. The `Pool` (ADR-0010) is move-only — it cannot be the allocator's value member.
2. **How are requests the pool cannot serve handled?** `n > 1`, an over-aligned `T`, or a rebound node larger than the pool's block must not corrupt memory.
3. **How does `deallocate` know which path freed each pointer** without per-pointer bookkeeping?
4. **What are the propagation traits**, and what do they imply for container copy / move / swap?

## Decision

### 1. The adapter is a non-owning back-reference to a `Pool`

`PoolAllocator<T>` holds a single `Pool* pool_` data member — a *non-owning* back-reference. The pool is owned elsewhere (by the caller, on the stack or in a longer-lived scope) and **must out-live every container that uses the adapter and every copy of the adapter** — the same lifetime contract `std::pmr::polymorphic_allocator` places on its `memory_resource`. Construction is from a `Pool&`:

```cpp
explicit PoolAllocator(Pool& pool) noexcept;
```

Copy and the rebinding converting constructor are trivial pointer copies; `sizeof(PoolAllocator<T>) == sizeof(void*)`. This resolves Q1: the adapter is freely copyable and rebindable precisely because it owns nothing.

### 2. Deterministic pool/fallback routing — no per-pointer bookkeeping

A request routes to the pool **iff** it is a single block that fits:

```cpp
bool routes_to_pool(std::size_t n) const noexcept {
    return n == 1U
        && sizeof(T) <= pool_->block_size()
        && alignof(T) <= alignof(std::max_align_t);
}
```

- **Pool-eligible** (`routes_to_pool(n)` true): `allocate` forwards to `Pool::try_allocate` and throws `std::bad_alloc` on exhaustion (ADR-0016 §2); `deallocate` forwards to `Pool::deallocate`.
- **Everything else**: `allocate` delegates to over-aligned `::operator new(n * sizeof(T), std::align_val_t{alignof(T)})` (with a `size_t`-overflow guard, mirroring ADR-0009 §3); `deallocate` to the matching sized/aligned `::operator delete`.

The key insight resolving Q3: the standard guarantees `deallocate(p, n)` is called with the **same `n`** that the matching `allocate(n)` received, and on the **same allocator type** (hence the same `sizeof(T)` / `alignof(T)`). `routes_to_pool` is a pure function of `(n, sizeof(T), alignof(T), pool_->block_size())` — and `block_size()` is invariant for a pool's lifetime — so the predicate evaluates **identically** at `allocate` and `deallocate`. Every pointer is therefore freed by exactly the path that allocated it, with **zero per-pointer bookkeeping** and no need to range-check against the pool at the C++ layer (the pool path's `memory_pool_free` already runs the ADR-0012 check defensively).

This makes `PoolAllocator<T>` a conforming allocator for *every* container (Q2): `std::list` runs entirely on the pool fast path, `std::vector` runs almost entirely on the fallback, and both are memory-correct.

### 3. A new introspection accessor: `memory_pool_block_size`

`routes_to_pool` needs the pool's configured block size at runtime, but `block_size_` lives behind the ADR-0010 Pimpl boundary. This ADR adds the C accessor

```c
size_t memory_pool_block_size(const memory_pool_t* pool);  /* 0 when pool is NULL */
```

the exact introspection-companion shape of `memory_pool_metadata_bytes` (ADR-0015) — `O(1)`, `NULL`-tolerant, held to the same ANSI-C C89 contract and exercised by `c_consumer_min.c`. A `[[nodiscard]] std::size_t Pool::block_size() const noexcept` forwarder is added in lock-step. The accessor is additive (a new symbol; no ABI break) and is the principled fix for the "is this `T` safe to put in a pool block?" question — a documented runtime guard instead of a silent buffer overflow when a rebound node exceeds the block.

### 4. Propagation traits and equality

| Trait | Value | Why |
|-------|-------|-----|
| `propagate_on_container_copy_assignment` | `std::false_type` | A copy-assigned container keeps its own allocator; if the two pools differ, the standard performs element-wise assignment into the target's pool — never frees through the wrong pool. |
| `propagate_on_container_move_assignment` | `std::false_type` | Same reasoning for move-assignment. When the allocators compare **equal** (same pool) the buffer can still be adopted cheaply; when they differ, elements move individually — always safe. |
| `propagate_on_container_swap` | `std::false_type` | Swapping two containers does **not** swap their pool references; each keeps freeing through the pool it allocated from. |
| `is_always_equal` | `std::false_type` | The adapter is stateful — two instances are interchangeable only if they reference the same pool. |

`operator==` is `true` iff the two adapters (of any rebound element types) hold the **same `Pool*`**; `operator!=` is its negation. `select_on_container_copy_construction` is left at the `std::allocator_traits` default (returns a copy of the allocator), so a copied container keeps using the same pool — the intended behaviour for a shared resource.

Choosing all three `propagate_on_*` traits `false` is the conservative, always-correct posture: memory routing is tied to `(n, T)` and to the pool a pointer came from, never to allocator *identity propagation* across container operations. It mirrors `std::pmr`'s "do not propagate" stance, adjusted only in `select_on_container_copy_construction` (we retain the back-reference rather than reverting to a default resource, because the whole point is to keep using the pool).

## Alternatives Considered

- **Owning allocator (the allocator holds the `Pool`).** Rejected. A *Cpp17Allocator* is copied and rebound freely by every container; an owning, move-only `Pool` cannot be a copyable allocator's member, and copying the backing storage per rebind is absurd. The non-owning back-reference is the only shape that satisfies the requirements.
- **No fallback — throw on every non-single-block request.** Rejected. It makes the adapter usable only with node-based containers and turns the M3.5 `std::vector` test into a demonstration of failure. The fallback costs nothing on the pool fast path and makes the adapter universally conforming.
- **Fall back to `::operator new` on pool *exhaustion* too.** Rejected. It breaks the deterministic-routing invariant of §2: a pool-eligible `n == 1` pointer could then have come from *either* the pool or the heap, so `deallocate` could no longer decide the path from `(n, T)` alone — forcing per-pointer bookkeeping (a side table or range probe on every free). Exhaustion of a pool-eligible request therefore throws `std::bad_alloc`, consistent with ADR-0016 §2.
- **Per-pointer bookkeeping (side table / tagged pointers) to track the allocation path.** Rejected. Unnecessary given §2's deterministic predicate, and it would add per-allocation time and space overhead that defeats the pool's reason to exist.
- **Document the block-size fit as caller responsibility instead of adding `memory_pool_block_size`.** Rejected. A rebound node silently overflowing an undersized block is memory corruption, not a documented no-op like ADR-0012's foreign pointer. The enterprise quality bar (warnings-as-errors, sanitizer-clean) does not tolerate a known UB path that a one-line `O(1)` accessor closes.
- **Propagate-on-* all `true` (steal buffers aggressively on assignment/swap).** Rejected. With a non-owning reference and path-dependent freeing, propagating allocator identity across container operations creates cases where memory allocated under one pool is freed under another. `false` across the board is the only always-safe choice for this resource model.
- **`std::pmr::memory_resource` subclass instead of a typed allocator.** Deferred, not rejected outright. A `pmr` resource would let one pool back many `pmr` containers without the template rebind dance, but it commits to the `pmr` virtual-dispatch model and a different ABI surface; it is a candidate for a future ergonomics milestone, recorded here so the door stays open.

## Consequences

**Positive**

- Standard node-based containers (`std::list`, `std::map`, `std::set`) run entirely on the pool's O(1) fast path; `std::vector` and friends remain correct via the fallback. This unblocks the M3.5 container test matrix.
- The deterministic routing predicate (§2) gives correct `deallocate` routing with zero per-pointer overhead — the adapter adds nothing to the hot path beyond a couple of compile-time-foldable comparisons and one `block_size()` load.
- `memory_pool_block_size` rounds out the introspection surface alongside `memory_pool_metadata_bytes`, and turns a potential silent-overflow footgun into a checked, documented runtime decision.
- The adapter is header-only (a class template): zero library-ABI impact beyond the additive C accessor, and zero metadata-budget impact (ADR-0015 untouched).

**Negative**

- The pool/fallback split means a single `std::vector` can hold storage from two sources over its lifetime (pool for an early `n == 1` capacity, heap after it grows). This is correct but can surprise someone expecting "every byte came from the pool"; it is documented on the class.
- The non-owning back-reference puts a hard lifetime contract on the caller: the `Pool` must out-live every container and every adapter copy. Misuse is a dangling pointer, not a diagnosed error. (A debug-build liveness check belongs to the Milestone 6 instrumentation wave, alongside double-free detection deferred by ADR-0012.)
- Sizing a pool for a node-based container requires knowing the rebound node size, which is implementation-defined. The `routes_to_pool` size check degrades safely (an undersized pool simply routes everything to the fallback rather than corrupting memory), but a too-small pool silently yields no speed-up. M3.5 documents the sizing recipe.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0018.
- [`docs/patterns/README.md`](../patterns/README.md) — new *Adopted* row for **Adapter** (status `Implemented`).
- [`ROADMAP.md`](../../ROADMAP.md) §3.3 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M3.3)* entry.

[`pool_allocator.hpp`](../../src/main/cpp/it/d4np/memorypool/pool_allocator.hpp) lands the template; [`pool_allocator_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_allocator_test.cpp) lands its dedicated doctest binary (registered with CTest as `pool_allocator`). The new `memory_pool_block_size` accessor lands in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) / [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) with the `Pool::block_size()` forwarder in [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp), and is exercised under the C89/C99 jobs by [`c_consumer_min.c`](../../src/test/c/it/d4np/memorypool/c_consumer_min.c).

## References

- ISO C++17 [allocator.requirements] — the *Cpp17Allocator* contract (`allocate`, `deallocate`, `value_type`, rebinding, equality, propagation traits).
- ISO C++17 [allocator.traits] — `std::allocator_traits` defaults (`max_size`, `construct`, `destroy`, `select_on_container_copy_construction`, rebind via the first template parameter).
- ISO C++17 [container.requirements.general] — the guarantee that `deallocate` receives the same size argument as the matching `allocate`, and the role of the propagation traits in container copy / move / swap.
- [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 — the `std::bad_alloc`-on-exhaustion verb the pool path forwards to.
- [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) — the introspection-accessor precedent `memory_pool_block_size` follows.
