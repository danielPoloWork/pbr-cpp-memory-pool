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
/* The struct tag is snake_case to match the C convention for forward-
 * declared opaque types; the .clang-tidy StructCase rule (CamelCase) is
 * the right default for new C++ types, suppressed here for the C-interop
 * boundary per the same rationale recorded at the definition site in
 * memory_pool.cpp. */
/* NOLINTNEXTLINE(readability-identifier-naming) */
typedef struct memory_pool memory_pool_t;

/**
 * Create a memory pool able to vend @p block_count blocks of @p block_size
 * bytes each. Memory is allocated contiguously and pre-populated as a free
 * list per spec section 4. The full layout and validation contract is
 * recorded in ADR-0009 (`docs/adr/0009-...`); the summary below is binding.
 *
 * @param block_size  Size of each block in bytes. ADR-0009 §2 requires all
 *                    of:
 *                      - `block_size > 0`
 *                      - `block_size >= sizeof(void*)` (the free-list link
 *                        must fit in a free slot)
 *                      - `block_size` is a multiple of `alignof(max_align_t)`
 *                        (drop-in `malloc`-parity alignment, ADR-0009 §5).
 *                    Any violation makes the call return `NULL`; the
 *                    implementation never silently rounds up.
 * @param block_count Number of blocks the pool can vend. Must be greater
 *                    than zero. ADR-0009 §3 additionally requires that
 *                    `block_size * block_count` not overflow `size_t`;
 *                    overflow is treated as an argument-validation
 *                    failure and returns `NULL`.
 *
 * @return Pointer to the newly created pool, or `NULL` on any precondition
 *         violation or on backing-storage allocation failure. Allocation
 *         failure inside the implementation never propagates as a C++
 *         exception across this C ABI boundary (ADR-0005 §3 + ADR-0009 §7).
 */
memory_pool_t* memory_pool_create(size_t block_size, size_t block_count);

/**
 * Allocate one block from @p pool in O(1).
 *
 * @param pool Pool returned by ::memory_pool_create. Passing `NULL` is
 *             defined and returns `NULL`.
 *
 * @return Pointer to a block of `block_size` bytes, or `NULL` when the pool
 *         is exhausted (fixed-size mode) or, post-Milestone 5, when dynamic
 *         growth itself fails. The pointer is aligned to
 *         `alignof(max_align_t)` — drop-in parity with `malloc` per
 *         ADR-0009 §5.
 */
void* memory_pool_alloc(memory_pool_t* pool);

/**
 * Return a previously allocated block to @p pool in O(1).
 *
 * Per ADR-0012, the function is a defined no-op in three cases that the
 * caller might pass by mistake:
 *
 *   - @p pool is `NULL`;
 *   - @p block is `NULL`;
 *   - @p block is a foreign pointer — outside `pool`'s backing buffer or
 *     in-range but not aligned to a slot boundary.
 *
 * Detection of the foreign-pointer case is an `O(1)` range + alignment
 * check against the pool's backing extents (ADR-0009 §6 fields). The
 * pool state is bit-identical before and after a no-op call. Note that
 * the policy does NOT detect *double-free* on a legitimately-in-range,
 * already-on-the-free-list pointer; that case remains undefined
 * behaviour and is addressed by the optional Decorator instrumentation
 * landing in Milestone 6.
 *
 * @param pool  Pool the block originally came from.
 * @param block Block to release, or `NULL`, or a foreign pointer.
 */
void memory_pool_free(memory_pool_t* pool, void* block);

/**
 * Destroy @p pool and release every byte of pre-allocated backing storage
 * back to the operating system, per spec section 3.1 and ADR-0009 §7.
 *
 * The backing buffer is released through the matching aligned
 * `::operator delete` overload of the C++17 `::operator new(size,
 * std::align_val_t)` used at creation; the metadata struct is released
 * via the matching plain `delete`. Passing `NULL` is a no-op. After this
 * call, @p pool must not be reused.
 *
 * @param pool Pool to destroy, or `NULL`.
 */
void memory_pool_destroy(memory_pool_t* pool);

/**
 * Report the per-pool metadata overhead in bytes (spec section 3.2 /
 * ADR-0015).
 *
 * Returns the size of pool-internal bookkeeping — currently the
 * `struct memory_pool` itself, per ADR-0009 section 6. The value is O(1)
 * in both `block_count` and `block_size`: a pool with one million blocks
 * reports the same number as a pool with one. Per-block metadata is
 * zero by construction (implicit free list, ADR-0009 section 1).
 *
 * The CI build matrix gates `sizeof(struct memory_pool)` to a 128-byte
 * upper bound through a `static_assert` in the implementation file
 * (ADR-0015 section 3); this function reports the value at runtime so
 * test code and production diagnostics can verify the budget against
 * the same number that gates compile time.
 *
 * @param pool Pool to inspect, or `NULL`.
 * @return Number of metadata bytes for @p pool, or 0 if @p pool is
 *         `NULL`.
 */
size_t memory_pool_metadata_bytes(const memory_pool_t* pool);

/**
 * Report the configured per-block size of @p pool in bytes (ADR-0018
 * section 2).
 *
 * The value is exactly the `block_size` argument accepted by
 * ::memory_pool_create — the implementation never silently rounds
 * (ADR-0009 section 2), so the reported size is also the caller's
 * original request. Consumers use it for capacity planning and, in the
 * STL allocator adapter, to decide whether a type fits a pool slot.
 *
 * @param pool Pool to inspect, or `NULL`.
 * @return The per-block size in bytes, or 0 if @p pool is `NULL`.
 */
size_t memory_pool_block_size(const memory_pool_t* pool);

/**
 * Report whether @p block points at a slot of @p pool (ADR-0018
 * section 2).
 *
 * This is the ADR-0012 O(1) range + alignment check promoted to public
 * API: the result is 1 if and only if @p block lies inside the pool's
 * contiguous backing buffer at an exact slot boundary. The predicate
 * reports *address ownership*, NOT allocation state — it cannot
 * distinguish a currently-allocated slot from a free one (the same
 * limitation recorded for double-free detection in ADR-0012; the
 * optional Milestone 6 instrumentation addresses it).
 *
 * Never dereferences @p block, so it is safe to probe arbitrary
 * pointers, including foreign heap and stack addresses.
 *
 * @param pool  Pool whose extents are checked, or `NULL` (returns 0).
 * @param block Pointer to test, or `NULL` (returns 0).
 * @return 1 when @p block is a slot of @p pool, 0 otherwise.
 */
int memory_pool_owns(const memory_pool_t* pool, const void* block);

#ifdef __cplusplus
}
#endif

#endif
