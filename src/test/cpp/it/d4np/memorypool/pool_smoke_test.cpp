// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_smoke_test.cpp
 * @brief Milestone 1 smoke tests for the public C and C++ surface.
 *
 * These tests prove that:
 *   - the public headers compile cleanly under our standards (C++17 here,
 *     C89/C99 verification lives in ROADMAP §1.10);
 *   - the static library links and exposes every spec §5 symbol;
 *   - the Milestone 1 stub implementations behave exactly as documented
 *     (NULL on alloc, no-op on free/destroy).
 *
 * They deliberately make no claim about real allocation behaviour — that
 * surface is exercised by the dedicated correctness tests landing with
 * Milestone 2.7. When the M2 implementations replace the stubs, the
 * assertions below will need to be updated; the doxygen on each TEST_CASE
 * records the M1 contract so the diff is obvious.
 */

// IMPORTANT: defining DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN before the doctest
// header instructs doctest to emit `main()` from this translation unit.
// clang-format's IncludeBlocks: Regroup may place the doctest include in a
// different priority group below — the define is still in effect there
// because preprocessor scope is forward from the define.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>
#include <it/d4np/memorypool/version.hpp>

#include <cstddef>
#include <string_view>
#include <utility>

#include <doctest/doctest.h>

using it::d4np::memorypool::Pool;
namespace mem = it::d4np::memorypool;

TEST_CASE("version constants are consistent with the project version") {
    // The string form must contain the integer components separated by dots.
    const std::string_view ver{mem::PBR_MEMORY_POOL_VERSION_STRING};
    CHECK(ver.find('.') != std::string_view::npos);

    // During Milestone 1 we are building toward v0.1.0; the components must
    // match the value declared in version.hpp. Bumping the version is a
    // release-PR step (docs/workflow/release.md §2), not a free-floating
    // change.
    CHECK(mem::PBR_MEMORY_POOL_VERSION_MAJOR == 0U);
    CHECK(mem::PBR_MEMORY_POOL_VERSION_MINOR == 1U);
    CHECK(mem::PBR_MEMORY_POOL_VERSION_PATCH == 0U);
}

TEST_CASE("C API symbols link and follow the Milestone 1 stub contract") {
    // memory_pool_create currently returns NULL by spec of the M1 stub —
    // M2.3 will replace this with a real backing-storage allocation.
    memory_pool_t* pool = memory_pool_create(64, 16);
    CHECK(pool == nullptr);

    // memory_pool_destroy on NULL is documented as a no-op; the stub
    // behaves identically.
    memory_pool_destroy(pool);

    // alloc on a null pool returns NULL; free on a null block is a no-op.
    void* block = memory_pool_alloc(pool);
    CHECK(block == nullptr);
    memory_pool_free(pool, block);
}

TEST_CASE("Pool RAII wrapper constructs, moves, and destroys cleanly") {
    // Construction wraps memory_pool_create; under the M1 stub the
    // resulting native handle is NULL but the wrapper itself must remain
    // valid (no crash on destruction).
    Pool a(64, 16);
    CHECK(a.native_handle() == nullptr);

    // allocate / deallocate forward to the stubs.
    void* slot = a.allocate();
    CHECK(slot == nullptr);
    a.deallocate(slot);

    // Move construction leaves the source in a valid empty state — the
    // M2 implementations will preserve this invariant, so we lock it in
    // now.
    Pool b(std::move(a));
    CHECK(a.native_handle() == nullptr);
    CHECK(b.native_handle() == nullptr);

    // Move assignment must release the current handle (no-op with stubs)
    // and adopt the source's.
    Pool c(128, 8);
    c = std::move(b);
    CHECK(b.native_handle() == nullptr);
    CHECK(c.native_handle() == nullptr);
}
