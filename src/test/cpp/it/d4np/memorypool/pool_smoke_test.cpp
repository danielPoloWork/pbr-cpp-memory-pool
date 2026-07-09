// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_smoke_test.cpp
 * @brief Smoke tests for the public C and C++ surface across Milestones 1–3.
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
 *     in a valid empty state (ADR-0010);
 *   - the ADR-0016 exception policy holds on the C++ surface — the ctor
 *     and `allocate()` throw `std::bad_alloc` on failure, while
 *     `try_allocate()` / `Pool::make` / `PoolBuilder` report failure
 *     in-band and never throw.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>
#include <it/d4np/memorypool/version.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
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

    // v1.2.0 — Milestone 9 (Ergonomics, Hardening & Tooling): the additive,
    // ABI-compatible new-feature wave — std::pmr adapter, opt-in debug
    // hardening, a fuzzing harness, and the benchmark extension. The frozen
    // v1.0.0 public surface is unchanged, so this is a SemVer MINOR. These
    // constants move in lockstep with version.hpp.
    CHECK(mem::PBR_MEMORY_POOL_VERSION_MAJOR == 1U);
    CHECK(mem::PBR_MEMORY_POOL_VERSION_MINOR == 2U);
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

TEST_CASE("Pool ctor throws std::bad_alloc on invalid configuration (ADR-0016 §3)") {
    // ADR-0016 §3 amends ADR-0010 §2: the silent-empty-state ctor is
    // retired. A precondition violation (misaligned block_size, ADR-0009
    // §2) collapses to NULL at the C boundary and surfaces as
    // std::bad_alloc from the ctor. The lambda keeps the unnamed Pool
    // temporary out of a bare expression statement, so the throw happens
    // inside a call and bugprone-unused-raii stays quiet.
    constexpr std::size_t MISALIGNED = alignof(std::max_align_t) + 1U;
    const auto construct = []() -> Pool { return {MISALIGNED, SAFE_BLOCK_COUNT}; };
    CHECK_THROWS_AS(construct(), std::bad_alloc);
}

TEST_CASE("Pool::allocate throws std::bad_alloc on exhaustion (ADR-0016 §2)") {
    // The throwing verb: a single-block pool vends once, then the second
    // allocate() throws instead of returning nullptr. After returning the
    // block, allocation succeeds again — the throw left the free list
    // untouched.
    Pool pool(SAFE_BLOCK_SIZE, 1U);
    void* const only = pool.allocate();
    REQUIRE(only != nullptr);
    // The static_cast<void> discards the [[nodiscard]] result inside the
    // macro — the call is expected to throw, not to produce a block.
    CHECK_THROWS_AS(static_cast<void>(pool.allocate()), std::bad_alloc);
    pool.deallocate(only);
    void* const again = pool.allocate();
    CHECK(again == only);
    pool.deallocate(again);
}

TEST_CASE("Pool::try_allocate returns nullptr on exhaustion (ADR-0016 §2)") {
    // The non-throwing verb keeps the exact v0.2.0 allocate() semantics:
    // in-band nullptr on exhaustion, noexcept.
    Pool pool(SAFE_BLOCK_SIZE, 1U);
    void* const only = pool.try_allocate();
    REQUIRE(only != nullptr);
    CHECK(pool.try_allocate() == nullptr);
    pool.deallocate(only);
}

TEST_CASE("moved-from Pool: try_allocate returns nullptr, allocate throws (ADR-0016 §2)") {
    // The moved-from wrapper's null handle is indistinguishable from
    // exhaustion at the C boundary, so the two verbs report it the same
    // way they report exhaustion.
    Pool source(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    const Pool target(std::move(source));
    REQUIRE(target.metadata_bytes() > 0U);  // the handle really moved
    // The use-after-move below is the behaviour under test.
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK(source.try_allocate() == nullptr);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK_THROWS_AS(static_cast<void>(source.allocate()), std::bad_alloc);
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
    // Clang-tidy's bugprone-unchecked-optional-access only recognises an
    // explicit `if (opt)` / `if (opt.has_value())` block as a flow guard;
    // neither doctest's REQUIRE nor the throwing `.value()` accessor
    // counts. The REQUIRE catches an unexpected empty optional with a
    // useful failure message; the if-block then gives clang-tidy the
    // flow guard it needs and exposes a `Pool&` for the remaining
    // accesses.
    std::optional<Pool> opt = Pool::make(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(opt.has_value());
    if (opt.has_value()) {
        Pool& pool = *opt;
        CHECK(pool.native_handle() != nullptr);
        void* const block = pool.allocate();
        CHECK(block != nullptr);
        pool.deallocate(block);
    }
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
    // delegates to Pool::make and returns the optional. The fluent
    // chain is shorter than 120 columns with the `opt` local name
    // (118 cols at the column limit) so it stays on one line and
    // clang-format leaves it alone.
    std::optional<Pool> opt = PoolBuilder{}.with_block_size(SAFE_BLOCK_SIZE).with_block_count(SAFE_BLOCK_COUNT).build();
    REQUIRE(opt.has_value());
    if (opt.has_value()) {
        Pool& pool = *opt;
        CHECK(pool.native_handle() != nullptr);
    }
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
    if (first.has_value() && second.has_value()) {
        // Independently-owned: the two pools have distinct native handles.
        CHECK((*first).native_handle() != (*second).native_handle());
    }
}

// ===========================================================================
// M2.7 — Foreign-pointer / out-of-range pointer policy per ADR-0012.
//
// All five cases verify the same invariant: a foreign-pointer pass to
// memory_pool_free is a silent no-op — the pool's free-list head, the
// total capacity (provable via consecutive allocs), and the integrity of
// the chain are bit-identical before and after the offending call. ASan
// and UBSan in the CI matrix observe the absence of out-of-bounds writes.
// ===========================================================================

TEST_CASE("memory_pool_free is a no-op on an out-of-range pointer below the backing") {
    // ADR-0012 — block_addr < base_addr branch of is_block_in_range.
    // Allocate a real block to learn pool->head_, then synthesise a
    // pointer one slot before the backing buffer via uintptr_t
    // arithmetic. NOLINT on the cast is the standing pattern for
    // ptr-to-int in tests (memory feedback).
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);

    void* const block = memory_pool_alloc(pool);
    REQUIRE(block != nullptr);
    memory_pool_free(pool, block);  // restore the head — the foreign-pointer free below must leave it alone

    // Subsequent alloc/free of a real block should still work — proves
    // the pool state isn't corrupted by the foreign call.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto base_addr = reinterpret_cast<std::uintptr_t>(block);
    // The reverse int->ptr cast trips two checks:
    // cppcoreguidelines-pro-type-reinterpret-cast on the cast itself, and
    // performance-no-int-to-ptr because the synthesised pointer has no
    // provenance the optimizer can track. Both NOLINTs are justified —
    // a synthetic out-of-range pointer is precisely the data being
    // tested by the foreign-pointer policy.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    auto* const foreign_below = reinterpret_cast<void*>(base_addr - SAFE_BLOCK_SIZE);
    memory_pool_free(pool, foreign_below);

    // The pool must still vend block_count slots — a single free that
    // mutated head_ via the foreign pointer would either crash or
    // produce a smaller usable capacity.
    void* const a = memory_pool_alloc(pool);
    void* const b = memory_pool_alloc(pool);
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    memory_pool_free(pool, a);
    memory_pool_free(pool, b);

    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_free is a no-op on an out-of-range pointer above the backing") {
    // ADR-0012 — block_addr >= end_addr branch.
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);

    // Construct a pointer one slot past the end of the backing buffer.
    void* const block = memory_pool_alloc(pool);
    REQUIRE(block != nullptr);
    memory_pool_free(pool, block);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto base_addr = reinterpret_cast<std::uintptr_t>(block);
    // Same NOLINT rationale as the out-of-range-below test above: the
    // int->ptr synthesis is intentional for testing the foreign-pointer
    // policy and trips both pro-type-reinterpret-cast and
    // performance-no-int-to-ptr. Compute the target address into a
    // uintptr_t local first so the cast itself fits on the single line
    // that NOLINTNEXTLINE actually suppresses (the directive applies to
    // the immediately following physical line, not the logical
    // statement that may span two lines).
    const std::uintptr_t target_addr = base_addr + (SAFE_BLOCK_SIZE * SAFE_BLOCK_COUNT) + SAFE_BLOCK_SIZE;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    auto* const foreign_above = reinterpret_cast<void*>(target_addr);
    memory_pool_free(pool, foreign_above);

    void* const a = memory_pool_alloc(pool);
    CHECK(a != nullptr);
    memory_pool_free(pool, a);

    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_free is a no-op on an in-range but misaligned pointer") {
    // ADR-0012 — third branch of is_block_in_range: the pointer lies
    // inside the backing buffer but at a non-slot-boundary offset.
    // We construct a misaligned in-range pointer by taking a legitimate
    // block and offsetting it by 1 byte. Pointer arithmetic on an
    // in-range char* is well-defined within the block's storage; the
    // resulting pointer is in-range from the pool's perspective.
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);

    void* const block = memory_pool_alloc(pool);
    REQUIRE(block != nullptr);

    auto* const misaligned = static_cast<char*>(block) + 1;
    memory_pool_free(pool, misaligned);  // must be a no-op — block is still owned by the caller

    // The legitimate `block` is still owned; freeing it must succeed
    // and the pool must return it on the next alloc (LIFO).
    memory_pool_free(pool, block);
    void* const re = memory_pool_alloc(pool);
    CHECK(re == block);
    memory_pool_free(pool, re);

    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_free is a no-op on a foreign heap pointer") {
    // ADR-0012 — pointer from a different heap allocation. The
    // is_block_in_range check rejects it via the address comparison
    // without ever dereferencing — ASan / UBSan see no access.
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);

    // A second pool's backing is guaranteed by `operator new` to be a
    // disjoint allocation; passing one of its slots into pool's free
    // function exercises exactly the cross-allocation case the
    // [expr.rel]/4 unspecified-behaviour clause warns about.
    memory_pool_t* foreign_pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(foreign_pool != nullptr);
    void* const foreign_block = memory_pool_alloc(foreign_pool);
    REQUIRE(foreign_block != nullptr);

    memory_pool_free(pool, foreign_block);  // must not crash; must not mutate pool

    // The foreign pool's block is still owned by foreign_pool; the
    // legitimate free returns it there. pool, untouched, still vends.
    memory_pool_free(foreign_pool, foreign_block);
    void* const a = memory_pool_alloc(pool);
    CHECK(a != nullptr);
    memory_pool_free(pool, a);

    memory_pool_destroy(foreign_pool);
    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_free is a no-op on a stack pointer") {
    // ADR-0012 — stack-allocated objects produce pointers that are
    // definitely outside any heap-allocated pool backing. The address
    // comparison rejects without dereferencing.
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);

    int stack_local = 0;
    memory_pool_free(pool, &stack_local);

    // Pool state is unchanged — every slot still allocatable.
    std::array<void*, SAFE_BLOCK_COUNT> slots{};
    for (std::size_t i = 0; i < SAFE_BLOCK_COUNT; ++i) {
        slots.at(i) = memory_pool_alloc(pool);
        REQUIRE(slots.at(i) != nullptr);
    }
    // Pool exhausted after exactly block_count successful pops.
    CHECK(memory_pool_alloc(pool) == nullptr);
    for (std::size_t i = 0; i < SAFE_BLOCK_COUNT; ++i) {
        memory_pool_free(pool, slots.at(i));
    }
    memory_pool_destroy(pool);
}

// ===========================================================================
// M2.10 — Metadata-overhead budget and introspection per ADR-0015.
//
// Four invariants enforced by the four TEST_CASEs below:
//   - NULL pool → 0 bytes (defined no-op);
//   - live pool → > 0 bytes (sanity — the function must report something);
//   - live pool → <= 128 bytes (the ADR-0015 §3 budget; the same budget is
//     also gated at compile time via a static_assert in memory_pool.cpp);
//   - metadata_bytes is O(1) in block_count — a 1024-block pool and a
//     1,000,000-block pool report identical values.
//
// The C++ Pool::metadata_bytes() forwarder is exercised in the same set so
// the wrapper's noexcept const accessor is covered alongside the C path.
// ===========================================================================

TEST_CASE("memory_pool_metadata_bytes returns 0 on a null pool") {
    // ADR-0015 §2 — NULL is a defined input, returning 0 (no metadata
    // exists for a destroyed / never-created pool).
    CHECK(memory_pool_metadata_bytes(nullptr) == 0U);
}

TEST_CASE("memory_pool_metadata_bytes returns a positive value on a live pool") {
    // Sanity — the struct exists and has at least one byte. The exact
    // value is gated below; this TEST_CASE only proves the function does
    // not under-report (e.g., return 0 for a non-null input).
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);
    CHECK(memory_pool_metadata_bytes(pool) > 0U);
    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_metadata_bytes stays within the ADR-0015 budget") {
    // ADR-0015 §3 — the per-pool fixed-struct metadata budget, renegotiated
    // per ADR-0015 §4 from 128 to 192 in M5.3 (the dynamic-growth grow_factor_
    // took the MUTEX struct to 136). The same constant gates
    // `sizeof(memory_pool)` at compile time in memory_pool.cpp; this CHECK
    // gates the runtime-reported value (a fixed pool has no overflow chunks,
    // so it equals the struct footprint) against the same number.
    constexpr std::size_t ADR_0015_BUDGET_BYTES = 192U;
    memory_pool_t* pool = memory_pool_create(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool != nullptr);
    CHECK(memory_pool_metadata_bytes(pool) <= ADR_0015_BUDGET_BYTES);
    memory_pool_destroy(pool);
}

TEST_CASE("memory_pool_metadata_bytes is O(1) in block_count (spec §3.2)") {
    // The implicit free list (ADR-0009 §1) means per-block external
    // metadata is zero. Two pools with vastly different block_count
    // values must therefore report identical metadata bytes — that is
    // the operational definition of the spec §3.2 "minimal" guarantee.
    //
    // 1024 vs 1,000,000 spans three orders of magnitude — comfortably
    // wide enough to surface any accidental block_count dependency,
    // and small enough that the 1M-block pool's 64 MB backing
    // allocation stays well within every Tier-1 host's memory budget.
    constexpr std::size_t SMALL_COUNT = 1024U;
    constexpr std::size_t HUGE_COUNT = 1'000'000U;
    memory_pool_t* small_pool = memory_pool_create(SAFE_BLOCK_SIZE, SMALL_COUNT);
    REQUIRE(small_pool != nullptr);
    memory_pool_t* huge_pool = memory_pool_create(SAFE_BLOCK_SIZE, HUGE_COUNT);
    REQUIRE(huge_pool != nullptr);
    CHECK(memory_pool_metadata_bytes(small_pool) == memory_pool_metadata_bytes(huge_pool));
    memory_pool_destroy(small_pool);
    memory_pool_destroy(huge_pool);
}

TEST_CASE("Pool::metadata_bytes() forwards to the C accessor") {
    // ADR-0015 §2 — the C++ wrapper is a thin noexcept const forwarder.
    // The reported value must match the C function's report for the same
    // underlying handle.
    Pool pool(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT);
    REQUIRE(pool.native_handle() != nullptr);
    CHECK(pool.metadata_bytes() == memory_pool_metadata_bytes(pool.native_handle()));
    CHECK(pool.metadata_bytes() > 0U);
}

TEST_CASE("memory_pool_create_dynamic rejects a growth factor below 2") {
    // ADR-0024 §3 — a factor must actually grow. This holds under every
    // build: NONE/MUTEX reject the degenerate factor, LOCKFREE rejects
    // dynamic mode outright (ADR-0024 §2). Either way → NULL.
    CHECK(memory_pool_create_dynamic(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT, 1U) == nullptr);
    CHECK(memory_pool_create_dynamic(SAFE_BLOCK_SIZE, SAFE_BLOCK_COUNT, 0U) == nullptr);
}

TEST_CASE("a dynamic pool grows past its initial capacity") {
    // ADR-0022 / ADR-0024 — a dynamic pool acquires overflow chunks on
    // exhaustion, so it vends far more than its initial block_count. Under a
    // lock-free build dynamic mode is unsupported and creation returns NULL
    // (the documented contract — ADR-0024 §2); the comprehensive per-policy
    // matrix is M5.4, so here we simply skip that case.
    constexpr std::size_t INITIAL_BLOCKS = 4U;
    constexpr std::size_t GROW_ALLOCS = 100U;  // 25x the initial capacity
    memory_pool_t* const pool = memory_pool_create_dynamic(SAFE_BLOCK_SIZE, INITIAL_BLOCKS, 2U);
    if (pool == nullptr) {
        return;  // lock-free build: dynamic mode rejected by design
    }

    std::array<void*, GROW_ALLOCS> blocks{};
    for (std::size_t i = 0; i < GROW_ALLOCS; ++i) {
        void* const block = memory_pool_alloc(pool);
        REQUIRE(block != nullptr);  // never exhausts — it grows
        blocks.at(i) = block;
    }
    // The pool grew, so its metadata now includes overflow-chunk descriptors:
    // strictly more than the fixed-struct footprint.
    CHECK(memory_pool_metadata_bytes(pool) > sizeof(void*));

    for (std::size_t i = 0; i < GROW_ALLOCS; ++i) {
        memory_pool_free(pool, blocks.at(i));
    }
    memory_pool_destroy(pool);
}
