// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file zero_overhead_test.cpp
 * @brief Zero-overhead-when-disabled verification (M6.3 / ADR-0025 §5).
 *
 * Milestone 6 adds observability "without touching the hot path of release
 * builds" (ROADMAP §6 goal). ADR-0025 §5 resolves "instrumentation disabled"
 * as **opt-in by type**: a program that uses `Pool` directly pays exactly
 * nothing — no counter, no branch, no atomic — because the instrumentation
 * lives in a *separate* type (`InstrumentedPool`) a caller chooses to wrap.
 * M6.3 is the verification that this holds. ADR-0025 §5 names two obligations:
 *
 *   1. the plain-`Pool` path is **byte-identical** to a build without the
 *      decorator, and
 *   2. the overhead lives **only inside `InstrumentedPool`**.
 *
 * Both are *structural* facts, not timing facts, so they are verified by
 * `static_assert` (compile-time, config-independent — they hold in Release
 * exactly as in Debug) plus a runtime behavioural-equivalence check, rather
 * than a wall-clock benchmark. A timing gate is deliberately avoided: shared
 * CI runners are too noisy for a meaningful per-op threshold (ADR-0014 §8),
 * and a measured delta could never *prove* zero overhead the way the type
 * structure does. The benchmark binary (`pool_vs_malloc_bench`) remains the
 * place for indicative numbers; this binary is the gate.
 *
 * Because the proof is structural, the same assertions are exercised in every
 * cell of the CI matrix, including the Release cells — that is the "in release
 * builds" coverage the roadmap item asks for, obtained by registering this as
 * an ordinary CTest target rather than by a bespoke Release-only job.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/instrumented_pool.hpp>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

using it::d4np::memorypool::InstrumentedPool;
using it::d4np::memorypool::Pool;
using it::d4np::memorypool::PoolObserver;

namespace {

constexpr std::size_t BLOCK_SIZE = 64U;
constexpr std::size_t BLOCK_COUNT = 16U;

// --------------------------------------------------------------------------
// Detection idiom: prove the instrumentation surface is opt-in *by type*.
// `stats()` and `add_observer()` exist on InstrumentedPool and on nothing
// reachable from a plain Pool — so a program holding a Pool cannot so much as
// name an instrumentation operation, let alone pay for one.
// --------------------------------------------------------------------------
template <typename, typename = void>
struct HasStats : std::false_type {};
template <typename T>
struct HasStats<T, std::void_t<decltype(std::declval<const T&>().stats())>> : std::true_type {};

template <typename, typename = void>
struct HasAddObserver : std::false_type {};
template <typename T>
struct HasAddObserver<T, std::void_t<decltype(std::declval<T&>().add_observer(std::declval<PoolObserver&>()))>>
    : std::true_type {};

// (1) Opt-in by type — the instrumentation surface is absent from Pool.
static_assert(!HasStats<Pool>::value, "a plain Pool must expose no stats() — instrumentation is opt-in by type");
static_assert(!HasAddObserver<Pool>::value,
              "a plain Pool must expose no add_observer() — observers are opt-in by type");
static_assert(HasStats<InstrumentedPool>::value, "InstrumentedPool is the type that carries the stats surface");
static_assert(HasAddObserver<InstrumentedPool>::value,
              "InstrumentedPool is the type that carries the observer surface");

// (2) Byte-identical footprint — wrapping costs the Pool nothing. Pool stays a
// single handle (ADR-0010 §2): the existence of InstrumentedPool adds no
// member, vtable, or padding to it.
static_assert(sizeof(Pool) == sizeof(memory_pool_t*),
              "Pool must remain exactly one handle — the decorator adds nothing to it");
static_assert(std::is_standard_layout_v<Pool>,
              "Pool must stay standard-layout — no instrumentation creeps into its layout");

// (3) Overhead is contained in the decorator. The five relaxed-atomic counters
// (ADR-0025 §2) plus the growth watermark are the cost; it lives wholly inside
// InstrumentedPool, never in Pool. We assert the decorator is at least that
// much larger than the bare handle, rather than pinning an exact size (the
// observer vector's footprint is implementation-defined).
constexpr std::size_t COUNTER_OVERHEAD = (5U * sizeof(std::atomic<std::size_t>)) + sizeof(std::size_t);
static_assert(
    sizeof(InstrumentedPool) >= sizeof(Pool) + COUNTER_OVERHEAD,
    "the instrumentation counters must account for the decorator's size growth — overhead lives in the decorator");

// Allocate @p count blocks from @p pool into @p out; each one must succeed.
// Templated so the bare `Pool` and the `InstrumentedPool` decorator drive the
// identical workload — the comparison in the behavioural-equivalence case.
template <typename PoolT>
void drain(PoolT& pool, std::vector<void*>& out, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        void* const block = pool.try_allocate();
        REQUIRE(block != nullptr);
        out.push_back(block);
    }
}

/** Return every block in @p blocks to @p pool. */
template <typename PoolT>
void release_all(PoolT& pool, const std::vector<void*>& blocks) {
    for (void* const block : blocks) {
        pool.deallocate(block);
    }
}

}  // namespace

TEST_CASE("the instrumentation surface is opt-in by type (ADR-0025 §5)") {
    // Mirror the namespace-scope static_asserts at runtime so the contract is
    // visible in the test report, not only as a build-time gate.
    CHECK_FALSE(HasStats<Pool>::value);
    CHECK_FALSE(HasAddObserver<Pool>::value);
    CHECK(HasStats<InstrumentedPool>::value);
    CHECK(HasAddObserver<InstrumentedPool>::value);
}

TEST_CASE("a plain Pool keeps its byte-identical single-handle footprint") {
    CHECK(sizeof(Pool) == sizeof(memory_pool_t*));
    CHECK(sizeof(Pool) == sizeof(void*));
    // The decorator is strictly larger — the instrumentation cost is real and
    // is paid only by callers who opt into the wrapper.
    CHECK(sizeof(InstrumentedPool) > sizeof(Pool));
}

TEST_CASE("wrapping a pool adds zero bytes to the library's per-pool metadata") {
    // metadata_bytes() reports the C-side `struct memory_pool` footprint
    // (ADR-0015). Instrumentation is header-only C++ state on top of the same
    // C struct, so a decorated pool's library-managed metadata is identical to
    // a bare pool's — the library object literally does not grow.
    Pool bare{BLOCK_SIZE, BLOCK_COUNT};
    InstrumentedPool decorated{Pool(BLOCK_SIZE, BLOCK_COUNT)};

    CHECK(decorated.metadata_bytes() == bare.metadata_bytes());
    CHECK(decorated.block_size() == bare.block_size());
}

TEST_CASE("the bare-Pool allocation path is behaviourally identical to the decorated path") {
    // Drain both pools to exhaustion under the same configuration and compare
    // the observable behaviour. Identical capacity, identical LIFO return
    // order, and identical exhaustion point prove the decorator is a
    // transparent pass-through: the Pool path is unchanged by its existence.
    Pool bare{BLOCK_SIZE, BLOCK_COUNT};
    InstrumentedPool decorated{Pool(BLOCK_SIZE, BLOCK_COUNT)};

    std::vector<void*> bare_blocks;
    std::vector<void*> decorated_blocks;
    bare_blocks.reserve(BLOCK_COUNT);
    decorated_blocks.reserve(BLOCK_COUNT);

    drain(bare, bare_blocks, BLOCK_COUNT);
    drain(decorated, decorated_blocks, BLOCK_COUNT);

    // Both exhausted at exactly block_count.
    CHECK(bare.try_allocate() == nullptr);
    CHECK(decorated.try_allocate() == nullptr);

    // Free everything, then re-drain: both hand blocks back in the same order
    // (the implicit free list, ADR-0009 §1, is untouched by the wrapper).
    release_all(bare, bare_blocks);
    release_all(decorated, decorated_blocks);

    std::vector<void*> bare_again;
    std::vector<void*> decorated_again;
    bare_again.reserve(BLOCK_COUNT);
    decorated_again.reserve(BLOCK_COUNT);
    drain(bare, bare_again, BLOCK_COUNT);
    drain(decorated, decorated_again, BLOCK_COUNT);

    // Absolute pointers differ (two independent backings), so the equivalence
    // is checked on the *behavioural signature*: freeing in order then
    // re-draining yields the reverse of the first sequence — the LIFO discipline
    // of the implicit free list. Both paths must exhibit it identically; the
    // decorator changed nothing about the underlying free-list mechanics.
    std::reverse(bare_blocks.begin(), bare_blocks.end());
    std::reverse(decorated_blocks.begin(), decorated_blocks.end());
    CHECK(bare_again == bare_blocks);            // bare path: reversed LIFO
    CHECK(decorated_again == decorated_blocks);  // decorated path: identical pattern

    release_all(bare, bare_again);
    release_all(decorated, decorated_again);
}
