// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_hardening_test.cpp
 * @brief Tests for the opt-in debug hardening mode (M9.2 / ADR-0043).
 *
 * The suite is gated behind `PBR_MEMORY_POOL_HARDENING` so the binary still
 * builds (with a single placeholder case) in the default, non-hardened build.
 * When hardening is on, the cases install a *recording* violation handler (in
 * place of the default diagnostic-and-abort one) so a detected violation can be
 * asserted without terminating the process, and prove that:
 *   - correct alloc/free cycles raise no violation (no false positives);
 *   - a write to a freed block's poisoned payload is caught on the next
 *     allocation (use-after-free);
 *   - a contiguous write past `block_size` is caught on free (buffer overflow);
 *   - freeing the same block twice is caught (double-free).
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/memory_pool.hpp>
#include <it/d4np/memorypool/pool_hardening.hpp>

#include <cstddef>
#include <cstring>
#include <set>
#include <vector>

// doctest is included last (after the project headers and the standard
// library), mirroring the sibling test TUs: doctest forward-declares a few
// std types (incl. `std::tuple`) in namespace std, so the real headers must be
// seen first or a newer toolchain rejects the redeclaration.
#include <doctest/doctest.h>

#if PBR_MEMORY_POOL_HARDENING

using it::d4np::memorypool::Pool;
namespace hv = it::d4np::memorypool;

namespace {

constexpr std::size_t BLOCK_SIZE = 64U;
constexpr std::size_t BLOCK_COUNT = 8U;

struct Violation {
    const char* kind_;
    std::size_t count_;
};

// Function-local static (not a namespace-scope global) records the handler's
// last observation across a single test's operations.
Violation& last_violation() noexcept {
    static Violation state{nullptr, 0U};
    return state;
}

void recording_handler(const char* kind, const void* /*block*/) noexcept {
    last_violation().kind_ = kind;
    ++last_violation().count_;
}

// Reset the record and install the recording handler. Called at the top of
// each case so a detected violation is captured instead of aborting.
void arm() noexcept {
    last_violation() = Violation{nullptr, 0U};
    hv::set_hardening_violation_handler(&recording_handler);
}

}  // namespace

TEST_CASE("correct alloc/free cycles raise no hardening violation") {
    arm();
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    std::vector<void*> blocks;
    for (std::size_t i = 0; i < BLOCK_COUNT; ++i) {
        blocks.push_back(pool.allocate());
    }
    for (void* const block : blocks) {
        pool.deallocate(block);
    }
    // Recycle once more — every slot goes free -> allocated -> free cleanly.
    for (std::size_t i = 0; i < BLOCK_COUNT; ++i) {
        void* const block = pool.allocate();
        pool.deallocate(block);
    }

    CHECK(last_violation().count_ == 0U);
}

TEST_CASE("use-after-free is detected on the next allocation") {
    arm();
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    void* const block = pool.allocate();
    pool.deallocate(block);

    // Scribble on the freed block's poisoned payload (past the in-band
    // next-link). This is the use-after-free the poison exists to catch.
    auto* const bytes = static_cast<unsigned char*>(block);
    bytes[sizeof(void*)] = 0x00U;

    // Re-allocating pops the same slot (LIFO); on_alloc verifies the poison and
    // finds it broken.
    void* const reused = pool.allocate();
    CHECK(last_violation().count_ >= 1U);
    REQUIRE(last_violation().kind_ != nullptr);
    CHECK(std::strcmp(last_violation().kind_, hv::HARDENING_USE_AFTER_FREE) == 0);

    pool.deallocate(reused);
}

TEST_CASE("a contiguous write past block_size is detected on free") {
    arm();
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    void* const block = pool.allocate();

    // Write one byte past the usable block_size — into the trailing guard word.
    auto* const bytes = static_cast<unsigned char*>(block);
    bytes[BLOCK_SIZE] = 0xFFU;

    pool.deallocate(block);  // on_free finds the guard neither allocated nor free
    CHECK(last_violation().count_ >= 1U);
    REQUIRE(last_violation().kind_ != nullptr);
    CHECK(std::strcmp(last_violation().kind_, hv::HARDENING_OVERFLOW) == 0);
}

TEST_CASE("a double free is detected") {
    arm();
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    void* const block = pool.allocate();
    pool.deallocate(block);  // legitimate free — guard stamped free
    pool.deallocate(block);  // second free of the same block — guard still free

    CHECK(last_violation().count_ >= 1U);
    REQUIRE(last_violation().kind_ != nullptr);
    CHECK(std::strcmp(last_violation().kind_, hv::HARDENING_DOUBLE_FREE) == 0);

    // No-further-corruption contract (ADR-0043): the rejected double-free must
    // NOT re-link the already-freed block, so the free list still holds exactly
    // BLOCK_COUNT distinct slots. Draining the pool must yield BLOCK_COUNT
    // unique pointers and then exhaust — a duplicate or a self-cycle would show
    // up as a repeated pointer or an over-count. The loop is bounded so a
    // regression that cycles the list cannot hang the test.
    std::vector<void*> drained;
    for (std::size_t i = 0; i < BLOCK_COUNT + 1U; ++i) {
        // try_allocate (noexcept, nullptr on exhaustion) — not allocate(),
        // which throws std::bad_alloc when the pool is drained (ADR-0016).
        void* const slot = pool.try_allocate();
        if (slot == nullptr) {
            break;
        }
        drained.push_back(slot);
    }
    CHECK(drained.size() == BLOCK_COUNT);
    const std::set<void*> distinct(drained.begin(), drained.end());
    CHECK(distinct.size() == drained.size());
}

TEST_CASE("the violation handler is swappable and restorable") {
    const hv::HardeningViolationHandler previous = hv::set_hardening_violation_handler(&recording_handler);
    CHECK(hv::hardening_violation_handler() == &recording_handler);
    // Passing nullptr restores the default handler.
    hv::set_hardening_violation_handler(nullptr);
    CHECK(hv::hardening_violation_handler() != &recording_handler);
    // Leave the previous handler as we found it.
    hv::set_hardening_violation_handler(previous);
}

#else  // PBR_MEMORY_POOL_HARDENING

TEST_CASE("debug hardening is compiled out in this build") {
    // The default (non-hardened) build compiles the mechanism out entirely
    // (ADR-0043); this placeholder keeps the binary and its CTest registration
    // valid, mirroring the free_list_iterator_test pattern.
    CHECK(true);
}

#endif  // PBR_MEMORY_POOL_HARDENING
