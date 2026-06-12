// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file typed_pool_test.cpp
 * @brief Tests for the `TypedPool<T>` type-safe surface (M3.2 / ADR-0017).
 *
 * The cases below prove that:
 *   - `block_size()` satisfies the ADR-0009 §2 preconditions by
 *     construction for both tiny and large `T` (compile-time and runtime);
 *   - the storage verbs follow the ADR-0016 dual-verb policy with typed
 *     pointers (throwing `allocate`, in-band `try_allocate`, LIFO reuse);
 *   - `construct` / `destroy` run the full `T` lifecycle, including the
 *     strong exception guarantee when `T`'s constructor throws;
 *   - the ctor / `make` construction split mirrors `Pool` (ADR-0016 §3);
 *   - a moved-from `TypedPool` is a valid empty shell.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/typed_pool.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

#include <doctest/doctest.h>

using it::d4np::memorypool::TypedPool;

namespace {

constexpr std::size_t SAFE_BLOCK_COUNT = 16U;

// Instrumented type for the construct/destroy lifecycle tests. The live
// counter is caller-owned (passed by reference) so the test needs no
// mutable global state; the type is deliberately immovable so a slot can
// only ever host the instance constructed in it.
class Counted {
public:
    explicit Counted(int& live) noexcept : live_(&live) {
        ++(*live_);
    }
    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;
    Counted(Counted&&) = delete;
    Counted& operator=(Counted&&) = delete;
    ~Counted() {
        --(*live_);
    }

private:
    int* live_;
};

// Type whose constructor always throws — drives the strong-exception-
// guarantee test for TypedPool::construct.
struct BoomOnConstruct {
    BoomOnConstruct() {
        throw std::runtime_error{"deliberate test throw"};
    }
};

}  // namespace

TEST_CASE("TypedPool<T>::block_size satisfies the ADR-0009 §2 preconditions by construction") {
    // Tiny T: the sizeof(void*) floor dominates, then rounds up to the
    // alignof(std::max_align_t) multiple (ADR-0017 §2).
    constexpr std::size_t char_slot = TypedPool<char>::block_size();
    static_assert(char_slot >= sizeof(void*), "free-list link must fit (ADR-0009 §2)");
    static_assert(char_slot % alignof(std::max_align_t) == 0U, "slot must be alignment-multiple (ADR-0009 §2)");

    // Large T: sizeof(T) dominates and still rounds up cleanly.
    struct Big {
        std::array<unsigned char, 100> bytes_;
    };
    constexpr std::size_t big_slot = TypedPool<Big>::block_size();
    static_assert(big_slot >= sizeof(Big), "slot must hold one T");
    static_assert(big_slot % alignof(std::max_align_t) == 0U, "slot must be alignment-multiple (ADR-0009 §2)");

    // Runtime mirror of the compile-time facts so the TEST_CASE reports
    // assertions under doctest as well.
    CHECK(char_slot >= sizeof(void*));
    CHECK(big_slot >= sizeof(Big));
}

TEST_CASE("TypedPool storage verbs follow the ADR-0016 dual-verb policy") {
    TypedPool<int> pool(2U);
    int* const first = pool.try_allocate();
    int* const second = pool.try_allocate();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    // Exhausted: the non-throwing verb reports in-band, the throwing
    // verb throws (ADR-0016 §2).
    CHECK(pool.try_allocate() == nullptr);
    CHECK_THROWS_AS(static_cast<void>(pool.allocate()), std::bad_alloc);

    // LIFO reuse, typed: the most recently deallocated slot comes back.
    pool.deallocate(first);
    int* const reissued = pool.allocate();
    CHECK(reissued == first);

    pool.deallocate(reissued);
    pool.deallocate(second);
}

TEST_CASE("TypedPool vends live, value-correct, aligned objects through construct") {
    TypedPool<std::uint64_t> pool(SAFE_BLOCK_COUNT);
    std::uint64_t* const value = pool.construct(std::uint64_t{0xCAFEBABEU});
    REQUIRE(value != nullptr);
    CHECK(*value == std::uint64_t{0xCAFEBABEU});

    // Same narrow ptr-to-int NOLINT pattern as pool_smoke_test.cpp — an
    // alignment check has no portable C++17 alternative.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto addr = reinterpret_cast<std::uintptr_t>(value);
    CHECK((addr % alignof(std::uint64_t)) == 0U);

    pool.destroy(value);
}

TEST_CASE("TypedPool construct/destroy run the full T lifecycle") {
    int live = 0;
    TypedPool<Counted> pool(4U);

    Counted* const first = pool.construct(live);
    REQUIRE(first != nullptr);
    CHECK(live == 1);

    Counted* const second = pool.construct(live);
    REQUIRE(second != nullptr);
    CHECK(live == 2);

    pool.destroy(first);
    CHECK(live == 1);
    pool.destroy(second);
    CHECK(live == 0);

    pool.destroy(nullptr);  // documented no-op
    CHECK(live == 0);
}

TEST_CASE("TypedPool::construct returns the slot when T's ctor throws (strong guarantee)") {
    // ADR-0017 §3 — pool capacity is invariant across a failed construct:
    // the single slot must be back on the free list after the throw.
    TypedPool<BoomOnConstruct> pool(1U);
    CHECK_THROWS_AS(static_cast<void>(pool.construct()), std::runtime_error);

    BoomOnConstruct* const slot = pool.try_allocate();
    CHECK(slot != nullptr);
    pool.deallocate(slot);
}

TEST_CASE("TypedPool ctor throws std::bad_alloc on block_count == 0 (ADR-0016 §3)") {
    // The ADR-0009 §2 block_size preconditions cannot fail (satisfied by
    // construction), so the degenerate count is the representative ctor
    // failure. The named local inside the lambda keeps the temporary out
    // of a bare expression statement (bugprone-unused-raii).
    const auto make_degenerate = [] {
        TypedPool<int> degenerate{0U};
        static_cast<void>(degenerate);
    };
    CHECK_THROWS_AS(make_degenerate(), std::bad_alloc);
}

TEST_CASE("TypedPool::make mirrors the non-throwing Pool::make split") {
    std::optional<TypedPool<int>> good = TypedPool<int>::make(SAFE_BLOCK_COUNT);
    REQUIRE(good.has_value());
    if (good.has_value()) {
        int* const slot = good->try_allocate();
        CHECK(slot != nullptr);
        good->deallocate(slot);
    }

    std::optional<TypedPool<int>> bad = TypedPool<int>::make(0U);
    CHECK_FALSE(bad.has_value());
}

TEST_CASE("moved-from TypedPool is a valid empty shell (ADR-0016 §2)") {
    TypedPool<int> source(4U);
    const TypedPool<int> target(std::move(source));
    REQUIRE(target.metadata_bytes() > 0U);  // the handle really moved

    // The use-after-move below is the behaviour under test.
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK(source.try_allocate() == nullptr);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK(source.metadata_bytes() == 0U);
}
