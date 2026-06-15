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
using it::d4np::memorypool::PoolEvent;
using it::d4np::memorypool::PoolObserver;
using it::d4np::memorypool::PoolStats;

namespace {

constexpr std::size_t BLOCK_SIZE = 64U;

// Counts each event type. Uses plain counters (no allocation) so the override
// is genuinely noexcept, as the PoolObserver contract requires.
struct RecordingObserver : PoolObserver {
    int exhausted_ = 0;
    int grew_ = 0;
    int destroyed_ = 0;

    void on_pool_event(PoolEvent event, const PoolStats& /*stats*/) noexcept override {
        switch (event) {
        case PoolEvent::exhausted:
            ++exhausted_;
            break;
        case PoolEvent::grew:
            ++grew_;
            break;
        case PoolEvent::destroyed:
            ++destroyed_;
            break;
        }
    }
};

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

TEST_CASE("an observer is notified of exhaustion (ADR-0026)") {
    RecordingObserver obs;  // declared first → out-lives the pool it observes
    InstrumentedPool pool{Pool(BLOCK_SIZE, 1U)};
    pool.add_observer(obs);

    void* const a = pool.try_allocate();
    REQUIRE(a != nullptr);
    CHECK(pool.try_allocate() == nullptr);  // exhausted → notifies
    CHECK(obs.exhausted_ == 1);

    pool.deallocate(a);
}

TEST_CASE("an observer is notified of destruction (ADR-0026)") {
    RecordingObserver obs;
    {
        InstrumentedPool pool{Pool(BLOCK_SIZE, 4U)};
        pool.add_observer(obs);
    }  // pool destroyed here → notifies once
    CHECK(obs.destroyed_ == 1);
}

TEST_CASE("an observer is notified of growth on a dynamic pool (ADR-0026)") {
    RecordingObserver obs;
    std::optional<InstrumentedPool> opt = InstrumentedPool::make_dynamic(BLOCK_SIZE, 2U, 2U);
    if (!opt.has_value()) {
        return;  // lock-free build: dynamic mode rejected — no growth to observe
    }
    opt->add_observer(obs);

    std::vector<void*> blocks;
    for (int i = 0; i < 50; ++i) {
        void* const block = opt->try_allocate();
        REQUIRE(block != nullptr);  // grows
        blocks.push_back(block);
    }
    CHECK(obs.grew_ >= 1);  // grew at least once past the initial 2

    for (void* const block : blocks) {
        opt->deallocate(block);
    }
}

TEST_CASE("InstrumentedPool::deallocate clamps live_ at zero on a foreign pointer (BUG-0002)") {
    InstrumentedPool pool{Pool(BLOCK_SIZE, 4U)};
    void* const a = pool.try_allocate();
    REQUIRE(a != nullptr);
    pool.deallocate(a);  // live_ -> 0

    int stack_var = 0;
    pool.deallocate(&stack_var);  // foreign pointer: core no-op (ADR-0012); must not underflow

    const PoolStats s = pool.stats();
    CHECK(s.live_ == 0U);           // clamped at zero, not wrapped to SIZE_MAX
    CHECK(s.deallocations_ == 2U);  // both non-null calls counted (documented meaning)
}

TEST_CASE("InstrumentedPool move-assignment notifies destroyed for the replaced pool (BUG-0003)") {
    RecordingObserver obs;  // out-lives both pools below
    InstrumentedPool dest{Pool(BLOCK_SIZE, 4U)};
    dest.add_observer(obs);

    dest = InstrumentedPool{Pool(BLOCK_SIZE, 4U)};  // replaces dest's pool
    CHECK(obs.destroyed_ == 1);                     // the replaced pool announced destruction
}
