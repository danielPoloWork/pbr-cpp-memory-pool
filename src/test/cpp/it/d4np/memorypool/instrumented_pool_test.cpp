// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file instrumented_pool_test.cpp
 * @brief Tests for the InstrumentedPool Decorator (M6.1 / ADR-0025).
 *
 * The cases prove that the decorator counts allocations / deallocations /
 * exhaustion failures, tracks the live count and its high-water mark, forwards
 * allocation behaviour unchanged, exposes a snapshot + summary, is move-only
 * with counters carried across a move, and works over a dynamic pool. Fixed
 * pools are built through `Pool`'s throwing constructor (no optional, keeping
 * each case small); the dynamic case uses the `std::nullopt`-returning factory
 * so it can skip cleanly under a lock-free build.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/instrumented_pool.hpp>

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

using it::d4np::memorypool::InstrumentedPool;
using it::d4np::memorypool::Pool;
using it::d4np::memorypool::PoolStats;

namespace {

constexpr std::size_t BLOCK_SIZE = 64U;

}  // namespace

TEST_CASE("InstrumentedPool counts successful allocations and the live high-water mark") {
    InstrumentedPool pool{Pool(BLOCK_SIZE, 8U)};
    void* const a = pool.try_allocate();
    void* const b = pool.try_allocate();
    void* const c = pool.try_allocate();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    const PoolStats s = pool.stats();
    CHECK(s.allocations_ == 3U);
    CHECK(s.live_ == 3U);
    CHECK(s.peak_live_ == 3U);
    CHECK(s.allocation_failures_ == 0U);

    pool.deallocate(a);
    pool.deallocate(b);
    pool.deallocate(c);
}

TEST_CASE("InstrumentedPool deallocations lower live but the peak persists") {
    InstrumentedPool pool{Pool(BLOCK_SIZE, 8U)};
    void* const a = pool.try_allocate();
    void* const b = pool.try_allocate();
    pool.deallocate(a);

    const PoolStats s = pool.stats();
    CHECK(s.deallocations_ == 1U);
    CHECK(s.live_ == 1U);
    CHECK(s.peak_live_ == 2U);  // high-water mark stays at the peak

    pool.deallocate(b);
}

TEST_CASE("InstrumentedPool counts exhaustion failures from both verbs") {
    InstrumentedPool pool{Pool(BLOCK_SIZE, 2U)};
    void* const first = pool.try_allocate();
    void* const second = pool.try_allocate();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    CHECK(pool.try_allocate() == nullptr);                                // in-band failure
    CHECK_THROWS_AS(static_cast<void>(pool.allocate()), std::bad_alloc);  // throwing failure

    const PoolStats s = pool.stats();
    CHECK(s.allocations_ == 2U);
    CHECK(s.allocation_failures_ == 2U);

    pool.deallocate(first);
    pool.deallocate(second);
}

TEST_CASE("InstrumentedPool forwards LIFO allocation behaviour unchanged") {
    InstrumentedPool pool{Pool(BLOCK_SIZE, 2U)};
    void* const first = pool.try_allocate();
    void* const second = pool.try_allocate();
    REQUIRE(second != nullptr);

    pool.deallocate(second);
    CHECK(pool.try_allocate() == second);  // most recently freed comes back

    pool.deallocate(second);
    pool.deallocate(first);
}

TEST_CASE("InstrumentedPool::write_summary renders the counters") {
    InstrumentedPool pool{Pool(BLOCK_SIZE, 4U)};
    void* const block = pool.try_allocate();
    REQUIRE(block != nullptr);

    std::ostringstream oss;
    pool.write_summary(oss);
    const std::string summary = oss.str();
    CHECK(summary.find("allocations=1") != std::string::npos);
    CHECK(summary.find("peak_live=1") != std::string::npos);

    pool.deallocate(block);
}

TEST_CASE("InstrumentedPool is move-only and carries counters + pass-throughs across a move") {
    InstrumentedPool original{Pool(BLOCK_SIZE, 8U)};
    void* const block = original.try_allocate();
    REQUIRE(block != nullptr);
    original.deallocate(block);

    InstrumentedPool moved{std::move(original)};
    const PoolStats s = moved.stats();
    CHECK(s.allocations_ == 1U);  // counters survived the move
    CHECK(s.deallocations_ == 1U);
    CHECK(moved.block_size() == BLOCK_SIZE);  // pass-through still works
    CHECK(moved.native_handle() != nullptr);
}

TEST_CASE("InstrumentedPool over a dynamic pool never fails and tracks the rising peak") {
    std::optional<InstrumentedPool> opt = InstrumentedPool::make_dynamic(BLOCK_SIZE, 4U, 2U);
    if (!opt.has_value()) {
        return;  // lock-free build: dynamic mode rejected (ADR-0024 §2) — nothing to test
    }
    InstrumentedPool& pool = *opt;

    constexpr std::size_t COUNT = 100U;  // far past the initial 4 — forces growth
    std::vector<void*> blocks;
    blocks.reserve(COUNT);
    for (std::size_t i = 0; i < COUNT; ++i) {
        void* const block = pool.try_allocate();
        REQUIRE(block != nullptr);  // dynamic: grows rather than failing
        blocks.push_back(block);
    }

    const PoolStats s = pool.stats();
    CHECK(s.allocations_ == COUNT);
    CHECK(s.allocation_failures_ == 0U);
    CHECK(s.peak_live_ == COUNT);

    for (void* const block : blocks) {
        pool.deallocate(block);
    }
}
