// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_allocator_test.cpp
 * @brief Tests for the `PoolAllocator<T>` STL Adapter (M3.3 / ADR-0018).
 *
 * The cases below prove that:
 *   - the Cpp17Allocator surface is shaped correctly (rebind, traits,
 *     handle-identity equality — compile-time and runtime);
 *   - single-element requests are pool-served, with provenance observable
 *     through `memory_pool_owns` and LIFO slot reuse;
 *   - bulk, oversized, and pool-exhausted requests fall back to the heap
 *     and deallocate routes each pointer by provenance (ADR-0018 §1);
 *   - a real node-based container (`std::list`) genuinely draws its nodes
 *     from the pool and returns them on destruction;
 *   - `std::vector` composes correctly (the full container battery is
 *     ROADMAP §3.5).
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/pool_allocator.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <type_traits>
#include <vector>

#include <doctest/doctest.h>

using it::d4np::memorypool::Pool;
using it::d4np::memorypool::PoolAllocator;

namespace {

constexpr std::size_t SAFE_BLOCK_SIZE = 64U;
constexpr std::size_t SAFE_BLOCK_COUNT = 8U;

}  // namespace

// Compile-time shape of the adapter — ADR-0018 §3.
static_assert(sizeof(PoolAllocator<int>) == sizeof(void*), "single non-owning handle (ADR-0018 §3)");
static_assert(std::is_trivially_copyable_v<PoolAllocator<int>>, "allocator copies must be cheap");
static_assert(PoolAllocator<int>::propagate_on_container_copy_assignment::value, "POCCA (ADR-0018 §3)");
static_assert(PoolAllocator<int>::propagate_on_container_move_assignment::value, "POCMA (ADR-0018 §3)");
static_assert(PoolAllocator<int>::propagate_on_container_swap::value, "POCS (ADR-0018 §3)");
static_assert(!PoolAllocator<int>::is_always_equal::value, "equality is handle identity (ADR-0018 §3)");
static_assert(std::is_same_v<std::allocator_traits<PoolAllocator<int>>::rebind_alloc<long>, PoolAllocator<long>>,
              "rebinding maps onto the same template (ADR-0018 §3)");

TEST_CASE("PoolAllocator equality is handle identity, across rebound types") {
    Pool first(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    Pool second(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);

    const PoolAllocator<int> a(first);
    const PoolAllocator<int> b(first);
    const PoolAllocator<int> c(second);
    const PoolAllocator<long> rebound(a);  // converting ctor — shares first's handle

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a == rebound);
    CHECK(rebound.native_handle() == first.native_handle());
}

TEST_CASE("single-element requests are pool-served with LIFO reuse (ADR-0018 §1)") {
    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    PoolAllocator<int> alloc(pool);

    int* const slot = alloc.allocate(1U);
    REQUIRE(slot != nullptr);
    CHECK(memory_pool_owns(pool.native_handle(), slot) == 1);

    // Capture the slot's identity as an integer while it is still live.
    // The LIFO assertion below must compare *addresses*, not the freed
    // `slot` pointer itself: reading a deallocated pointer variable inside
    // doctest's CHECK machinery trips clang-analyzer-cplusplus.NewDelete
    // (the analyzer cannot see through the opaque pool C boundary and
    // models allocate() as ::operator new). Comparing the uintptr_t copy
    // is the same ptr-to-int idiom used elsewhere in the suite and is
    // immune to that false positive.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto slot_addr = reinterpret_cast<std::uintptr_t>(slot);

    alloc.deallocate(slot, 1U);
    int* const reissued = alloc.allocate(1U);
    // LIFO — the slot went back to the pool, not the heap, so the reissued
    // address matches the one just returned.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    CHECK(reinterpret_cast<std::uintptr_t>(reissued) == slot_addr);
    alloc.deallocate(reissued, 1U);
}

TEST_CASE("bulk requests fall back to the heap (ADR-0018 §1)") {
    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    PoolAllocator<int> alloc(pool);

    int* const bulk = alloc.allocate(8U);
    REQUIRE(bulk != nullptr);
    CHECK(memory_pool_owns(pool.native_handle(), bulk) == 0);
    alloc.deallocate(bulk, 8U);

    // The pool is untouched by the bulk round-trip: every slot vends.
    int* const probe = alloc.allocate(1U);
    CHECK(memory_pool_owns(pool.native_handle(), probe) == 1);
    alloc.deallocate(probe, 1U);
}

TEST_CASE("pool exhaustion degrades gracefully to the heap (ADR-0018 §1)") {
    Pool pool(SAFE_BLOCK_SIZE, 1U);
    PoolAllocator<int> alloc(pool);

    int* const from_pool = alloc.allocate(1U);
    REQUIRE(from_pool != nullptr);
    CHECK(memory_pool_owns(pool.native_handle(), from_pool) == 1);
    // Capture the pool slot's identity as an integer while live — see the
    // LIFO test above for why the post-deallocate comparison must be on
    // addresses rather than on the freed pointer (NewDelete false positive
    // across the opaque pool boundary).
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto from_pool_addr = reinterpret_cast<std::uintptr_t>(from_pool);

    // Second single-element request: pool exhausted, allocator stays total.
    int* const from_heap = alloc.allocate(1U);
    REQUIRE(from_heap != nullptr);
    CHECK(memory_pool_owns(pool.native_handle(), from_heap) == 0);

    // Provenance routing returns each pointer to its origin.
    alloc.deallocate(from_heap, 1U);
    alloc.deallocate(from_pool, 1U);
    int* const reissued = alloc.allocate(1U);
    // The slot is back on the free list, so the reissued address matches.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    CHECK(reinterpret_cast<std::uintptr_t>(reissued) == from_pool_addr);
    alloc.deallocate(reissued, 1U);
}

TEST_CASE("oversized rebound types route to the heap (ADR-0018 §1)") {
    struct Oversized {
        std::array<unsigned char, 128> bytes_;
    };
    static_assert(sizeof(Oversized) > SAFE_BLOCK_SIZE, "must not fit a slot");

    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    PoolAllocator<Oversized> alloc{PoolAllocator<int>(pool)};  // arrive via rebinding, as containers do

    Oversized* const storage = alloc.allocate(1U);
    REQUIRE(storage != nullptr);
    CHECK(memory_pool_owns(pool.native_handle(), storage) == 0);
    alloc.deallocate(storage, 1U);
}

TEST_CASE("std::list draws its nodes from the pool and returns them (ADR-0018 §1)") {
    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    PoolAllocator<int> alloc(pool);
    {
        std::list<int, PoolAllocator<int>> values(alloc);
        for (int i = 0; i < static_cast<int>(SAFE_BLOCK_COUNT); ++i) {
            values.push_back(i);
        }
        // At least block_count single-element node allocations happened
        // (some implementations also pool-allocate a sentinel), so the
        // pool is now exhausted: a probe must route to the heap.
        int* const probe = alloc.allocate(1U);
        CHECK(memory_pool_owns(pool.native_handle(), probe) == 0);
        alloc.deallocate(probe, 1U);
        CHECK(values.size() == SAFE_BLOCK_COUNT);
        CHECK(values.front() == 0);
        CHECK(values.back() == static_cast<int>(SAFE_BLOCK_COUNT) - 1);
    }
    // The list returned every node: the next probe is pool-served again.
    int* const probe = alloc.allocate(1U);
    CHECK(memory_pool_owns(pool.native_handle(), probe) == 1);
    alloc.deallocate(probe, 1U);
}

TEST_CASE("std::vector composes with PoolAllocator (full battery in M3.5)") {
    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    const PoolAllocator<int> alloc(pool);

    std::vector<int, PoolAllocator<int>> values(alloc);
    for (int i = 0; i < 100; ++i) {
        values.push_back(i);
    }
    REQUIRE(values.size() == 100U);
    CHECK(values[0] == 0);
    CHECK(values[99] == 99);
    // Contiguous growth (n > 1) is heap-backed by design — provenance is
    // asserted on the storage rather than assumed.
    CHECK(memory_pool_owns(pool.native_handle(), values.data()) == 0);
}
