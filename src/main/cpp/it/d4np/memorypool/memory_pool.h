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

/*
 * Diagnostics gate (ADR-0019 §1). A single macro controls the entire
 * free-list diagnostic surface: the C accessors below and the C++
 * `free_list_iterator.hpp` iterator. The default is build-type driven so
 * the surface is available in debug builds and compiled out of release
 * builds; an explicit definition (e.g. the CMake option
 * PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS, which defines it to 1 PUBLIC-ly on
 * the library target) always wins. Walking the free list is O(free_count)
 * and has no place on the allocation hot path — hence "disabled in release
 * unless explicitly enabled" (ROADMAP 3.4).
 */
#ifndef PBR_MEMORY_POOL_DIAGNOSTICS
#  ifdef NDEBUG
#    define PBR_MEMORY_POOL_DIAGNOSTICS 0
#  else
#    define PBR_MEMORY_POOL_DIAGNOSTICS 1
#  endif
#endif

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
 * Report the configured per-block size of @p pool in bytes (ADR-0018 §3).
 *
 * Returns the `block_size` value @p pool was created with — the byte size
 * of every block ::memory_pool_alloc vends. The value is fixed for the
 * pool's lifetime and the call is `O(1)`. This is the introspection
 * companion to ::memory_pool_metadata_bytes; the STL allocator adapter
 * (ADR-0018) uses it to decide whether a given object size fits in a
 * single block before routing a request to the pool.
 *
 * @param pool Pool to inspect, or `NULL`.
 * @return The pool's `block_size` in bytes, or 0 if @p pool is `NULL`.
 */
size_t memory_pool_block_size(const memory_pool_t* pool);

#if PBR_MEMORY_POOL_DIAGNOSTICS

/**
 * Diagnostics — return the head of @p pool's implicit free list (ADR-0019
 * §2). This is the address of the first free slot, or `NULL` when the pool
 * is exhausted or @p pool is `NULL`. Read-only; never mutate the storage
 * the returned pointer addresses.
 *
 * Available only when `PBR_MEMORY_POOL_DIAGNOSTICS` is non-zero (the
 * default in debug builds; opt-in in release builds). Intended for tests
 * and diagnostics — the walk it begins is `O(free_count)`.
 *
 * @param pool Pool to inspect, or `NULL`.
 * @return Address of the first free slot, or `NULL`.
 */
const void* memory_pool_debug_free_list_head(const memory_pool_t* pool);

/**
 * Diagnostics — given a free slot @p current obtained from
 * ::memory_pool_debug_free_list_head or a previous call to this function,
 * return the next free slot in the implicit free list, or `NULL` at the
 * end of the list (ADR-0019 §2). The next-free link is read from inside
 * @p current per the ADR-0009 §1 layout, which stays encapsulated in the
 * implementation.
 *
 * @param pool    Pool the slot belongs to, or `NULL` (returns `NULL`).
 * @param current A free slot from this pool's list, or `NULL` (returns
 *                `NULL`).
 * @return Address of the next free slot, or `NULL` at end of list.
 */
const void* memory_pool_debug_free_list_next(const memory_pool_t* pool, const void* current);

/**
 * Diagnostics — count the free slots currently in @p pool's free list by
 * walking it in `O(free_count)` (ADR-0019 §2). Equivalent to
 * `std::distance(begin, end)` over the C++ `FreeListView`; the test suite
 * cross-checks the two.
 *
 * @param pool Pool to inspect, or `NULL`.
 * @return Number of free slots, or 0 when @p pool is `NULL`.
 */
size_t memory_pool_debug_free_count(const memory_pool_t* pool);

#endif /* PBR_MEMORY_POOL_DIAGNOSTICS */

#ifdef __cplusplus
}
#endif

#endif
