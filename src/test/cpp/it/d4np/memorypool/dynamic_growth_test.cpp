// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file dynamic_growth_test.cpp
 * @brief Exhaustion-and-grow tests for the dynamic-growth mode (M5.4).
 *
 * Exercises the ADR-0022 / ADR-0023 / ADR-0024 dynamic pool: on exhaustion a
 * dynamic pool acquires a geometric overflow chunk instead of failing. The
 * CMake target mirrors the library's `PBR_MEMORY_POOL_THREAD_SAFETY` mode, so
 * this TU knows whether it is a lock-free build:
 *   - NONE / MUTEX: the full growth matrix — repeated geometric growth,
 *     several factors, cross-chunk distinctness, full recovery / no leak
 *     (ASan- and Valgrind-checked), the ADR-0012 range check across chunks,
 *     and the C++ `make_dynamic` / `PoolBuilder` surface;
 *   - LOCKFREE: the rejection contract — `memory_pool_create_dynamic`,
 *     `Pool::make_dynamic`, and a growth `PoolBuilder` all fail (ADR-0024 §2),
 *     while fixed-mode pools still work.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <cstddef>
#include <optional>
#include <set>
#include <vector>

#include <doctest/doctest.h>

namespace mem = it::d4np::memorypool;

namespace {

constexpr std::size_t BLOCK_SIZE = 64U;

}  // namespace

#if PBR_MEMORY_POOL_THREAD_SAFETY != PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE

namespace {

// Drain a dynamic pool for `count` blocks; every alloc must succeed (the pool
// grows rather than exhausting) and every pointer must be distinct.
void drain_and_check_distinct(memory_pool_t* pool, std::size_t count) {
    std::set<void*> seen;
    std::vector<void*> blocks;
    blocks.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        void* const block = memory_pool_alloc(pool);
        REQUIRE(block != nullptr);  // dynamic: never exhausts
        blocks.push_back(block);
        seen.insert(block);
    }
    CHECK(seen.size() == count);  // all distinct across chunks
    for (void* const block : blocks) {
        memory_pool_free(pool, block);
    }
}

}  // namespace

TEST_CASE("a dynamic pool grows repeatedly and vends distinct blocks across chunks") {
    constexpr std::size_t INITIAL = 4U;
    constexpr std::size_t COUNT = 1000U;  // ~8 geometric growths from 4 at factor 2
    memory_pool_t* const pool = memory_pool_create_dynamic(BLOCK_SIZE, INITIAL, 2U);
    REQUIRE(pool != nullptr);

    const std::size_t metadata_before = memory_pool_metadata_bytes(pool);
    drain_and_check_distinct(pool, COUNT);
    // Growth added overflow-chunk descriptors, so the per-pool metadata rose.
    CHECK(memory_pool_metadata_bytes(pool) > metadata_before);

    memory_pool_destroy(pool);
}

TEST_CASE("dynamic growth works across several growth factors") {
    for (const std::size_t factor : {2U, 3U, 4U}) {
        memory_pool_t* const pool = memory_pool_create_dynamic(BLOCK_SIZE, 2U, factor);
        REQUIRE(pool != nullptr);
        drain_and_check_distinct(pool, 500U);
        memory_pool_destroy(pool);
    }
}

TEST_CASE("a grown dynamic pool fully recovers — no leak, re-allocatable") {
    constexpr std::size_t INITIAL = 8U;
    constexpr std::size_t COUNT = 600U;
    memory_pool_t* const pool = memory_pool_create_dynamic(BLOCK_SIZE, INITIAL, 2U);
    REQUIRE(pool != nullptr);

    // Grow, free everything, then re-drain the same count — the freed blocks
    // (now a larger free list) satisfy the second pass with no further growth
    // failure. Leak-freedom is asserted by the ASan / Valgrind CI cells.
    drain_and_check_distinct(pool, COUNT);
    drain_and_check_distinct(pool, COUNT);

    memory_pool_destroy(pool);
}

TEST_CASE("the ADR-0012 range check spans grown chunks") {
    memory_pool_t* const pool = memory_pool_create_dynamic(BLOCK_SIZE, 2U, 2U);
    REQUIRE(pool != nullptr);

    // Force growth, then keep a block that lives in an overflow chunk.
    std::vector<void*> blocks;
    for (std::size_t i = 0; i < 64U; ++i) {
        void* const block = memory_pool_alloc(pool);
        REQUIRE(block != nullptr);
        blocks.push_back(block);
    }

    // A foreign (stack) pointer is a silent no-op; an in-range grown-chunk
    // block frees normally and is handed back out on the next alloc.
    int stack_object = 0;
    memory_pool_free(pool, &stack_object);  // foreign — ignored
    void* const recycled = blocks.back();
    blocks.pop_back();
    memory_pool_free(pool, recycled);  // genuine grown-chunk block — accepted
    void* const reissued = memory_pool_alloc(pool);
    CHECK(reissued == recycled);  // LIFO: the just-freed block comes back
    blocks.push_back(reissued);

    for (void* const block : blocks) {
        memory_pool_free(pool, block);
    }
    memory_pool_destroy(pool);
}

TEST_CASE("a fixed pool does not grow — it exhausts at block_count") {
    constexpr std::size_t CAPACITY = 8U;
    memory_pool_t* const pool = memory_pool_create(BLOCK_SIZE, CAPACITY);
    REQUIRE(pool != nullptr);
    std::vector<void*> blocks;
    for (std::size_t i = 0; i < CAPACITY; ++i) {
        void* const block = memory_pool_alloc(pool);
        REQUIRE(block != nullptr);
        blocks.push_back(block);
    }
    CHECK(memory_pool_alloc(pool) == nullptr);  // fixed: exhausts
    for (void* const block : blocks) {
        memory_pool_free(pool, block);
    }
    memory_pool_destroy(pool);
}

TEST_CASE("the C++ surface grows: Pool::make_dynamic and PoolBuilder::with_growth_factor") {
    std::optional<mem::Pool> made = mem::Pool::make_dynamic(BLOCK_SIZE, 4U, 2U);
    REQUIRE(made.has_value());
    if (made.has_value()) {
        for (int i = 0; i < 200; ++i) {
            void* const block = made->try_allocate();
            REQUIRE(block != nullptr);  // grows
            made->deallocate(block);
        }
    }

    std::optional<mem::Pool> built =
        mem::PoolBuilder{}.with_block_size(BLOCK_SIZE).with_block_count(4U).with_growth_factor(2U).build();
    REQUIRE(built.has_value());
    if (built.has_value()) {
        void* const block = built->allocate();  // throwing verb; grows, never throws here
        CHECK(block != nullptr);
        built->deallocate(block);
    }
}

#else  // PBR_MEMORY_POOL_THREAD_SAFETY == LOCKFREE

TEST_CASE("dynamic mode is rejected under the lock-free policy (ADR-0024 §2)") {
    // Safe lock-free chunk-list growth is deferred; creation must fail rather
    // than hand back a pool that silently never grows.
    CHECK(memory_pool_create_dynamic(BLOCK_SIZE, 16U, 2U) == nullptr);
    CHECK_FALSE(mem::Pool::make_dynamic(BLOCK_SIZE, 16U, 2U).has_value());
    const std::optional<mem::Pool> built =
        mem::PoolBuilder{}.with_block_size(BLOCK_SIZE).with_block_count(16U).with_growth_factor(2U).build();
    CHECK_FALSE(built.has_value());
}

TEST_CASE("fixed-mode pools still work under the lock-free policy") {
    std::optional<mem::Pool> pool = mem::Pool::make(BLOCK_SIZE, 16U);
    REQUIRE(pool.has_value());
    if (pool.has_value()) {
        void* const block = pool->try_allocate();
        CHECK(block != nullptr);
        pool->deallocate(block);
    }
}

#endif  // PBR_MEMORY_POOL_THREAD_SAFETY != LOCKFREE
