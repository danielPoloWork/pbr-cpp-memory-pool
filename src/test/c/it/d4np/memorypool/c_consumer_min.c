/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Daniel Polo
 *
 * Minimal C translation unit exercising the public C API of pbr-cpp-memory-pool.
 *
 * Compiled by the ANSI-C / C99 verification jobs in .github/workflows/ci.yml
 * under `-std=c89 -pedantic -Werror` AND `-std=c99 -pedantic -Werror` (ADR-0005
 * section 3, ROADMAP section 1.10). This file exists *only* to keep the C
 * compatibility contract from drifting: if a future change to memory_pool.h
 * accidentally introduces a C99-or-later construct, this file stops compiling
 * under C89 and CI fails.
 *
 * Deliberately avoids any C99-or-later features: no // comments, no inline,
 * no _Bool, no mixed declarations and code, no designated initialisers,
 * no compound literals.
 */

#include <it/d4np/memorypool/memory_pool.h>

int main(void) {
    memory_pool_t* pool;
    void* block;

    pool = memory_pool_create(64, 16);

    block = memory_pool_alloc(pool);
    memory_pool_free(pool, block);

    memory_pool_destroy(pool);

    /* M1 stubs return NULL, so block == NULL is the expected state; we never
     * dereference it. The verification job's success criterion is "compiles
     * cleanly under -pedantic -Werror", not "runs to a specific value".
     */
    (void)block;
    return 0;
}
