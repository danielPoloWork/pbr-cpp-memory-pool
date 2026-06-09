/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Daniel Polo
 */

#ifndef IT_D4NP_MEMORYPOOL_MEMORY_POOL_H_
#define IT_D4NP_MEMORYPOOL_MEMORY_POOL_H_

/**
 * @file memory_pool.h
 * @brief Public C API for the pbr-cpp-memory-pool fixed-block-size allocator.
 *
 * This header is the C-language contract surface defined by spec section 5.
 * It is held to ANSI C (C89) compatibility per ADR-0005 section 3; a CI job
 * (ROADMAP item 1.10) compiles it under `-std=c89 -pedantic -Werror` and
 * `-std=c99 -pedantic -Werror`. Avoid C99-or-later constructs (inline,
 * designated initialisers, `_Bool`, single-line `//` comments outside of
 * other headers, mixed declarations and code) in this file.
 *
 * Implementations of every function declared here arrive in Milestone 2;
 * during Milestone 1 the library is a header-only INTERFACE target so any
 * consumer that calls these functions will get a link error until then.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to a memory pool instance. */
typedef struct memory_pool memory_pool_t;

/**
 * Create a memory pool able to vend @p block_count blocks of @p block_size
 * bytes each. Memory is allocated contiguously and pre-populated as a free
 * list per spec section 4.
 *
 * @param block_size  Size of each block in bytes. Must be at least
 *                    `sizeof(void*)` so the free-list link fits inside a
 *                    free block. See ADR (to be filed in Milestone 2.1).
 * @param block_count Number of blocks the pool can vend.
 *
 * @return Pointer to the newly created pool, or `NULL` on invalid arguments
 *         or backing-storage allocation failure.
 */
memory_pool_t* memory_pool_create(size_t block_size, size_t block_count);

/**
 * Allocate one block from @p pool in O(1).
 *
 * @param pool Pool returned by ::memory_pool_create. Must not be `NULL`.
 *
 * @return Pointer to a block of `block_size` bytes, or `NULL` when the pool
 *         is exhausted (fixed-size mode) or, post-Milestone 5, when dynamic
 *         growth itself fails. The pointer is aligned per ADR (Milestone 2.1).
 */
void* memory_pool_alloc(memory_pool_t* pool);

/**
 * Return a previously allocated block to @p pool in O(1).
 *
 * Passing `NULL` for @p block is a no-op. Passing a pointer not previously
 * returned by ::memory_pool_alloc on the same @p pool is undefined behaviour;
 * the policy for detecting and reporting such misuse is set in Milestone 2.7.
 *
 * @param pool  Pool the block originally came from.
 * @param block Block to release, or `NULL`.
 */
void memory_pool_free(memory_pool_t* pool, void* block);

/**
 * Destroy @p pool and release every byte of pre-allocated backing storage
 * back to the operating system, per spec section 3.1.
 *
 * Passing `NULL` is a no-op. After this call, @p pool must not be reused.
 *
 * @param pool Pool to destroy, or `NULL`.
 */
void memory_pool_destroy(memory_pool_t* pool);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* IT_D4NP_MEMORYPOOL_MEMORY_POOL_H_ */
