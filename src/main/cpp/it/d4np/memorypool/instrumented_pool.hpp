// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_INSTRUMENTED_POOL_HPP_
#define IT_D4NP_MEMORYPOOL_INSTRUMENTED_POOL_HPP_

/**
 * @file instrumented_pool.hpp
 * @brief Instrumented pool variant — the Decorator pattern (ADR-0025).
 *
 * `InstrumentedPool` composes a `Pool` (ADR-0010) and re-exposes its surface,
 * counting allocations / deallocations / failures and tracking the live-block
 * high-water mark. It is the **Decorator** in its idiomatic-C++ (composition)
 * form: `Pool` is a concrete, move-only value type with no virtual surface, so
 * the decorator wraps it rather than inheriting (as `TypedPool` / `PoolAllocator`
 * do — ADR-0017 / ADR-0018).
 *
 * Instrumentation is **opt-in by type**: a program that uses `Pool` directly
 * pays nothing (no counter, no branch, no atomic) — the ROADMAP §6 zero-overhead
 * goal holds by construction (verified in M6.3). The counters are relaxed
 * atomics, so the decorator is safe to wrap a thread-safe (`MUTEX`/`LOCKFREE`)
 * pool and drive it concurrently; under contention the live/peak high-water
 * mark is an approximate, eventually-consistent diagnostic.
 *
 * Header-only; adds zero object code and zero per-pool metadata to the library.
 * Per-event lifecycle notification (exhaustion / growth / destruction) is the
 * Observer pattern added in M6.2, not here.
 */

#include <it/d4np/memorypool/memory_pool.hpp>

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <ostream>
#include <utility>

namespace it::d4np::memorypool {

/**
 * @brief Copyable snapshot of an `InstrumentedPool`'s counters (ADR-0025 §2).
 *
 * A plain value type returned by `InstrumentedPool::stats()`. Members carry the
 * project's trailing-underscore suffix even though it is a public aggregate.
 */
struct PoolStats {
    std::size_t allocations_;          ///< successful allocations
    std::size_t deallocations_;        ///< deallocate calls with a non-null block
    std::size_t allocation_failures_;  ///< allocate/try_allocate that found the pool exhausted
    std::size_t live_;                 ///< currently outstanding blocks
    std::size_t peak_live_;            ///< high-water mark of `live_`
};

/**
 * @brief Move-only Decorator over `Pool` that instruments allocation activity.
 *
 * Mirrors the `Pool` allocation surface (ADR-0016 verbs) and counts it. Copy is
 * deleted (it owns a move-only `Pool`); move is hand-written because the atomic
 * counters are not move-constructible.
 */
class InstrumentedPool {
public:
    /** Adopt @p pool by move and start instrumenting it. */
    explicit InstrumentedPool(Pool&& pool) noexcept : pool_(std::move(pool)) {}

    /** Factory mirroring `Pool::make` — `std::nullopt` on construction failure. */
    [[nodiscard]] static std::optional<InstrumentedPool> make(std::size_t block_size, std::size_t block_count) {
        std::optional<Pool> pool = Pool::make(block_size, block_count);
        if (!pool.has_value()) {
            return std::nullopt;
        }
        return {InstrumentedPool{std::move(*pool)}};
    }

    /** Factory mirroring `Pool::make_dynamic` (ADR-0024) — `std::nullopt` on failure. */
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] static std::optional<InstrumentedPool> make_dynamic(std::size_t block_size, std::size_t block_count,
                                                                      std::size_t growth_factor) {
        std::optional<Pool> pool = Pool::make_dynamic(block_size, block_count, growth_factor);
        if (!pool.has_value()) {
            return std::nullopt;
        }
        return {InstrumentedPool{std::move(*pool)}};
    }

    InstrumentedPool(const InstrumentedPool&) = delete;
    InstrumentedPool& operator=(const InstrumentedPool&) = delete;

    /** Move-construct; the atomic counters are loaded and re-seeded (ADR-0025 §2). */
    InstrumentedPool(InstrumentedPool&& other) noexcept
        : pool_(std::move(other.pool_)), allocations_(other.allocations_.load(std::memory_order_relaxed)),
          deallocations_(other.deallocations_.load(std::memory_order_relaxed)),
          allocation_failures_(other.allocation_failures_.load(std::memory_order_relaxed)),
          live_(other.live_.load(std::memory_order_relaxed)),
          peak_live_(other.peak_live_.load(std::memory_order_relaxed)) {}

    /** Move-assign; releases the current pool and re-seeds the counters. */
    InstrumentedPool& operator=(InstrumentedPool&& other) noexcept {
        if (this != &other) {
            pool_ = std::move(other.pool_);
            allocations_.store(other.allocations_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            deallocations_.store(other.deallocations_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            allocation_failures_.store(other.allocation_failures_.load(std::memory_order_relaxed),
                                       std::memory_order_relaxed);
            live_.store(other.live_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            peak_live_.store(other.peak_live_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    ~InstrumentedPool() = default;

    /** Throwing allocation verb (ADR-0016 §2). Counts the success, or the failure on `std::bad_alloc`. */
    [[nodiscard]] void* allocate() {
        void* block = nullptr;
        try {
            block = pool_.allocate();
        } catch (const std::bad_alloc&) {
            allocation_failures_.fetch_add(1U, std::memory_order_relaxed);
            throw;
        }
        record_allocation();
        return block;
    }

    /** Non-throwing allocation verb (ADR-0016 §2). Counts success or in-band failure. */
    [[nodiscard]] void* try_allocate() noexcept {
        void* const block = pool_.try_allocate();
        if (block == nullptr) {
            allocation_failures_.fetch_add(1U, std::memory_order_relaxed);
            return nullptr;
        }
        record_allocation();
        return block;
    }

    /** Return a block; counts the deallocation for any non-null pointer. */
    void deallocate(void* block) noexcept {
        if (block != nullptr) {
            deallocations_.fetch_add(1U, std::memory_order_relaxed);
            live_.fetch_sub(1U, std::memory_order_relaxed);
        }
        pool_.deallocate(block);
    }

    /** @return A snapshot of the current counters (ADR-0025 §2). */
    [[nodiscard]] PoolStats stats() const noexcept {
        return PoolStats{allocations_.load(std::memory_order_relaxed), deallocations_.load(std::memory_order_relaxed),
                         allocation_failures_.load(std::memory_order_relaxed), live_.load(std::memory_order_relaxed),
                         peak_live_.load(std::memory_order_relaxed)};
    }

    /** Write a one-line human-readable summary of the counters to @p os. */
    void write_summary(std::ostream& os) const {
        const PoolStats snapshot = stats();
        os << "InstrumentedPool: allocations=" << snapshot.allocations_ << " deallocations=" << snapshot.deallocations_
           << " failures=" << snapshot.allocation_failures_ << " live=" << snapshot.live_
           << " peak_live=" << snapshot.peak_live_ << "\n";
    }

    /** @return The underlying C handle (the decorator retains ownership). */
    [[nodiscard]] memory_pool_t* native_handle() noexcept {
        return pool_.native_handle();
    }

    /** @return The configured per-block size in bytes (ADR-0018 §3). */
    [[nodiscard]] std::size_t block_size() const noexcept {
        return pool_.block_size();
    }

    /** @return Per-pool metadata overhead in bytes (ADR-0015). */
    [[nodiscard]] std::size_t metadata_bytes() const noexcept {
        return pool_.metadata_bytes();
    }

private:
    /** Common post-allocation bookkeeping: bump live and lift the high-water mark. */
    void record_allocation() noexcept {
        allocations_.fetch_add(1U, std::memory_order_relaxed);
        const std::size_t live = live_.fetch_add(1U, std::memory_order_relaxed) + 1U;
        // Relaxed compare-exchange max — lift peak_live_ to `live` if it is behind.
        std::size_t peak = peak_live_.load(std::memory_order_relaxed);
        while (peak < live && !peak_live_.compare_exchange_weak(peak, live, std::memory_order_relaxed)) {
            // peak is reloaded by compare_exchange_weak on failure; retry.
        }
    }

    Pool pool_;
    std::atomic<std::size_t> allocations_{0U};
    std::atomic<std::size_t> deallocations_{0U};
    std::atomic<std::size_t> allocation_failures_{0U};
    std::atomic<std::size_t> live_{0U};
    std::atomic<std::size_t> peak_live_{0U};
};

}  // namespace it::d4np::memorypool

#endif  // IT_D4NP_MEMORYPOOL_INSTRUMENTED_POOL_HPP_
