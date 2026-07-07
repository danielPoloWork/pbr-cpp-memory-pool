// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_POOL_MEMORY_RESOURCE_HPP_
#define IT_D4NP_MEMORYPOOL_POOL_MEMORY_RESOURCE_HPP_

/**
 * @file pool_memory_resource.hpp
 * @brief `std::pmr::memory_resource` Adapter over `Pool` — ADR-0042.
 *
 * `PoolMemoryResource` is the second **Adapter** the library ships (the
 * sibling of the `PoolAllocator<T>` Adapter in ADR-0018). Where
 * `PoolAllocator<T>` bridges the pool to the compile-time
 * `std::allocator_traits` contract — one rebound allocator type per element
 * type — `PoolMemoryResource` bridges the pool to the **runtime**
 * `std::pmr::memory_resource` interface, so a *single* resource can back any
 * number of heterogeneous `std::pmr` containers through
 * `std::pmr::polymorphic_allocator` without the per-type rebind dance. This is
 * exactly the "door left open" in ADR-0018's alternatives.
 *
 * The adapter is a **non-owning** back-reference: it holds a `Pool*` and an
 * upstream `std::pmr::memory_resource*`. The pool (and the upstream) must
 * out-live this resource and every `polymorphic_allocator` / container bound
 * to it — the same lifetime contract `std::pmr::polymorphic_allocator` places
 * on its resource, and `PoolAllocator<T>` on its `Pool`.
 *
 * Header-only; adds zero object code to the static library and zero per-pool
 * metadata (ADR-0015 unaffected).
 *
 * **Availability.** `std::pmr` is a C++17 facility, but some standard
 * libraries shipped `<memory_resource>` late and freestanding profiles omit
 * it. The whole adapter is therefore gated behind `PBR_MEMORY_POOL_HAS_PMR`,
 * which is `1` only when `<memory_resource>` and its
 * `__cpp_lib_memory_resource` feature-test macro are present. A consumer can
 * test that macro before relying on the type; where it is `0` this header is a
 * harmless no-op.
 */

#include <it/d4np/memorypool/memory_pool.hpp>

#if defined(__has_include)
#if __has_include(<memory_resource>)
#include <memory_resource>
#endif
#endif

// Availability gate (see the @file note). This drives `#if` here, in the
// dedicated test, and in consumer code, so it must be a preprocessor macro
// rather than a constexpr constant — hence the macro-usage suppression.
/* NOLINTBEGIN(cppcoreguidelines-macro-usage) */
#if defined(__cpp_lib_memory_resource) && __cpp_lib_memory_resource >= 201603L
#define PBR_MEMORY_POOL_HAS_PMR 1
#else
#define PBR_MEMORY_POOL_HAS_PMR 0
#endif
/* NOLINTEND(cppcoreguidelines-macro-usage) */

#if PBR_MEMORY_POOL_HAS_PMR

#include <cstddef>
#include <new>

namespace it::d4np::memorypool {

/**
 * @brief A `std::pmr::memory_resource` that draws single blocks from a `Pool`.
 *
 * **Routing** mirrors ADR-0018 §2, adapted to `std::pmr`'s
 * `(bytes, alignment)` interface (there is no element count `n` here — a
 * `memory_resource` request is a single contiguous region). A request routes
 * to the pool **iff** one pool block can satisfy it:
 *
 * @code
 *   bytes <= pool.block_size() && alignment <= alignof(std::max_align_t)
 * @endcode
 *
 * - **Pool-eligible:** served by `Pool::try_allocate`, throwing
 *   `std::bad_alloc` on exhaustion (ADR-0016 §2). It **never** falls back to
 *   the upstream on exhaustion — doing so would break the deterministic
 *   routing below (see ADR-0042).
 * - **Everything else** (over-sized or over-aligned): delegated to the
 *   `upstream` resource (`std::pmr::get_default_resource()` by default).
 *
 * Because `do_deallocate` receives the *same* `(bytes, alignment)` the
 * matching `do_allocate` received, and `block_size()` is invariant for the
 * pool's lifetime, the routing predicate evaluates identically at allocate and
 * deallocate time. Every pointer is therefore released through exactly the
 * path that produced it, with **zero per-pointer bookkeeping** — the same
 * insight that makes `PoolAllocator<T>` correct.
 *
 * Two resources compare equal (`is_equal`) iff they are both
 * `PoolMemoryResource` bound to the same `Pool` **and** the same upstream, so
 * a `polymorphic_allocator` can safely deallocate through either.
 */
class PoolMemoryResource : public std::pmr::memory_resource {
public:
    /**
     * Bind the resource to @p pool, delegating requests the pool cannot serve
     * to @p upstream. Neither the pool nor the upstream is owned; both must
     * out-live this resource and everything bound to it.
     *
     * @param pool     The fixed-block pool that serves pool-eligible requests.
     * @param upstream Resource for over-sized / over-aligned requests; defaults
     *                 to `std::pmr::get_default_resource()`.
     */
    explicit PoolMemoryResource(Pool& pool,
                                std::pmr::memory_resource* upstream = std::pmr::get_default_resource()) noexcept
        : pool_(&pool), upstream_(upstream) {}

    /** @return The bound pool (non-owning). */
    [[nodiscard]] Pool& pool() const noexcept {
        return *pool_;
    }

    /** @return The upstream resource requests fall back to (non-owning). */
    [[nodiscard]] std::pmr::memory_resource* upstream_resource() const noexcept {
        return upstream_;
    }

private:
    /**
     * Serve one allocation (ADR-0042). Pool-eligible requests come from the
     * pool (throwing `std::bad_alloc` on exhaustion); all others go upstream.
     *
     * @throw std::bad_alloc on pool exhaustion, or whatever the upstream
     *        resource throws.
     */
    // The `(bytes, alignment)` pair is the std::pmr::memory_resource override
    // signature and cannot be restructured into a parameter struct.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (routes_to_pool(bytes, alignment)) {
            void* const block = pool_->try_allocate();
            if (block == nullptr) {
                throw std::bad_alloc{};
            }
            return block;
        }
        return upstream_->allocate(bytes, alignment);
    }

    /**
     * Release storage obtained from ::do_allocate. @p bytes and @p alignment
     * must match the originating request (a `memory_resource` guarantee),
     * which keeps the pool / upstream routing deterministic.
     */
    // Fixed override signature — see ::do_allocate.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override {
        if (routes_to_pool(bytes, alignment)) {
            pool_->deallocate(ptr);
            return;
        }
        upstream_->deallocate(ptr, bytes, alignment);
    }

    /** Equal iff same concrete type bound to the same pool and upstream. */
    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        const auto* const rhs = dynamic_cast<const PoolMemoryResource*>(&other);
        return rhs != nullptr && rhs->pool_ == pool_ && rhs->upstream_ == upstream_;
    }

    /** ADR-0042 routing predicate — pure in (bytes, alignment, block_size). */
    // Mirrors the std::pmr `(bytes, alignment)` shape — see ::do_allocate.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] bool routes_to_pool(std::size_t bytes, std::size_t alignment) const noexcept {
        return bytes <= pool_->block_size() && alignment <= alignof(std::max_align_t);
    }

    Pool* pool_;
    std::pmr::memory_resource* upstream_;
};

}  // namespace it::d4np::memorypool

#endif  // PBR_MEMORY_POOL_HAS_PMR

#endif  // IT_D4NP_MEMORYPOOL_POOL_MEMORY_RESOURCE_HPP_
