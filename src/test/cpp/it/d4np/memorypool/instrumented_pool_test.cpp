// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file instrumented_pool_test.cpp
 * @brief Tests for the InstrumentedPool Decorator (M6.1 / ADR-0025).
 *
 * The cases prove that the decorator:
 *   - counts allocations, deallocations, and exhaustion failures correctly;
 *   - tracks the live-block count and its high-water mark (peak_live_);
 *   - forwards allocation behaviour (LIFO, exhaustion) unchanged;
 *   - exposes a PoolStats snapshot and a human-readable summary;
 *   - is move-only with hand-written move semantics (counters carried over);
 *   - passes through native_handle / block_size / metadata_bytes;
 *   - works equally over a dynamic pool (make_dynamic), where allocation never
 *     fails because the pool grows.
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
using it::d4np::memorypool::PoolStats;

namespace {

constexpr std::size_t BLOCK_SIZE = 64U;

}  // namespace

TEST_CASE("InstrumentedPool counts allocations, deallocations, live, and peak") {
    std::optional<InstrumentedPool> opt = InstrumentedPool::make(BLOCK_SIZE, 8U);
    REQUIRE(opt.has_value());
    if (!opt.has_value()) {
        return;
    }
    InstrumentedPool& pool = *opt;

    void* const a = pool.try_allocate();
    void* const b = pool.try_allocate();
    void* const c = pool.try_allocate();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    PoolStats s = pool.stats();
    CHECK(s.allocations_ == 3U);
    CHECK(s.deallocations_ == 0U);
    CHECK(s.live_ == 3U);
    CHECK(s.peak_live_ == 3U);
    CHECK(s.allocation_failures_ == 0U);

    pool.deallocate(a);
    pool.deallocate(b);
    s = pool.stats();
    CHECK(s.deallocations_ == 2U);
    CHECK(s.live_ == 1U);
    CHECK(s.peak_live_ == 3U);  // high-water mark stays at the peak

    pool.deallocate(c);
}

TEST_CASE("InstrumentedPool counts exhaustion failures and forwards behaviour") {
    std::optional<InstrumentedPool> opt = InstrumentedPool::make(BLOCK_SIZE, 2U);
    REQUIRE(opt.has_value());
    if (!opt.has_value()) {
        return;
    }
    InstrumentedPool& pool = *opt;

    void* const first = pool.try_allocate();
    void* const second = pool.try_allocate();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    // Exhausted: try_allocate reports in-band, allocate throws — both counted.
    CHECK(pool.try_allocate() == nullptr);
    CHECK_THROWS_AS(static_cast<void>(pool.allocate()), std::bad_alloc);

    const PoolStats s = pool.stats();
    CHECK(s.allocations_ == 2U);
    CHECK(s.allocation_failures_ == 2U);
    CHECK(s.live_ == 2U);

    // LIFO forwarding is unchanged: the most recently freed block returns.
    pool.deallocate(second);
    void* const reissued = pool.try_allocate();
    CHECK(reissued == second);

    pool.deallocate(reissued);
    pool.deallocate(first);
}

TEST_CASE("InstrumentedPool::write_summary renders the counters") {
    std::optional<InstrumentedPool> opt = InstrumentedPool::make(BLOCK_SIZE, 4U);
    REQUIRE(opt.has_value());
    if (!opt.has_value()) {
        return;
    }
    InstrumentedPool& pool = *opt;
    void* const block = pool.try_allocate();
    REQUIRE(block != nullptr);

    std::ostringstream oss;
    pool.write_summary(oss);
    const std::string summary = oss.str();
    CHECK(summary.find("allocations=1") != std::string::npos);
    CHECK(summary.find("peak_live=1") != std::string::npos);

    pool.deallocate(block);
}

TEST_CASE("InstrumentedPool is move-only and carries its counters across a move") {
    std::optional<InstrumentedPool> opt = InstrumentedPool::make(BLOCK_SIZE, 8U);
    REQUIRE(opt.has_value());
    if (!opt.has_value()) {
        return;
    }
    void* const block = opt->try_allocate();
    REQUIRE(block != nullptr);
    opt->deallocate(block);

    // Move-construct a new decorator from the engaged optional's value.
    InstrumentedPool moved{std::move(*opt)};
    const PoolStats s = moved.stats();
    CHECK(s.allocations_ == 1U);    // counters survived the move
    CHECK(s.deallocations_ == 1U);
    CHECK(s.peak_live_ == 1U);
    CHECK(moved.block_size() == BLOCK_SIZE);          // pass-through still works
    CHECK(moved.native_handle() != nullptr);
    CHECK(moved.metadata_bytes() > 0U);
}

TEST_CASE("InstrumentedPool over a dynamic pool never fails and tracks the rising peak") {
    std::optional<InstrumentedPool> opt = InstrumentedPool::make_dynamic(BLOCK_SIZE, 4U, 2U);
    if (!opt.has_value()) {
        // Lock-free build: dynamic mode is rejected (ADR-0024 §2) — nothing to test here.
        return;
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
    CHECK(s.allocation_failures_ == 0U);  // never exhausted
    CHECK(s.live_ == COUNT);
    CHECK(s.peak_live_ == COUNT);

    for (void* const block : blocks) {
        pool.deallocate(block);
    }
    CHECK(pool.stats().live_ == 0U);
}
