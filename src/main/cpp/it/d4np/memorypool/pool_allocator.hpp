// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_POOL_ALLOCATOR_HPP_
#define IT_D4NP_MEMORYPOOL_POOL_ALLOCATOR_HPP_

/**
 * @file pool_allocator.hpp
 * @brief STL-compatible allocator over the pool — the Adapter pattern,
 *        ADR-0018.
 *
 * `PoolAllocator<T>` satisfies the C++17 Cpp17Allocator requirements and
 * adapts the one-fixed-block-at-a-time pool interface to the
 * `allocate(n)` / rebind / propagation contract containers consume
 * through `std::allocator_traits`.
 *
 * Hybrid node-allocator semantics (ADR-0018 §1): single-element requests
 * that fit a slot are served by the pool; bulk (`n != 1`), oversized, or
 * pool-exhausted requests fall back to `::operator new`. Deallocation
 * routes by provenance via ::memory_pool_owns — O(1), zero bookkeeping.
 */

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>

namespace it::d4np::memorypool {

/**
 * @brief Cpp17Allocator adapting the fixed-block pool for standard
 *        containers.
 *
 * Holds a single **non-owning** `memory_pool_t*` — `sizeof(PoolAllocator)
 * == sizeof(void*)`, trivially copyable. The referenced pool must outlive
 * every container (and every copy of the allocator) using it — the same
 * arena-lifetime contract as `std::pmr` (ADR-0018 §3).
 *
 * Sizing guidance: containers allocate their *node* type, not `T` —
 * create the pool's `block_size` for the node (64 bytes comfortably
 * covers every Tier-1 `std::list` / `std::map` node for small elements).
 * A pool sized too small is never an error: requests that do not fit
 * route to the heap, observably via ::memory_pool_owns.
 *
 * Equality is handle identity across rebound types; the propagation
 * traits follow pointer identity so container copy-assign / move-assign /
 * swap are O(1) and deallocation-correct (ADR-0018 §3).
 *
 * @tparam T Element type. Must not be over-aligned: both the pool slots
 *           (ADR-0009 §5) and the `::operator new` fallback guarantee
 *           exactly `alignof(std::max_align_t)`.
 */
template <typename T>
class PoolAllocator {
    static_assert(!std::is_void_v<T>, "PoolAllocator<T>: T must be an object type");
    static_assert(!std::is_reference_v<T>, "PoolAllocator<T>: T must be an object type, not a reference");
    static_assert(alignof(T) <= alignof(std::max_align_t),
                  "PoolAllocator<T>: over-aligned types are not supported — pool slots and the "
                  "::operator new fallback guarantee alignof(std::max_align_t) only (ADR-0018 §3)");

public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    /**
     * Adapt @p pool. Non-owning: @p pool must outlive this allocator,
     * every copy of it, and every container using any of them.
     */
    explicit PoolAllocator(Pool& pool) noexcept : handle_(pool.native_handle()) {}

    /**
     * Adapt a raw C handle (interop path). Same non-owning lifetime
     * contract as the `Pool&` overload. A null handle is permitted and
     * routes every request to the heap fallback.
     */
    explicit PoolAllocator(memory_pool_t* handle) noexcept : handle_(handle) {}

    /** Rebinding conversion — shares the same pool handle (ADR-0018 §3). */
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor) — converting ctor required by Cpp17Allocator rebinding
    PoolAllocator(const PoolAllocator<U>& other) noexcept : handle_(other.native_handle()) {}

    /**
     * Allocate storage for @p count objects of `T` — throwing verb per
     * [allocator.requirements] and ADR-0016 §2.
     *
     * Pool fast path iff `count == 1` and `sizeof(T)` fits the pool's
     * slot size; every other shape — bulk, oversized `T` (e.g. after
     * rebinding), exhausted or null pool — falls back to
     * `::operator new` (ADR-0018 §1). The allocator is total: pool
     * limits never fail an allocation, only genuine OOM does.
     *
     * @throw std::bad_alloc on `count * sizeof(T)` overflow or heap OOM.
     */
    [[nodiscard]] T* allocate(std::size_t count) {
        if (count == 1U && sizeof(T) <= memory_pool_block_size(handle_)) {
            void* const slot = memory_pool_alloc(handle_);
            if (slot != nullptr) {
                return static_cast<T*>(slot);
            }
        }
        if (count > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
            throw std::bad_alloc{};
        }
        return static_cast<T*>(::operator new(count * sizeof(T)));
    }

    /**
     * Release storage obtained from ::allocate on an equal allocator.
     *
     * Routes by provenance (ADR-0018 §1): a pointer ::memory_pool_owns
     * reports as a pool slot returns via ::memory_pool_free, anything
     * else via `::operator delete`. @p count is structurally redundant —
     * only `count == 1` requests are ever pool-served — and is ignored.
     */
    void deallocate(T* storage, std::size_t count) noexcept {
        static_cast<void>(count);
        if (memory_pool_owns(handle_, storage) != 0) {
            memory_pool_free(handle_, storage);
            return;
        }
        ::operator delete(storage);
    }

    /** @return The adapted (non-owned) C handle — also the equality key. */
    [[nodiscard]] memory_pool_t* native_handle() const noexcept {
        return handle_;
    }

private:
    memory_pool_t* handle_;
};

/** Allocators compare equal iff they adapt the same pool (ADR-0018 §3). */
template <typename T, typename U>
[[nodiscard]] bool operator==(const PoolAllocator<T>& lhs, const PoolAllocator<U>& rhs) noexcept {
    return lhs.native_handle() == rhs.native_handle();
}

/** @copydoc operator==(const PoolAllocator<T>&, const PoolAllocator<U>&) */
template <typename T, typename U>
[[nodiscard]] bool operator!=(const PoolAllocator<T>& lhs, const PoolAllocator<U>& rhs) noexcept {
    return !(lhs == rhs);
}

}  // namespace it::d4np::memorypool

#endif  // IT_D4NP_MEMORYPOOL_POOL_ALLOCATOR_HPP_
