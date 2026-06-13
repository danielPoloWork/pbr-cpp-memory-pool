// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_allocator_test.cpp
 * @brief Tests for the `PoolAllocator<T>` STL adapter (M3.3 / ADR-0018).
 *
 * The cases below prove that:
 *   - pool-eligible single-block requests are served from the pool and
 *     exhaust it (so they are demonstrably *not* silently heap-backed);
 *   - requests the pool cannot serve (`n > 1`, oversized `T`) fall back to
 *     the heap and never touch the pool;
 *   - the pool/fallback routing is deterministic, so `deallocate` returns
 *     each pointer through the path that allocated it;
 *   - the propagation traits, statefulness, rebinding, and equality follow
 *     the Cpp17Allocator contract fixed by ADR-0018 §4;
 *   - a real standard container (`std::list`, `std::vector`) round-trips
 *     end-to-end through the adapter. The exhaustive container matrix is
 *     Milestone 3.5; these are conformance smoke checks.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/pool_allocator.hpp>

#include <array>
#include <cstddef>
#include <list>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

#include <doctest/doctest.h>

using it::d4np::memorypool::Pool;
using it::d4np::memorypool::PoolAllocator;

namespace {

// A pool block size comfortably above any small node type a container
// rebinds to on a 64-bit host, and a multiple of alignof(max_align_t)
// (ADR-0009 §2) so Pool construction succeeds.
constexpr std::size_t BLOCK_SIZE = 64U;
constexpr std::size_t BLOCK_COUNT = 8U;

}  // namespace

TEST_CASE("PoolAllocator serves pool-eligible single-block requests from the pool") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    PoolAllocator<int> alloc(pool);

    // Draining exactly BLOCK_COUNT single-int allocations and then seeing
    // the next throw proves these came from the fixed-capacity pool rather
    // than an unbounded heap fallback.
    std::vector<int*> blocks;
    for (std::size_t i = 0; i < BLOCK_COUNT; ++i) {
        int* const p = alloc.allocate(1);
        REQUIRE(p != nullptr);
        blocks.push_back(p);
    }
    CHECK_THROWS_AS(static_cast<void>(alloc.allocate(1)), std::bad_alloc);

    // Returning one block and re-allocating succeeds — the slot went back
    // to the pool's free list (deterministic routing, ADR-0018 §2).
    alloc.deallocate(blocks.back(), 1);
    blocks.pop_back();
    int* const reissued = alloc.allocate(1);
    CHECK(reissued != nullptr);
    blocks.push_back(reissued);

    for (int* const p : blocks) {
        alloc.deallocate(p, 1);
    }
}

TEST_CASE("PoolAllocator routes multi-block requests to the heap, never the pool") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    PoolAllocator<int> alloc(pool);

    // Many n>1 allocations far exceeding the pool's capacity must all
    // succeed — they are heap-backed and leave the pool untouched.
    std::vector<int*> arrays;
    for (std::size_t i = 0; i < BLOCK_COUNT * 4U; ++i) {
        int* const p = alloc.allocate(4);
        REQUIRE(p != nullptr);
        arrays.push_back(p);
    }
    for (int* const p : arrays) {
        alloc.deallocate(p, 4);
    }

    // The pool was never drained: BLOCK_COUNT single-block requests still
    // all succeed afterwards.
    std::vector<int*> blocks;
    for (std::size_t i = 0; i < BLOCK_COUNT; ++i) {
        int* const p = alloc.allocate(1);
        REQUIRE(p != nullptr);
        blocks.push_back(p);
    }
    for (int* const p : blocks) {
        alloc.deallocate(p, 1);
    }
}

TEST_CASE("PoolAllocator falls back for a T larger than the pool's block") {
    // A type wider than BLOCK_SIZE can never fit one block; even n == 1
    // must route to the heap (and so never exhausts the pool).
    struct Wide {
        std::array<unsigned char, BLOCK_SIZE * 2U> bytes_;
    };
    static_assert(sizeof(Wide) > BLOCK_SIZE, "Wide must exceed the block size for this test");

    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    PoolAllocator<Wide> alloc(pool);

    std::vector<Wide*> objects;
    for (std::size_t i = 0; i < BLOCK_COUNT * 2U; ++i) {
        Wide* const p = alloc.allocate(1);
        REQUIRE(p != nullptr);
        objects.push_back(p);
    }
    for (Wide* const p : objects) {
        alloc.deallocate(p, 1);
    }
}

TEST_CASE("PoolAllocator equality, statefulness, and rebinding follow the Cpp17Allocator contract") {
    Pool pool_a(BLOCK_SIZE, BLOCK_COUNT);
    Pool pool_b(BLOCK_SIZE, BLOCK_COUNT);

    PoolAllocator<int> a1(pool_a);
    PoolAllocator<int> a2(pool_a);
    PoolAllocator<int> b1(pool_b);

    // Equal iff same underlying pool (ADR-0018 §4).
    CHECK(a1 == a2);
    CHECK(a1 != b1);

    // Rebinding preserves the back-reference, and a rebound allocator still
    // compares equal to its source across element types.
    using Rebound = std::allocator_traits<PoolAllocator<int>>::rebind_alloc<double>;
    static_assert(std::is_same_v<Rebound, PoolAllocator<double>>,
                  "rebind_alloc<U> must be PoolAllocator<U> (single-parameter default)");
    const Rebound a1_as_double(a1);
    CHECK(a1_as_double == a1);
    CHECK(a1_as_double != b1);
}

TEST_CASE("PoolAllocator declares the ADR-0018 §4 propagation traits") {
    using Alloc = PoolAllocator<int>;
    static_assert(!Alloc::propagate_on_container_copy_assignment::value, "POCCA must be false");
    static_assert(!Alloc::propagate_on_container_move_assignment::value, "POCMA must be false");
    static_assert(!Alloc::propagate_on_container_swap::value, "POCS must be false");
    static_assert(!Alloc::is_always_equal::value, "stateful: is_always_equal must be false");
    static_assert(sizeof(Alloc) == sizeof(void*), "adapter is a single non-owning back-reference");
    CHECK(true);  // runtime anchor so doctest reports the case
}

TEST_CASE("std::list round-trips through PoolAllocator (pool fast path)") {
    // A list node fits comfortably in a 64-byte block on every Tier-1
    // host, so every node allocation takes the pool path.
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    std::list<int, PoolAllocator<int>> values{PoolAllocator<int>(pool)};

    for (int i = 0; i < 5; ++i) {
        values.push_back(i);
    }
    REQUIRE(values.size() == 5U);

    int sum = 0;
    for (const int v : values) {
        sum += v;
    }
    CHECK(sum == 0 + 1 + 2 + 3 + 4);

    values.clear();
    CHECK(values.empty());
}

TEST_CASE("std::vector round-trips through PoolAllocator (fallback path)") {
    // vector requests contiguous n>1 storage as it grows; the adapter
    // services that from the heap fallback while staying conformant.
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    std::vector<int, PoolAllocator<int>> values{PoolAllocator<int>(pool)};

    for (int i = 0; i < 100; ++i) {
        values.push_back(i);
    }
    REQUIRE(values.size() == 100U);
    CHECK(values.front() == 0);
    CHECK(values.back() == 99);
}
