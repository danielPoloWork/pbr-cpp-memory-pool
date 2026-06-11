// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_MEMORY_POOL_HPP_
#define IT_D4NP_MEMORYPOOL_MEMORY_POOL_HPP_

/**
 * @file memory_pool.hpp
 * @brief C++17 RAII wrapper around the C memory pool.
 *
 * Layers an owning, exception-safe handle on top of the C API declared in
 * `<it/d4np/memorypool/memory_pool.h>`. The wrapper is the **RAII** pattern
 * adopted in ADR-0010; bodies live alongside the C implementation in
 * `memory_pool.cpp` and forward to the four spec §5 functions. The
 * wrapper is deliberately minimal — ADR-0010 §2 ("the deliberately small
 * surface") — additional ergonomics (typed allocation,
 * `std::allocator`-aware adapter, diagnostic iterators) arrive in
 * Milestone 3.
 */

#include <it/d4np/memorypool/memory_pool.h>

#include <cstddef>
#include <optional>

namespace it::d4np::memorypool {

/**
 * @brief Owning, non-copyable, move-only wrapper around a `memory_pool_t*`.
 *
 * On construction the wrapper calls ::memory_pool_create; on destruction it
 * calls ::memory_pool_destroy. Allocation is exposed through ::allocate and
 * ::deallocate to match `std::allocator`-style naming, with the same
 * semantics as the underlying C calls.
 *
 * Layout is exactly `sizeof(void*)` — a single `memory_pool_t*` data
 * member; the C handle is the Pimpl (ADR-0010). Copy operations are
 * deleted (the pool handle has unique ownership of its backing buffer;
 * a copy would double-free at destruction). Move operations leave the
 * source in a valid empty state (`handle_ == nullptr`) so its destructor
 * is a safe no-op.
 *
 * The exception policy at the C/C++ boundary (whether to translate `NULL`
 * returns from `memory_pool_create` into `std::bad_alloc`) is deferred to
 * Milestone 3.1; until then a failed `memory_pool_create` leaves the
 * wrapper's handle null and ::allocate returns `nullptr`.
 */
class Pool {
public:
    /**
     * Construct a pool with @p block_count blocks of @p block_size bytes each.
     *
     * @param block_size  Size of each block in bytes. Must satisfy ADR-0009 §2:
     *                    `block_size >= sizeof(void*)` and `block_size` is a
     *                    multiple of `alignof(std::max_align_t)`.
     * @param block_count Number of blocks the pool can vend; must be greater
     *                    than zero, and `block_size * block_count` must not
     *                    overflow `size_t` (ADR-0009 §3).
     *
     * @throw std::bad_alloc when backing-storage allocation fails (post
     *        Milestone 3.1; until then construction silently leaves the
     *        handle in a null state and ::allocate returns `nullptr`).
     *
     * @note Callers wanting an explicit yes/no signal at the construction
     *       expression should use ::make instead — see ADR-0011.
     */
    Pool(std::size_t block_size, std::size_t block_count);

    /**
     * Factory function returning an engaged `std::optional<Pool>` on
     * successful construction or `std::nullopt` on failure (any precondition
     * violation from ADR-0009 §2 / §3, or backing-storage allocation
     * failure). See ADR-0011 §1 for the design rationale and the comparison
     * with direct ctor invocation.
     *
     * @param block_size  Same contract as the ctor — ADR-0009 §2.
     * @param block_count Same contract as the ctor — ADR-0009 §3.
     */
    [[nodiscard]] static std::optional<Pool> make(std::size_t block_size, std::size_t block_count);

    /** Destroy the pool, releasing all backing storage to the OS. */
    ~Pool() noexcept;

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    /** Move-construct from @p other, leaving @p other in a valid empty state. */
    Pool(Pool&& other) noexcept;

    /** Move-assign from @p other, releasing the current pool first. */
    Pool& operator=(Pool&& other) noexcept;

    /**
     * Allocate one block in O(1).
     *
     * @return Pointer to the block, or `nullptr` when the pool is exhausted.
     *         Post-Milestone 3.1 this may instead throw `std::bad_alloc`
     *         depending on the configured exception policy.
     */
    void* allocate();

    /**
     * Return a previously allocated block to the pool in O(1).
     *
     * @param block Block to release, or `nullptr` (no-op).
     */
    void deallocate(void* block) noexcept;

    /**
     * @return The underlying C handle. Useful for interop with code that
     *         only knows the C API. The wrapper retains ownership.
     */
    memory_pool_t* native_handle() noexcept;

    /**
     * Report the per-pool metadata overhead in bytes (spec §3.2 / ADR-0015).
     *
     * Forwards to ::memory_pool_metadata_bytes. The returned value is O(1)
     * in both block_count and block_size — a pool with one million blocks
     * reports the same number as a pool with one. Per-block external
     * metadata is zero by construction (implicit free list, ADR-0009 §1).
     *
     * @return Bytes of pool-internal metadata, or 0 on a moved-from wrapper
     *         whose handle is null.
     */
    [[nodiscard]] std::size_t metadata_bytes() const noexcept;

private:
    memory_pool_t* handle_;
};

/**
 * @brief Fluent builder for configured `Pool` instances.
 *
 * Accumulates configuration through chainable `with_*` setters and produces
 * a `std::optional<Pool>` on `build()`. The Builder pattern absorbs the
 * future configuration-explosion (thread-safety strategy from Milestone 4,
 * dynamic-growth policy from Milestone 5) without requiring ABI changes to
 * the `Pool` ctor or to `Pool::make`. Adopted in ADR-0011 §2.
 *
 * Usage:
 * @code
 *   if (auto pool = PoolBuilder{}
 *                        .with_block_size(64)
 *                        .with_block_count(1024)
 *                        .build();
 *       pool) {
 *       void* block = pool->allocate();
 *       // ...
 *   }
 * @endcode
 *
 * `build()` is `const`, so a configured builder can produce multiple
 * identically-configured pools (useful for tests and for benchmark setup).
 * A default-constructed builder has `block_size_ == 0` and
 * `block_count_ == 0`; calling `build()` on it returns `std::nullopt` —
 * the deliberate fail-loud behaviour for forgotten configuration.
 */
class PoolBuilder {
public:
    /** Set the per-block size in bytes; must satisfy ADR-0009 §2 at build time. */
    PoolBuilder& with_block_size(std::size_t block_size) noexcept;

    /** Set the block count; must satisfy ADR-0009 §3 at build time. */
    PoolBuilder& with_block_count(std::size_t block_count) noexcept;

    /** Produce a `std::optional<Pool>` from the accumulated configuration. */
    [[nodiscard]] std::optional<Pool> build() const;

private:
    std::size_t block_size_ = 0;
    std::size_t block_count_ = 0;
};

}  // namespace it::d4np::memorypool

#endif  // IT_D4NP_MEMORYPOOL_MEMORY_POOL_HPP_
