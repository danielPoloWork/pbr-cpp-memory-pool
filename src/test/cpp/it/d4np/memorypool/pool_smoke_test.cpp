// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_smoke_test.cpp
 * @brief Smoke tests for the public C and C++ surface across Milestones 1–2.
 *
 * The cases below prove that:
 *   - the public headers compile cleanly under our standards (C++17 here,
 *     C89/C99 verification lives in ROADMAP §1.10);
 *   - the static library links and exposes every spec §5 symbol;
 *   - `memory_pool_create` and `memory_pool_destroy` honour ADR-0009 §2
 *     and §3 — three `block_size` preconditions, `block_count > 0`, and
 *     the `size_t` overflow guard — by returning `NULL` on each violation;
 *   - the `Pool` RAII wrapper correctly owns the C handle's lifetime,
 *     including move-construction / move-assignment that leave the source
 *     in a valid empty state (ADR-0010).
 *
 * `memory_pool_alloc` and `memory_pool_free` are still Milestone 1 stubs
 * here — their O(1) bodies arrive in M2.4, at which point the relevant
 * TEST_CASE below earns real assertions instead of the current "still a
 * stub" expectations.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>
#include <it/d4np/memorypool/version.hpp>

#include <cstddef>
#include <limits>
#include <string_view>
#include <utility>

#include <doctest/doctest.h>

using it::d4np::memorypool::Pool;
namespace mem = it::d4np::memorypool;

namespace {

// A `block_size` that satisfies ADR-0009 §2 on every Tier-1 platform:
// 64 is a multiple of alignof(std::max_align_t) (16 on every Tier-1 host)
// and is comfortably larger than sizeof(void*).
constexpr std::size_t SAFE_BLOCK_SIZE = 64U;
constexpr std::size_t SAFE_BLOCK_COUNT = 16U;

}  // namespace

TEST_CASE("version constants are consistent with the project version") {
    const std::string_view ver{mem::PBR_MEMORY_POOL_VERSION_STRING};
    CHECK(ver.find('.') != std::string_view::npos);

    // The Milestone 1.14 release closed at v0.1.0 and Milestone 2 has not
    // bumped the constants yet. The M2.11 release PR will update these
    // assertions to 0.2.0 in lockstep with the version.hpp bump.
    CHECK(mem::PBR_MEMORY_POOL_VERSION_MAJOR == 0U);
    CHECK(mem::PBR_MEMORY_POOL_VERSION_MINOR == 1U);
    CHECK(mem::PBR_MEMORY_POOL_VERSION_PATCH == 0U);
}

TEST_CASE("memory_pool_create / _destroy round-trip on valid arguments") {
    // The combination SAFE_BLOCK_SIZE / SAFE_BLOCK_COUNT satisfies every
    // ADR-0009 §2 + §3 precondition. Construction must therefore return a
    // non-null handle, and destruction must release the backing without
    // leaking — Valgrind / ASan / UBSan all observe this case in the M2.8
    // CI cells; here we only check the immediate contract.
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);
    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_destroy(NULL) is a defined no-op") {
    // ADR-0009 §7: passing nullptr must not crash. The CI runs this case
    // under ASan/UBSan as well, so any latent UB shows up immediately.
    memory_pool_destroy(nullptr);
}

TEST_CASE("memory_pool_create returns NULL on block_size == 0") {
    // ADR-0009 §2 — first precondition.
    CHECK(memory_pool_create(0U, SAFE_BLOCK_COUNT) == nullptr);
}

TEST_CASE("memory_pool_create returns NULL on block_size below sizeof(void*)") {
    // ADR-0009 §2 — the free-list link must fit. sizeof(void*) is 8 on
    // every Tier-1 host (LP64 / LLP64); SAFE_BLOCK_SIZE / 16 = 4 is safely
    // below the floor without depending on the host's word size beyond
    // the C++17 minimum.
    CHECK(memory_pool_create(sizeof(void*) / 2U, SAFE_BLOCK_COUNT) == nullptr);
}

TEST_CASE("memory_pool_create returns NULL on block_size not aligned to alignof(max_align_t)") {
    // ADR-0009 §2 — `block_size` must be a multiple of
    // `alignof(std::max_align_t)`. Picking `alignof(std::max_align_t) + 1`
    // is misaligned by exactly one byte and is comfortably above the
    // sizeof(void*) floor on every Tier-1 host (alignof(max_align_t) is
    // ≥ 8 everywhere we support).
    constexpr std::size_t MISALIGNED = alignof(std::max_align_t) + 1U;
    CHECK(memory_pool_create(MISALIGNED, SAFE_BLOCK_COUNT) == nullptr);
}

TEST_CASE("memory_pool_create returns NULL on block_count == 0") {
    // ADR-0009 §3 — a zero-slot pool is degenerate.
    CHECK(memory_pool_create(SAFE_BLOCK_SIZE, 0U) == nullptr);
}

TEST_CASE("memory_pool_create returns NULL when block_size * block_count overflows size_t") {
    // ADR-0009 §3 — overflow guard. SIZE_MAX / 2 multiplied by 4 overflows
    // by ~2x, well past the wrap-around boundary, while still using a
    // legitimate block_size that satisfies §2 in isolation. The constant
    // is named OVERFLOW_TRIGGER rather than the obvious HUGE because
    // Apple Clang's <math.h> #defines HUGE as a float macro, which
    // turns a local constexpr declaration into a parse error on macOS.
    constexpr std::size_t OVERFLOW_TRIGGER =
        (std::numeric_limits<std::size_t>::max() / 2U) & ~(alignof(std::max_align_t) - 1U);
    CHECK(memory_pool_create(OVERFLOW_TRIGGER, 4U) == nullptr);
}

TEST_CASE("C API surface: alloc and free are still Milestone 1 stubs") {
    // M2.3 has implemented create / destroy. Alloc and free remain stubs
    // until M2.4 — this TEST_CASE locks in the stub contract so M2.4's
    // implementation PR has an obvious signal that the bodies have
    // arrived (the assertions below will need to be replaced).
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);

    void* block = memory_pool_alloc(pool);
    CHECK(block == nullptr);  // M2.4 will replace with REQUIRE(block != nullptr).
    memory_pool_free(pool, block);  // M2.4 will exercise the free-list round-trip.

    memory_pool_destroy(pool);
}

TEST_CASE("Pool RAII wrapper: construction acquires a real handle") {
    // Post-M2.3, memory_pool_create returns a non-null handle on valid
    // arguments, so the wrapper exposes the real pointer via
    // native_handle(). Destruction is exercised at scope exit and is
    // covered for leaks by the M2.8 Valgrind / ASan jobs.
    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    CHECK(pool.native_handle() != nullptr);

    // allocate / deallocate still forward to the M1 stubs of alloc / free.
    void* slot = pool.allocate();
    CHECK(slot == nullptr);  // M2.4 will replace.
    pool.deallocate(slot);
}

TEST_CASE("Pool RAII wrapper: invalid construction leaves the wrapper empty") {
    // ADR-0010 §2 — when memory_pool_create fails (here: misaligned
    // block_size from ADR-0009 §2), the wrapper's handle_ stays null and
    // the destructor is a safe no-op. allocate() returns nullptr because
    // the underlying handle is null.
    constexpr std::size_t MISALIGNED = alignof(std::max_align_t) + 1U;
    Pool empty(MISALIGNED, SAFE_BLOCK_COUNT);
    CHECK(empty.native_handle() == nullptr);
    CHECK(empty.allocate() == nullptr);
    empty.deallocate(nullptr);
}

TEST_CASE("Pool RAII wrapper: move construction transfers the handle") {
    Pool source(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    memory_pool_t* const original_handle = source.native_handle();
    REQUIRE(original_handle != nullptr);

    Pool target(std::move(source));

    // The handle moved from source to target; source is the valid empty
    // state ADR-0010 §2 commits to.
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK(source.native_handle() == nullptr);
    CHECK(target.native_handle() == original_handle);
}

TEST_CASE("Pool RAII wrapper: move assignment releases the previous handle") {
    Pool source(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    Pool target(SAFE_BLOCK_SIZE * 2U, SAFE_BLOCK_COUNT / 2U);

    memory_pool_t* const source_handle = source.native_handle();
    REQUIRE(source_handle != nullptr);
    REQUIRE(target.native_handle() != nullptr);

    target = std::move(source);

    // Target now owns source's old handle; target's previous handle has
    // already been destroyed inside operator= (no leak — covered by
    // M2.8's Valgrind cell).
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK(source.native_handle() == nullptr);
    CHECK(target.native_handle() == source_handle);
}
