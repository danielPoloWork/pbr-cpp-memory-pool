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

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include <doctest/doctest.h>

using it::d4np::memorypool::Pool;
using it::d4np::memorypool::PoolBuilder;
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

TEST_CASE("memory_pool_alloc returns a block from a non-exhausted pool") {
    // M2.4 happy path — alloc pops the head of the free list and returns
    // a non-null block. Destroy at the end frees the backing.
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);
    void* const block = memory_pool_alloc(pool);
    CHECK(block != nullptr);
    memory_pool_free(pool, block);
    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_alloc returns NULL on a null pool") {
    // ADR-0009 §7 — null pool is a defined no-op returning NULL.
    CHECK(memory_pool_alloc(nullptr) == nullptr);
}

TEST_CASE("memory_pool_free is a no-op on null pool or null block") {
    // Both branches must not crash; the M2.8 Valgrind / ASan cells observe.
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);
    memory_pool_free(nullptr, nullptr);
    memory_pool_free(pool, nullptr);
    memory_pool_free(nullptr, pool);  // illegal pair, must still no-op via null-pool check
    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_alloc exhausts the pool after block_count successful pops") {
    // ADR-0009 §7 — fixed mode returns NULL on exhaustion. Pulling
    // block_count blocks must succeed; the (block_count + 1)-th call must
    // return nullptr. Free a block afterwards and verify the next alloc
    // succeeds again — proves the push/pop round-trip works without
    // depending on a specific block ordering (the implicit free list IS
    // ordered ascending after create, but after free/alloc cycles the
    // head can be anywhere within the pool).
    constexpr std::size_t COUNT = 4U;
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, COUNT);
    REQUIRE(pool != nullptr);

    std::array<void*, COUNT> slots{};
    for (std::size_t i = 0; i < COUNT; ++i) {
        slots.at(i) = memory_pool_alloc(pool);
        REQUIRE(slots.at(i) != nullptr);
    }
    // Exhausted.
    CHECK(memory_pool_alloc(pool) == nullptr);

    // Return one block; the next alloc must succeed.
    memory_pool_free(pool, slots[2]);
    void* const reissued = memory_pool_alloc(pool);
    CHECK(reissued != nullptr);
    // The free list is a stack (push to head, pop from head), so the
    // re-allocated pointer must equal the most recently freed block.
    CHECK(reissued == slots[2]);

    // Tidy up — return every outstanding block so destroy is leak-clean.
    for (std::size_t i = 0; i < COUNT; ++i) {
        if (i != 2U) {
            memory_pool_free(pool, slots.at(i));
        }
    }
    memory_pool_free(pool, reissued);
    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_alloc returns distinct, aligned pointers") {
    // Every alloc returns a distinct, aligned pointer. Uniqueness
    // confirms the free list never hands out the same slot twice;
    // alignment confirms the ADR-0009 §5 contract that every block is
    // alignof(std::max_align_t)-aligned (the structural argument is
    // already in the ADR — this is the runtime check that the
    // construction matches the contract).
    constexpr std::size_t COUNT = 8U;
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, COUNT);
    REQUIRE(pool != nullptr);

    std::array<void*, COUNT> slots{};
    for (std::size_t i = 0; i < COUNT; ++i) {
        slots.at(i) = memory_pool_alloc(pool);
        REQUIRE(slots.at(i) != nullptr);
        // The pointer-to-integer conversion is exactly the case
        // cppcoreguidelines-pro-type-reinterpret-cast targets, but for
        // an alignment check in test code there is no portable C++17
        // alternative — std::align is array-oriented, std::bit_cast is
        // C++20, and a C-style cast trips the same rule. NOLINT is
        // appropriately narrow here.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto addr = reinterpret_cast<std::uintptr_t>(slots.at(i));
        CHECK((addr % alignof(std::max_align_t)) == 0U);
    }
    // Pairwise distinctness — O(N^2) but COUNT is tiny.
    for (std::size_t i = 0; i < COUNT; ++i) {
        for (std::size_t j = i + 1U; j < COUNT; ++j) {
            CHECK(slots.at(i) != slots.at(j));
        }
    }
    for (std::size_t i = 0; i < COUNT; ++i) {
        memory_pool_free(pool, slots.at(i));
    }
    memory_pool_destroy(pool);
}

TEST_CASE("Pool RAII wrapper: allocate / deallocate exercise the real free list") {
    // The wrapper forwards to the M2.4 bodies, so allocate now returns a
    // non-null block from a valid pool. deallocate returns the block to
    // the free list; a subsequent allocate re-issues it (LIFO ordering).
    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool.native_handle() != nullptr);

    void* const first = pool.allocate();
    REQUIRE(first != nullptr);
    pool.deallocate(first);

    void* const second = pool.allocate();
    CHECK(second == first);  // LIFO — most-recently-freed comes back first.
    pool.deallocate(second);
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

// ===========================================================================
// M2.6 — Factory Method (Pool::make) and Builder (PoolBuilder) per ADR-0011.
// ===========================================================================

TEST_CASE("Pool::make returns an engaged optional on valid arguments") {
    // ADR-0011 §1 — successful construction yields std::optional<Pool>
    // engaged with the moved-in Pool. The wrapped Pool's native_handle()
    // is non-null and allocate() returns a real block.
    //
    // We use `.value().X` rather than `opt->X` because clang-tidy's
    // bugprone-unchecked-optional-access does not recognise doctest's
    // REQUIRE(opt.has_value()) as a control-flow check (REQUIRE is a
    // do-while macro that throws on failure but is not `[[noreturn]]`
    // from the static analyser's perspective). `.value()` throws
    // std::bad_optional_access on an empty optional, which the check
    // considers safe.
    std::optional<Pool> pool = Pool::make(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool.has_value());
    CHECK(pool.value().native_handle() != nullptr);
    void* const block = pool.value().allocate();
    CHECK(block != nullptr);
    pool.value().deallocate(block);
}

TEST_CASE("Pool::make returns nullopt on a misaligned block_size") {
    // ADR-0011 §1 — ADR-0009 §2 violation surfaces as std::nullopt at the
    // construction expression, rather than as a silent empty wrapper.
    constexpr std::size_t MISALIGNED = alignof(std::max_align_t) + 1U;
    std::optional<Pool> pool = Pool::make(MISALIGNED, SAFE_BLOCK_COUNT);
    CHECK_FALSE(pool.has_value());
}

TEST_CASE("Pool::make returns nullopt on block_count == 0") {
    // ADR-0009 §3 — degenerate pool — surfaces as nullopt.
    std::optional<Pool> pool = Pool::make(SAFE_BLOCK_SIZE, 0U);
    CHECK_FALSE(pool.has_value());
}

TEST_CASE("PoolBuilder builds a configured Pool fluently") {
    // ADR-0011 §2 — happy path. with_* setters return *this; build()
    // delegates to Pool::make and returns the optional.
    //
    // The single-line form of the fluent chain
    //   `std::optional<Pool> pool = PoolBuilder{}.with_*(...)*N.build();`
    // hits 121 columns — one over the .clang-format 120-col soft limit
    // — so the natural clang-format wrap is a break after the `=`
    // assignment operator. The continuation lands at column 9
    // (4 base indent + 4 ContinuationIndentWidth) and stays on a single
    // line, sidestepping the manual deep-indent column-alignment
    // anti-pattern recorded in the agent's memory.
    std::optional<Pool> pool =
        PoolBuilder{}.with_block_size(SAFE_BLOCK_SIZE).with_block_count(SAFE_BLOCK_COUNT).build();
    REQUIRE(pool.has_value());
    CHECK(pool.value().native_handle() != nullptr);
}

TEST_CASE("PoolBuilder::build returns nullopt on a default-constructed builder") {
    // ADR-0011 §2 — fail-loud for forgotten configuration: a default
    // builder has block_size_ = block_count_ = 0, both ADR-0009 §2/§3
    // violations, so build() flows through Pool::make to std::nullopt.
    std::optional<Pool> pool = PoolBuilder{}.build();
    CHECK_FALSE(pool.has_value());
}

TEST_CASE("PoolBuilder::build returns nullopt on a partially-configured builder") {
    // Only block_size is set; block_count stays 0 (ADR-0009 §3 violation).
    std::optional<Pool> pool = PoolBuilder{}.with_block_size(SAFE_BLOCK_SIZE).build();
    CHECK_FALSE(pool.has_value());
}

TEST_CASE("PoolBuilder::build is const — same builder produces multiple pools") {
    // ADR-0011 §2 — build() is const-qualified, so the same configured
    // builder can produce independently-owned pools. Useful for tests
    // and for benchmark setup where the same configuration is replayed
    // under varying conditions.
    const PoolBuilder builder = PoolBuilder{}.with_block_size(SAFE_BLOCK_SIZE).with_block_count(SAFE_BLOCK_COUNT);
    std::optional<Pool> first = builder.build();
    std::optional<Pool> second = builder.build();
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    // Independently-owned: the two pools have distinct native handles.
    CHECK(first.value().native_handle() != second.value().native_handle());
}
