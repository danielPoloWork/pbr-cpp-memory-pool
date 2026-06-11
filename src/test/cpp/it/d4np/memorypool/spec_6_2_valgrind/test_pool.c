/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Daniel Polo
 *
 * Literal demonstrative test for spec section 6.2 (Valgrind verification).
 *
 * The spec command (verbatim from docs/specs/01_spec_cpp_memory_pool.md section 6.2):
 *
 *     gcc -g -O0 test_pool.c memory_pool.c -o test_pool
 *     valgrind --leak-check=full --show-leak-kinds=all ./test_pool
 *
 * Success criterion (verbatim): ERROR SUMMARY: 0 errors from 0 contexts.
 *
 * Our implementation is C++17 not C (ADR-0009 section 1 — backing acquired
 * via ::operator new(size, std::align_val_t)), so the literal command needs
 * two structural substitutions: g++ in place of gcc on the implementation
 * translation unit, and memory_pool.cpp in place of memory_pool.c on the
 * source name. The C test program below is unchanged ANSI C89. The exact
 * compile / link / run lines used by CI and reproducible locally are
 * documented in ./README.md.
 *
 * The test exercises the spec section 6.1 correctness scenarios that
 * intersect Valgrind's surface — exhaustion, null inputs, foreign
 * pointers — plus the create/destroy and alloc/free round-trips whose
 * spec section 3.1 contract ("all pre-allocated memory returned to the
 * OS") is exactly what Valgrind audits at process exit.
 *
 * Deliberately strict ANSI C89: no // comments, no inline, no _Bool, no
 * mixed declarations and code, no designated initialisers, no
 * variable-length arrays. Matches src/test/c/it/d4np/memorypool/c_consumer_min.c.
 */

#include <it/d4np/memorypool/memory_pool.h>

#include <stdlib.h>

#define BLOCK_SIZE ((size_t)64)
#define BLOCK_COUNT ((size_t)16)

int main(void) {
    memory_pool_t* pool;
    void* blocks[BLOCK_COUNT];
    void* exhaustion_extra;
    void* foreign_heap;
    int stack_local;
    size_t i;

    /* Spec section 6.1 — happy path. memory_pool_create returns a valid
       handle on the safe BLOCK_SIZE / BLOCK_COUNT combination. */
    pool = memory_pool_create(BLOCK_SIZE, BLOCK_COUNT);
    if (pool == NULL) {
        return 1;
    }

    /* Spec section 6.1 — null inputs are defined no-ops. Valgrind must
       see no invalid read or write from any of these calls. */
    memory_pool_free(NULL, NULL);
    memory_pool_free(pool, NULL);
    if (memory_pool_alloc(NULL) != NULL) {
        memory_pool_destroy(pool);
        return 2;
    }

    /* Spec section 6.1 — exhaustion. Pull every slot, prove the next
       call returns NULL, then return every block. The full cycle is the
       round-trip whose section 3.1 contract Valgrind audits at exit. */
    for (i = 0; i < BLOCK_COUNT; ++i) {
        blocks[i] = memory_pool_alloc(pool);
        if (blocks[i] == NULL) {
            memory_pool_destroy(pool);
            return 3;
        }
    }
    exhaustion_extra = memory_pool_alloc(pool);
    if (exhaustion_extra != NULL) {
        memory_pool_destroy(pool);
        return 4;
    }

    /* ADR-0012 — foreign-pointer probes. A heap pointer from malloc and
       a stack pointer must both no-op silently. Valgrind would surface
       any unguarded dereference inside the range check. */
    foreign_heap = malloc(BLOCK_SIZE);
    if (foreign_heap == NULL) {
        for (i = 0; i < BLOCK_COUNT; ++i) {
            memory_pool_free(pool, blocks[i]);
        }
        memory_pool_destroy(pool);
        return 5;
    }
    memory_pool_free(pool, foreign_heap);
    memory_pool_free(pool, &stack_local);
    free(foreign_heap);

    /* Return every outstanding block, then re-allocate the full pool,
       then return every block again. Two full alloc/free cycles prove
       the implicit free list rebuilds cleanly and that destroy sees a
       full free list at the end. */
    for (i = 0; i < BLOCK_COUNT; ++i) {
        memory_pool_free(pool, blocks[i]);
    }
    for (i = 0; i < BLOCK_COUNT; ++i) {
        blocks[i] = memory_pool_alloc(pool);
        if (blocks[i] == NULL) {
            memory_pool_destroy(pool);
            return 6;
        }
    }
    for (i = 0; i < BLOCK_COUNT; ++i) {
        memory_pool_free(pool, blocks[i]);
    }

    /* Spec section 3.1 — destroy releases every byte of pre-allocated
       backing storage back to the OS. Valgrind audits the final HEAP
       SUMMARY at process exit; the success criterion is the literal
       sentence "ERROR SUMMARY: 0 errors from 0 contexts". */
    memory_pool_destroy(pool);

    return 0;
}
