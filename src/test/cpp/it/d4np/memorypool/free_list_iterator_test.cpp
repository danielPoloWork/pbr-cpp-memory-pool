// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file free_list_iterator_test.cpp
 * @brief Tests for the free-list diagnostic Iterator (M3.4 / ADR-0019).
 *
 * The whole suite is gated behind `PBR_MEMORY_POOL_DIAGNOSTICS` so the
 * binary still builds (with a single placeholder case) under a release
 * CTest run where the diagnostic surface is compiled out. When enabled,
 * the cases prove that:
 *   - a fresh pool's free list walks all `block_count` slots in ascending
 *     address order, block-strided, and `free_count` agrees with
 *     `std::distance`;
 *   - allocation shrinks the walked list, free restores it, and a freed
 *     block returns to the head (LIFO) so the iterator sees it first;
 *   - an exhausted pool yields an empty range (`begin() == end()`);
 *   - the type behaves as a LegacyForwardIterator (range-for, std::find,
 *     post-increment, default-constructed end sentinel).
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/free_list_iterator.hpp>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <set>
#include <vector>

#include <doctest/doctest.h>

#if PBR_MEMORY_POOL_DIAGNOSTICS

using it::d4np::memorypool::FreeListIterator;
using it::d4np::memorypool::FreeListView;
using it::d4np::memorypool::Pool;

namespace {

constexpr std::size_t BLOCK_SIZE = 32U;
constexpr std::size_t BLOCK_COUNT = 8U;

std::uintptr_t to_uint(const void* ptr) noexcept {
    // Same narrow ptr-to-int NOLINT pattern as the other test TUs — an
    // address/alignment check has no portable C++17 alternative.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<std::uintptr_t>(ptr);
}

}  // namespace

TEST_CASE("FreeListView walks every slot of a fresh pool in ascending, strided order") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    const FreeListView view(pool);

    std::vector<const void*> slots;
    for (const void* const slot : view) {
        slots.push_back(slot);
    }

    REQUIRE(slots.size() == BLOCK_COUNT);
    CHECK(::memory_pool_debug_free_count(pool.native_handle()) == BLOCK_COUNT);
    CHECK(static_cast<std::size_t>(std::distance(view.begin(), view.end())) == BLOCK_COUNT);

    // ADR-0009 §1 — a fresh pool initialises the list in ascending address
    // order at a fixed physical slot stride, so slot i sits exactly i strides
    // above the head, and all addresses are distinct. The stride equals
    // block_size in a default build; the opt-in hardening build (ADR-0043)
    // widens it by a trailing guard word, so derive it from the first gap
    // rather than assuming block_size — the walked-order property is what this
    // case asserts, not the exact stride.
    const std::uintptr_t head = to_uint(slots.front());
    const std::uintptr_t stride = (slots.size() > 1U) ? (to_uint(slots.at(1)) - head) : BLOCK_SIZE;
    CHECK(stride >= BLOCK_SIZE);
    CHECK((stride % alignof(std::max_align_t)) == 0U);
    std::set<const void*> distinct;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        CHECK(to_uint(slots.at(i)) == head + (i * stride));
        distinct.insert(slots.at(i));
    }
    CHECK(distinct.size() == BLOCK_COUNT);
}

TEST_CASE("allocation shrinks the walked free list") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    void* const first = pool.allocate();
    REQUIRE(first != nullptr);

    const FreeListView view(pool);
    CHECK(static_cast<std::size_t>(std::distance(view.begin(), view.end())) == BLOCK_COUNT - 1U);
    CHECK(::memory_pool_debug_free_count(pool.native_handle()) == BLOCK_COUNT - 1U);

    pool.deallocate(first);
}

TEST_CASE("a freed block returns to the head and is walked first (LIFO)") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    void* const first = pool.allocate();
    void* const second = pool.allocate();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    // Free `first` last: it becomes the new head, so the iterator must
    // visit it before anything else (ADR-0009 §1 push-to-head).
    pool.deallocate(second);
    pool.deallocate(first);

    const FreeListView view(pool);
    CHECK(static_cast<std::size_t>(std::distance(view.begin(), view.end())) == BLOCK_COUNT);
    CHECK(*view.begin() == first);
}

TEST_CASE("an exhausted pool yields an empty range") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    std::vector<void*> held;
    for (std::size_t i = 0; i < BLOCK_COUNT; ++i) {
        void* const block = pool.allocate();
        REQUIRE(block != nullptr);
        held.push_back(block);
    }

    const FreeListView view(pool);
    CHECK(view.begin() == view.end());
    CHECK(::memory_pool_debug_free_count(pool.native_handle()) == 0U);

    for (void* const block : held) {
        pool.deallocate(block);
    }
}

TEST_CASE("FreeListIterator behaves as a LegacyForwardIterator") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    const FreeListView view(pool);

    // Default-constructed iterator is the end sentinel.
    CHECK(FreeListIterator{} == view.end());

    // std::find composes (multi-pass forward iterator). The head slot is
    // findable; a never-allocated foreign address is not.
    const void* const head = *view.begin();
    CHECK(std::find(view.begin(), view.end(), head) != view.end());
    int stack_object = 0;
    CHECK(std::find(view.begin(), view.end(), &stack_object) == view.end());

    // Post-increment returns the pre-advance position.
    FreeListIterator it = view.begin();
    const FreeListIterator before = it++;
    CHECK(before == view.begin());
    CHECK(it != view.begin());
}

#else  // PBR_MEMORY_POOL_DIAGNOSTICS

TEST_CASE("free-list diagnostic iterator is compiled out in this build") {
    // Release builds (NDEBUG, option off) gate the diagnostic surface out
    // entirely (ADR-0019 §1); this placeholder keeps the binary and its
    // CTest registration valid.
    CHECK(true);
}

#endif  // PBR_MEMORY_POOL_DIAGNOSTICS
