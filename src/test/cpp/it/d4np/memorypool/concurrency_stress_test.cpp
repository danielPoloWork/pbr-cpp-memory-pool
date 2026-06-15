// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file concurrency_stress_test.cpp
 * @brief Concurrent stress tests for the thread-safe pool policies (M4.4).
 *
 * Exercises `Pool` under contention from multiple threads to validate the
 * `MutexPolicy` and `LockFreePolicy` implementations (ADR-0020). The whole
 * suite is gated behind `PBR_MEMORY_POOL_THREAD_SAFETY != NONE`: under the
 * default single-threaded build the pool is intentionally racy (spec §2.4),
 * so the concurrent cases must not run — a placeholder case keeps the binary
 * valid. CMake mirrors the library's thread-safety mode onto this target so
 * the gate sees the same value the library was built with.
 *
 * The cases check three invariants:
 *   - **No over-vend / distinctness:** a concurrent drain hands out exactly
 *     `block_count` blocks, every one distinct.
 *   - **Full recovery / no leak:** after heavy concurrent alloc/free churn,
 *     the pool vends exactly `block_count` distinct blocks again.
 *   - **Exclusive ownership:** a block held by one thread is never written by
 *     another (no double-vend), checked by a per-thread byte marker.
 *
 * **TSan note (M4.4):** the dedicated ThreadSanitizer CI job runs this suite
 * under `MUTEX`. It is deliberately *not* run under `LOCKFREE`: the
 * Treiber-stack pop reads a node's next-link that another thread may have
 * already recycled (a benign race — the value is discarded when the tagged
 * CAS fails, and the pool's backing is never unmapped, so the read is always
 * of valid memory). Expressing that as a well-defined relaxed atomic needs
 * C++20 `std::atomic_ref` or hazard pointers, both out of scope here; so
 * LOCKFREE concurrent correctness is covered by these logical invariants
 * (run under the non-sanitized `thread-safety` CI job) plus the ADR-0020 §3
 * correctness argument, while TSan verifies the data-race-free `MutexPolicy`.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/instrumented_pool.hpp>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <optional>
#include <set>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#if PBR_MEMORY_POOL_THREAD_SAFETY != PBR_MEMORY_POOL_THREAD_SAFETY_NONE

using it::d4np::memorypool::InstrumentedPool;
using it::d4np::memorypool::Pool;

namespace {

// Fixed thread count keeps the test deterministic; 8 is enough to contend
// the head on the 2–4 core CI runners.
constexpr unsigned THREAD_COUNT = 8U;
constexpr std::size_t BLOCK_SIZE = 64U;

}  // namespace

TEST_CASE("concurrent drain vends exactly block_count distinct blocks (no over-vend)") {
    constexpr std::size_t BLOCK_COUNT = 4096U;
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    std::vector<std::vector<void*>> grabbed(THREAD_COUNT);
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (unsigned t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&pool, &grabbed, t] {
            std::vector<void*>& mine = grabbed.at(t);
            for (;;) {
                void* const block = pool.try_allocate();
                if (block == nullptr) {
                    break;
                }
                mine.push_back(block);
            }
        });
    }
    for (std::thread& th : threads) {
        th.join();
    }

    std::set<void*> distinct;
    std::size_t total = 0U;
    for (const std::vector<void*>& v : grabbed) {
        total += v.size();
        for (void* const p : v) {
            distinct.insert(p);
        }
    }
    // Exactly block_count handed out, and every one distinct — no double-vend.
    CHECK(total == BLOCK_COUNT);
    CHECK(distinct.size() == BLOCK_COUNT);

    for (const std::vector<void*>& v : grabbed) {
        for (void* const p : v) {
            pool.deallocate(p);
        }
    }
}

TEST_CASE("pool fully recovers after concurrent alloc/free churn (no leak)") {
    constexpr std::size_t BLOCK_COUNT = 512U;
    constexpr int ITERATIONS = 20000;
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (unsigned t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&pool] {
            for (int i = 0; i < ITERATIONS; ++i) {
                void* const block = pool.try_allocate();
                if (block != nullptr) {
                    // Touch the whole block while it is exclusively owned, so
                    // a double-vend would surface as a clobbered marker / a
                    // TSan race under MutexPolicy.
                    std::memset(block, 0xAB, BLOCK_SIZE);
                    pool.deallocate(block);
                }
            }
        });
    }
    for (std::thread& th : threads) {
        th.join();
    }

    // Nothing leaked: the pool vends exactly block_count distinct blocks again.
    std::set<void*> distinct;
    std::vector<void*> held;
    for (;;) {
        void* const block = pool.try_allocate();
        if (block == nullptr) {
            break;
        }
        held.push_back(block);
        distinct.insert(block);
    }
    CHECK(held.size() == BLOCK_COUNT);
    CHECK(distinct.size() == BLOCK_COUNT);

    for (void* const p : held) {
        pool.deallocate(p);
    }
}

TEST_CASE("a vended block is exclusively owned while held (no double-vend)") {
    constexpr std::size_t BLOCK_COUNT = 256U;
    constexpr int ITERATIONS = 20000;
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    std::atomic<bool> violated{false};
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (unsigned t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&pool, &violated, t] {
            const auto mark = static_cast<unsigned char>(t + 1U);
            std::array<unsigned char, BLOCK_SIZE> expected{};
            expected.fill(mark);
            for (int i = 0; i < ITERATIONS; ++i) {
                void* const block = pool.try_allocate();
                if (block == nullptr) {
                    continue;
                }
                std::memset(block, mark, BLOCK_SIZE);
                std::this_thread::yield();  // widen any race window
                if (std::memcmp(block, expected.data(), BLOCK_SIZE) != 0) {
                    violated.store(true, std::memory_order_relaxed);
                }
                pool.deallocate(block);
            }
        });
    }
    for (std::thread& th : threads) {
        th.join();
    }

    CHECK_FALSE(violated.load(std::memory_order_relaxed));
}

TEST_CASE("concurrent InstrumentedPool over a dynamic pool is race-free on the growth counter (BUG-0001)") {
    std::optional<InstrumentedPool> opt = InstrumentedPool::make_dynamic(BLOCK_SIZE, 64U, 2U);
    if (!opt.has_value()) {
        return;  // LOCKFREE: dynamic mode rejected (ADR-0024 §2) — no growth path to race on
    }
    InstrumentedPool& pool = *opt;

    // Drive concurrent allocations that force repeated growth. Before the fix,
    // notify_if_grew() read+wrote the non-atomic last_growths_ on this hot path;
    // ThreadSanitizer flags that race under MUTEX. Each thread keeps its blocks so
    // nothing is freed mid-flight (the focus is the growth-counter access).
    constexpr int PER_THREAD = 2000;
    std::vector<std::vector<void*>> grabbed(THREAD_COUNT);
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (unsigned t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&pool, &grabbed, t] {
            std::vector<void*>& mine = grabbed.at(t);
            for (int i = 0; i < PER_THREAD; ++i) {
                void* const block = pool.try_allocate();
                if (block != nullptr) {
                    mine.push_back(block);
                }
            }
        });
    }
    for (std::thread& th : threads) {
        th.join();
    }

    CHECK(pool.stats().allocation_failures_ == 0U);  // dynamic: grows rather than failing

    for (const std::vector<void*>& v : grabbed) {
        for (void* const p : v) {
            pool.deallocate(p);
        }
    }
}

#else  // PBR_MEMORY_POOL_THREAD_SAFETY != NONE

TEST_CASE("concurrency stress is gated to thread-safe builds") {
    // The default single-threaded build is intentionally racy (spec §2.4);
    // the concurrent cases run only under MUTEX / LOCKFREE. This placeholder
    // keeps the binary and its CTest registration valid.
    CHECK(true);
}

#endif  // PBR_MEMORY_POOL_THREAD_SAFETY != NONE
