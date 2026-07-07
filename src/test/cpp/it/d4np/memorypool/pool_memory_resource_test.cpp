// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_memory_resource_test.cpp
 * @brief Tests for the `PoolMemoryResource` std::pmr Adapter (M9.1 / ADR-0042).
 *
 * The suite is gated behind `PBR_MEMORY_POOL_HAS_PMR` so the binary still
 * builds (with a single placeholder case) on a toolchain whose standard
 * library does not provide `<memory_resource>`. When enabled, the cases prove
 * that:
 *   - pool-eligible requests (fit one block) are served from the pool and
 *     exhaust it — so they are demonstrably *not* silently upstream-backed;
 *   - deallocation returns a block to the pool (a freed slot is reissued);
 *   - over-sized and over-aligned requests route to the upstream resource, not
 *     the pool (proven with `null_memory_resource`, whose allocate always
 *     throws, and by leaving the pool undrained);
 *   - `is_equal` is by (pool, upstream) identity, across resource types;
 *   - a real `std::pmr` container and a `polymorphic_allocator` round-trip
 *     end-to-end through the resource.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/pool_memory_resource.hpp>

#include <doctest/doctest.h>

#if PBR_MEMORY_POOL_HAS_PMR

#include <it/d4np/memorypool/memory_pool.hpp>

#include <cstddef>
#include <list>
#include <memory_resource>
#include <new>
#include <vector>

using it::d4np::memorypool::Pool;
using it::d4np::memorypool::PoolMemoryResource;

namespace {

// A block size comfortably above any small pmr node on a 64-bit host and a
// multiple of alignof(max_align_t) (ADR-0009 §2) so Pool construction succeeds.
constexpr std::size_t BLOCK_SIZE = 64U;
constexpr std::size_t BLOCK_COUNT = 8U;

}  // namespace

TEST_CASE("PoolMemoryResource serves pool-eligible requests from the pool and exhausts it") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    PoolMemoryResource mr(pool);

    // Draining exactly BLOCK_COUNT fitting allocations then seeing the next
    // throw proves the storage came from the fixed-capacity pool, not from an
    // unbounded upstream fallback.
    std::vector<void*> blocks;
    for (std::size_t i = 0; i < BLOCK_COUNT; ++i) {
        void* const p = mr.allocate(BLOCK_SIZE);
        REQUIRE(p != nullptr);
        blocks.push_back(p);
    }
    CHECK_THROWS_AS(static_cast<void>(mr.allocate(BLOCK_SIZE)), std::bad_alloc);

    // Returning one block and re-allocating succeeds — the slot went back to
    // the pool's free list (deterministic routing, ADR-0042).
    mr.deallocate(blocks.back(), BLOCK_SIZE);
    blocks.pop_back();
    void* const reissued = mr.allocate(BLOCK_SIZE);
    CHECK(reissued != nullptr);
    blocks.push_back(reissued);

    for (void* const p : blocks) {
        mr.deallocate(p, BLOCK_SIZE);
    }
}

TEST_CASE("PoolMemoryResource routes over-sized and over-aligned requests to the upstream") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    // A null upstream throws on every request it receives, so a throw proves
    // the request was routed upstream rather than served by the pool.
    PoolMemoryResource mr(pool, std::pmr::null_memory_resource());

    // Over-sized (two blocks' worth) → upstream (null) → throws.
    CHECK_THROWS_AS(static_cast<void>(mr.allocate(BLOCK_SIZE * 2U)), std::bad_alloc);
    // Over-aligned (beyond max_align_t) → upstream (null) → throws.
    CHECK_THROWS_AS(static_cast<void>(mr.allocate(BLOCK_SIZE, alignof(std::max_align_t) * 2U)), std::bad_alloc);

    // A pool-eligible request still succeeds — it never touched the null
    // upstream — and the pool was left fully available by the routed-away
    // requests above (BLOCK_COUNT fitting allocations all succeed).
    std::vector<void*> blocks;
    for (std::size_t i = 0; i < BLOCK_COUNT; ++i) {
        void* const p = mr.allocate(BLOCK_SIZE);
        REQUIRE(p != nullptr);
        blocks.push_back(p);
    }
    for (void* const p : blocks) {
        mr.deallocate(p, BLOCK_SIZE);
    }
}

TEST_CASE("PoolMemoryResource::is_equal compares (pool, upstream) identity across types") {
    Pool pool_a(BLOCK_SIZE, BLOCK_COUNT);
    Pool pool_b(BLOCK_SIZE, BLOCK_COUNT);

    PoolMemoryResource ra1(pool_a);
    PoolMemoryResource ra2(pool_a);
    PoolMemoryResource rb(pool_b);
    PoolMemoryResource ra_upstream(pool_a, std::pmr::null_memory_resource());

    // Equal iff same pool AND same upstream (the free operator== consults
    // is_equal after the identity short-circuit).
    CHECK(ra1 == ra2);
    CHECK(ra1 != rb);
    CHECK(ra1 != ra_upstream);

    // Never equal to a different concrete resource type.
    CHECK(ra1 != *std::pmr::new_delete_resource());
    CHECK_FALSE(ra1.is_equal(*std::pmr::new_delete_resource()));
}

TEST_CASE("std::pmr::list round-trips through PoolMemoryResource") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    PoolMemoryResource mr(pool);

    // A node-based container allocates one node at a time; whether each node
    // fits a pool block or spills to the upstream, the container stays correct.
    std::pmr::list<int> values(&mr);
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

TEST_CASE("std::pmr::vector and a polymorphic_allocator round-trip through PoolMemoryResource") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    PoolMemoryResource mr(pool);

    // vector requests contiguous n>1 storage as it grows; the resource serves
    // that from the upstream while staying correct.
    std::pmr::vector<int> values(&mr);
    for (int i = 0; i < 100; ++i) {
        values.push_back(i);
    }
    REQUIRE(values.size() == 100U);
    CHECK(values.front() == 0);
    CHECK(values.back() == 99);

    // A polymorphic_allocator built from the resource vends single blocks from
    // the pool fast path and hands them back.
    std::pmr::polymorphic_allocator<int> alloc(&mr);
    int* const one = alloc.allocate(1);
    REQUIRE(one != nullptr);
    *one = 42;
    CHECK(*one == 42);
    alloc.deallocate(one, 1);
}

#else  // PBR_MEMORY_POOL_HAS_PMR

TEST_CASE("std::pmr adapter is compiled out on this toolchain") {
    // The standard library does not provide <memory_resource>; the adapter is
    // gated out (ADR-0042). This placeholder keeps the binary and its CTest
    // registration valid, mirroring the free_list_iterator_test pattern.
    CHECK(true);
}

#endif  // PBR_MEMORY_POOL_HAS_PMR
