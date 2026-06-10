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
 * adopted in Milestone 2.2 (ADR pending); function bodies arrive together
 * with the C implementation in Milestone 2. During Milestone 1 the class is
 * declared but not defined, so linking against any member function produces
 * an unresolved-symbol error — intentional, so consumers cannot accidentally
 * rely on a not-yet-implemented surface.
 */

#include <it/d4np/memorypool/memory_pool.h>

#include <cstddef>

namespace it::d4np::memorypool {

/**
 * @brief Owning, non-copyable, move-only wrapper around a `memory_pool_t*`.
 *
 * On construction the wrapper calls ::memory_pool_create; on destruction it
 * calls ::memory_pool_destroy. Allocation is exposed through ::allocate and
 * ::deallocate to match `std::allocator`-style naming, with the same
 * semantics as the underlying C calls.
 *
 * The exception policy at the C/C++ boundary (whether to translate `NULL`
 * returns into `std::bad_alloc`) is fixed in Milestone 3.1.
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
     */
    Pool(std::size_t block_size, std::size_t block_count);

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

private:
    memory_pool_t* handle_;
};

}  // namespace it::d4np::memorypool

#endif  // IT_D4NP_MEMORYPOOL_MEMORY_POOL_HPP_
