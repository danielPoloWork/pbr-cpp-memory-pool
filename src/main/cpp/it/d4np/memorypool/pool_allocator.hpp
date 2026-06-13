// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_POOL_ALLOCATOR_HPP_
#define IT_D4NP_MEMORYPOOL_POOL_ALLOCATOR_HPP_

/**
 * @file pool_allocator.hpp
 * @brief STL-compatible allocator Adapter over `Pool` — ADR-0018.
 *
 * `PoolAllocator<T>` satisfies the *Cpp17Allocator* requirements so that
 * standard containers can draw their storage from a `Pool` (ADR-0010). It
 * is the structural **Adapter** pattern: it bridges the pool's
 * fixed-block `void*` interface to the variable-size
 * `std::allocator_traits` contract every container expects.
 *
 * The adapter is a non-owning back-reference to a `Pool` (single `Pool*`
 * member, `sizeof == sizeof(void*)`): the pool is owned elsewhere and
 * **must out-live every container and every adapter copy that references
 * it** — the same lifetime contract `std::pmr::polymorphic_allocator`
 * places on its `memory_resource`.
 *
 * Header-only by necessity (class template); adds zero object code to the
 * static library and zero per-pool metadata (ADR-0015 unaffected).
 */

#include <it/d4np/memorypool/memory_pool.hpp>

#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>

namespace it::d4np::memorypool {

/**
 * @brief STL-compatible allocator vending storage from a `Pool`.
 *
 * **Routing (ADR-0018 §2).** A request routes to the pool **iff** it is a
 * single block that fits — `n == 1`, `sizeof(T) <= pool.block_size()`, and
 * `T` is not over-aligned. Such requests are served by `Pool::allocate`
 * (O(1), throwing `std::bad_alloc` on exhaustion per ADR-0016 §2). Every
 * other request — `n > 1`, an over-aligned `T`, or a rebound node larger
 * than the block — is delegated to over-aligned `::operator new` /
 * `::operator delete`.
 *
 * Because the standard guarantees `deallocate(p, n)` is called with the
 * same `n` (and on the same allocator type, hence the same `sizeof(T)` /
 * `alignof(T)`) as the matching `allocate(n)`, and `block_size()` is
 * invariant for the pool's lifetime, the routing predicate evaluates
 * identically at allocate and deallocate time. Every pointer is therefore
 * freed by exactly the path that allocated it, with no per-pointer
 * bookkeeping.
 *
 * As a result a single container may, over its lifetime, hold storage from
 * both sources (e.g. a `std::vector` whose `n == 1` initial capacity came
 * from the pool and whose grown buffer came from the heap) — correct, if
 * occasionally surprising.
 *
 * **Propagation (ADR-0018 §4).** All three `propagate_on_container_*`
 * traits are `false_type`; the adapter is stateful (`is_always_equal` is
 * `false_type`) and two instances compare equal iff they reference the
 * same `Pool`.
 *
 * @tparam T Element type the allocator vends. Rebinding to any `U` (e.g. a
 *           container's internal node type) is the single-template-parameter
 *           default supplied by `std::allocator_traits`.
 */
template <typename T>
class PoolAllocator {
public:
    using value_type = T;

    // Propagation traits — ADR-0018 §4. All three `false`: memory routing
    // is tied to (n, T) and to the pool a pointer came from, never to
    // allocator-identity propagation across container copy / move / swap.
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    // Stateful: two adapters are interchangeable only if they share a pool.
    using is_always_equal = std::false_type;

    /**
     * Bind the adapter to @p pool. The pool is **not** owned and must
     * out-live this adapter, every copy of it, and every container that
     * uses it (ADR-0018 §1).
     */
    explicit PoolAllocator(Pool& pool) noexcept : pool_(&pool) {}

    /**
     * Rebinding converting constructor — required by the *Cpp17Allocator*
     * requirements so a container can build an allocator for its internal
     * node type from the user-supplied `PoolAllocator<T>`. Deliberately
     * implicit (the rebind machinery relies on the implicit conversion);
     * it is a trivial back-reference copy.
     */
    template <typename U>
    PoolAllocator(const PoolAllocator<U>& other) noexcept : pool_(other.pool_) {}

    /**
     * Allocate storage for @p n objects of `T` (ADR-0018 §2).
     *
     * @return Pointer to uninitialized storage for @p n contiguous `T`.
     * @throw std::bad_alloc on pool exhaustion (pool-eligible requests), on
     *        `size_t` overflow of `n * sizeof(T)`, or from the fallback
     *        `::operator new`.
     */
    [[nodiscard]] T* allocate(std::size_t n) {
        if (routes_to_pool(n)) {
            void* const block = pool_->try_allocate();
            if (block == nullptr) {
                throw std::bad_alloc{};
            }
            return static_cast<T*>(block);
        }
        return allocate_fallback(n);
    }

    /**
     * Return storage obtained from ::allocate. @p n must be the value
     * passed to the matching ::allocate call (a *Cpp17Allocator*
     * guarantee), which keeps the pool/fallback routing deterministic.
     */
    void deallocate(T* ptr, std::size_t n) noexcept {
        if (routes_to_pool(n)) {
            pool_->deallocate(ptr);
            return;
        }
        deallocate_fallback(ptr);
    }

    /** Two adapters are equal iff they reference the same pool. */
    template <typename U>
    [[nodiscard]] bool operator==(const PoolAllocator<U>& rhs) const noexcept {
        return pool_ == rhs.pool_;
    }

    /** Negation of ::operator==. */
    template <typename U>
    [[nodiscard]] bool operator!=(const PoolAllocator<U>& rhs) const noexcept {
        return !(*this == rhs);
    }

private:
    // Rebound specialisations are mutual friends so the converting ctor and
    // operator== can read each other's private back-reference.
    template <typename U>
    friend class PoolAllocator;

    /** ADR-0018 §2 routing predicate — pure in (n, sizeof(T), alignof(T), block_size). */
    [[nodiscard]] bool routes_to_pool(std::size_t n) const noexcept {
        return n == 1U && sizeof(T) <= pool_->block_size() && alignof(T) <= alignof(std::max_align_t);
    }

    /** Over-aligned heap fallback for requests the pool cannot serve. */
    [[nodiscard]] static T* allocate_fallback(std::size_t n) {
        if (n > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
            // n * sizeof(T) would overflow size_t — ADR-0009 §3 guard,
            // applied here to the fallback path.
            throw std::bad_alloc{};
        }
        const std::size_t bytes = n * sizeof(T);
        // Over-aligned ::operator new so the fallback honours alignof(T)
        // even for over-aligned T (the case routes_to_pool excludes from
        // the pool). cppcoreguidelines-owning-memory misreads the returned
        // storage as a leaked resource; deallocate_fallback closes it.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        return static_cast<T*>(::operator new(bytes, std::align_val_t{alignof(T)}));
    }

    /** Matching sized/aligned release for ::allocate_fallback storage. */
    static void deallocate_fallback(T* ptr) noexcept {
        // Matching aligned ::operator delete for the over-aligned new
        // above — C++17 [expr.delete] requires the same alignment argument.
        // The T* -> void* conversion to the deallocation function is
        // implicit and unambiguous, so no explicit cast is needed.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        ::operator delete(ptr, std::align_val_t{alignof(T)});
    }

    Pool* pool_;
};

}  // namespace it::d4np::memorypool

#endif  // IT_D4NP_MEMORYPOOL_POOL_ALLOCATOR_HPP_
