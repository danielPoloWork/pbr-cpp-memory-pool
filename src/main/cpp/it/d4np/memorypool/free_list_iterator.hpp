// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_FREE_LIST_ITERATOR_HPP_
#define IT_D4NP_MEMORYPOOL_FREE_LIST_ITERATOR_HPP_

/**
 * @file free_list_iterator.hpp
 * @brief Read-only diagnostic Iterator over the pool's free list — ADR-0019.
 *
 * `FreeListIterator` is a LegacyForwardIterator that walks the implicit
 * free list (ADR-0009 §1) yielding the address of each free slot;
 * `FreeListView` is the range adaptor providing `begin()` / `end()`. The
 * traversal is delegated to the `memory_pool_debug_*` C accessors so the
 * free-list layout stays encapsulated behind the ADR-0010 Pimpl boundary.
 *
 * The whole facility is gated behind `PBR_MEMORY_POOL_DIAGNOSTICS`
 * (ADR-0019 §1) — available in debug builds, compiled out of release
 * builds unless explicitly enabled. Header-only; adds zero object code and
 * zero per-pool metadata.
 *
 * Diagnostics-only: walking the list is O(free_count) and must never be
 * used on the allocation hot path.
 */

#include <it/d4np/memorypool/memory_pool.h>

#if PBR_MEMORY_POOL_DIAGNOSTICS

#include <it/d4np/memorypool/memory_pool.hpp>

#include <cstddef>
#include <iterator>

namespace it::d4np::memorypool {

/**
 * @brief Read-only LegacyForwardIterator over the free-list slots.
 *
 * Dereferencing yields the address of a free slot as `const void*`. Because
 * that address is the iterator's own state, `operator*` returns a reference
 * to the stored member — so the type is a genuine forward iterator (with
 * the multi-pass guarantee), not an input-only proxy. The default-
 * constructed iterator (`current_ == nullptr`) is the end sentinel.
 *
 * Never mutate the storage a dereferenced pointer addresses — the surface
 * is read-only by construction (ADR-0019 §3).
 */
class FreeListIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = const void*;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    /** Construct the end sentinel. */
    FreeListIterator() noexcept = default;

    /** Construct an iterator positioned at @p current within @p pool's list. */
    FreeListIterator(const memory_pool_t* pool, const void* current) noexcept : pool_(pool), current_(current) {}

    /** @return The current free-slot address (by reference to iterator state). */
    [[nodiscard]] reference operator*() const noexcept {
        return current_;
    }

    /** @return Address of the current-slot-address member. */
    [[nodiscard]] pointer operator->() const noexcept {
        return &current_;
    }

    /** Advance to the next free slot (ADR-0019 §2). */
    FreeListIterator& operator++() noexcept {
        current_ = ::memory_pool_debug_free_list_next(pool_, current_);
        return *this;
    }

    /**
     * Post-increment; returns the pre-advance value. The `const` return is
     * the cert-dcl21-cpp convention (prevents `(it++)++` misuse); the
     * readability-const-return-type check disagrees, so it is suppressed.
     */
    // NOLINTNEXTLINE(readability-const-return-type)
    const FreeListIterator operator++(int) noexcept {
        const FreeListIterator previous = *this;
        ++(*this);
        return previous;
    }

    /** Two iterators are equal iff they sit on the same slot (end == end). */
    [[nodiscard]] bool operator==(const FreeListIterator& rhs) const noexcept {
        return current_ == rhs.current_;
    }

    /** Negation of ::operator==. */
    [[nodiscard]] bool operator!=(const FreeListIterator& rhs) const noexcept {
        return !(*this == rhs);
    }

private:
    const memory_pool_t* pool_ = nullptr;
    const void* current_ = nullptr;
};

/**
 * @brief Lightweight read-only range over a pool's free list (ADR-0019 §3).
 *
 * The entry point for the diagnostic walk:
 * @code
 *   for (const void* slot : FreeListView{pool}) {
 *       // inspect slot
 *   }
 *   const auto free = std::distance(FreeListView{pool}.begin(),
 *                                   FreeListView{pool}.end());
 * @endcode
 *
 * Holds only a `const memory_pool_t*`; constructing it neither allocates
 * nor walks. The view does not own the pool and must not out-live it.
 */
class FreeListView {
public:
    /** View the free list of @p pool (a C handle), or an empty range if `NULL`. */
    explicit FreeListView(const memory_pool_t* pool) noexcept : pool_(pool) {}

    /** View the free list of @p pool — ergonomic overload for the C++ wrapper. */
    explicit FreeListView(Pool& pool) noexcept : pool_(pool.native_handle()) {}

    /** @return Iterator at the head of the free list (ADR-0019 §2). */
    [[nodiscard]] FreeListIterator begin() const noexcept {
        return {pool_, ::memory_pool_debug_free_list_head(pool_)};
    }

    /**
     * @return End sentinel. The null `current_` is what marks the end
     *         (equality compares only the current slot), so this equals a
     *         default-constructed `FreeListIterator`; carrying `pool_` keeps
     *         the two iterators of a range mutually consistent.
     */
    [[nodiscard]] FreeListIterator end() const noexcept {
        return {pool_, nullptr};
    }

private:
    const memory_pool_t* pool_;
};

}  // namespace it::d4np::memorypool

#endif  // PBR_MEMORY_POOL_DIAGNOSTICS

#endif  // IT_D4NP_MEMORYPOOL_FREE_LIST_ITERATOR_HPP_
