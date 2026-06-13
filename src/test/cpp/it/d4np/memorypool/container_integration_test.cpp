// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file container_integration_test.cpp
 * @brief End-to-end integration of `PoolAllocator<T>` with standard and
 *        custom containers (M3.5 — exercises the M3.3 Adapter, ADR-0018).
 *
 * Three container families are driven through the adapter:
 *   - `std::list` — node-based, allocates one node at a time (`n == 1`), so
 *     every node takes the pool fast path. Where the diagnostics surface is
 *     enabled (ADR-0019), the tests assert each node really comes from the
 *     pool by watching `memory_pool_debug_free_count`.
 *   - `std::vector` — contiguous, allocates `n`-element buffers, so it runs
 *     on the heap fallback; the tests check correctness (contents, growth,
 *     copy/move, algorithms), not pool occupancy.
 *   - `ForwardList<T, Allocator>` — a minimal hand-written allocator-aware
 *     singly-linked list, proving the adapter works with an arbitrary
 *     container that obeys the `std::allocator_traits` contract, not just
 *     the standard ones.
 *
 * Pool-sizing recipe for node-based containers (ADR-0018 §3 consequences):
 * the pool's `block_size` must be at least the *rebound node* size, which is
 * implementation-defined and always larger than `sizeof(T)` (it adds the
 * container's per-node links). Pick a generous `block_size` — here 128 bytes
 * — and confirm at run time via `routes_to_pool` behaviour (an undersized
 * pool degrades safely to the heap fallback rather than corrupting memory).
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <it/d4np/memorypool/memory_pool.hpp>
#include <it/d4np/memorypool/pool_allocator.hpp>

#include <algorithm>
#include <cstddef>
#include <list>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

using it::d4np::memorypool::Pool;
using it::d4np::memorypool::PoolAllocator;

namespace {

// A block comfortably above any rebound node size on a 64-bit host, and a
// multiple of alignof(max_align_t) so Pool construction succeeds.
constexpr std::size_t BLOCK_SIZE = 128U;
constexpr std::size_t BLOCK_COUNT = 64U;

// Live-instance counter so the tests can prove a container destroys exactly
// what it constructs through the allocator (construct / destroy wired up).
class Tracked {
public:
    Tracked(int* live, int value) noexcept : live_(live), value_(value) {
        ++(*live_);
    }
    Tracked(const Tracked& other) noexcept : live_(other.live_), value_(other.value_) {
        ++(*live_);
    }
    Tracked(Tracked&& other) noexcept : live_(other.live_), value_(other.value_) {
        ++(*live_);
    }
    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&&) = default;
    ~Tracked() {
        --(*live_);
    }

    [[nodiscard]] int value() const noexcept {
        return value_;
    }

private:
    int* live_;
    int value_;
};

// Minimal allocator-aware singly-linked list — the "small custom container".
// It rebinds the user allocator to its node type and routes every node
// allocation / object lifetime through std::allocator_traits, exactly as a
// conforming container must. Deliberately move-only and minimal: the point
// is correct allocator usage, not a complete container.
template <typename T, typename Allocator = std::allocator<T>>
class ForwardList {
private:
    // Defined first so the nested const_iterator below sees a complete type.
    struct Node {
        T value_;
        Node* next_;
    };

    using NodeAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using NodeTraits = std::allocator_traits<NodeAlloc>;

public:
    using value_type = T;

    explicit ForwardList(const Allocator& alloc = Allocator{}) noexcept : alloc_(alloc) {}

    ForwardList(const ForwardList&) = delete;
    ForwardList& operator=(const ForwardList&) = delete;
    ForwardList(ForwardList&&) = delete;
    ForwardList& operator=(ForwardList&&) = delete;

    ~ForwardList() {
        clear();
    }

    void push_front(const T& value) {
        Node* const node = NodeTraits::allocate(alloc_, 1);
        try {
            NodeTraits::construct(alloc_, std::addressof(node->value_), value);
        } catch (...) {
            NodeTraits::deallocate(alloc_, node, 1);
            throw;
        }
        node->next_ = head_;
        head_ = node;
        ++size_;
    }

    void clear() noexcept {
        while (head_ != nullptr) {
            Node* const next = head_->next_;
            NodeTraits::destroy(alloc_, std::addressof(head_->value_));
            NodeTraits::deallocate(alloc_, head_, 1);
            head_ = next;
        }
        size_ = 0U;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_ == nullptr;
    }

    // Minimal const forward iterator — enough for range-for and <algorithm>.
    // CamelCase per the project's type-naming rule; the name is irrelevant to
    // range-for, which only needs begin()/end() and ++/*/!= on the result.
    class ConstIterator {
    public:
        ConstIterator() noexcept = default;
        // Non-explicit so begin()/end() can return a braced init list (Node
        // is private, so no accidental external conversion is possible).
        ConstIterator(const Node* node) noexcept : node_(node) {}

        const T& operator*() const noexcept {
            return node_->value_;
        }
        ConstIterator& operator++() noexcept {
            node_ = node_->next_;
            return *this;
        }
        [[nodiscard]] bool operator==(const ConstIterator& rhs) const noexcept {
            return node_ == rhs.node_;
        }
        [[nodiscard]] bool operator!=(const ConstIterator& rhs) const noexcept {
            return !(*this == rhs);
        }

    private:
        const Node* node_ = nullptr;
    };

    [[nodiscard]] ConstIterator begin() const noexcept {
        return {head_};
    }
    // A null node_ is the end sentinel; the body uses no member, which is
    // fine for a container's end() — silence the convert-to-static check.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] ConstIterator end() const noexcept {
        return {nullptr};
    }

private:
    NodeAlloc alloc_;
    Node* head_ = nullptr;
    std::size_t size_ = 0U;
};

}  // namespace

TEST_CASE("std::list<int> round-trips through PoolAllocator on the pool fast path") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    std::list<int, PoolAllocator<int>> values{PoolAllocator<int>(pool)};

#if PBR_MEMORY_POOL_DIAGNOSTICS
    // Baseline taken AFTER construction so any implementation-defined
    // sentinel node is already accounted for; each push must then consume
    // exactly one pool block (the node).
    const std::size_t baseline = ::memory_pool_debug_free_count(pool.native_handle());
#endif

    constexpr int COUNT = 10;
    for (int i = 0; i < COUNT; ++i) {
        values.push_back(i);
    }
    REQUIRE(values.size() == static_cast<std::size_t>(COUNT));

#if PBR_MEMORY_POOL_DIAGNOSTICS
    const std::size_t after = ::memory_pool_debug_free_count(pool.native_handle());
    CHECK(baseline - after == static_cast<std::size_t>(COUNT));
#endif

    // std::list algorithms work end-to-end.
    values.reverse();
    CHECK(values.front() == COUNT - 1);
    CHECK(values.back() == 0);
    CHECK(std::accumulate(values.begin(), values.end(), 0) == (COUNT - 1) * COUNT / 2);

    values.sort();
    CHECK(std::is_sorted(values.begin(), values.end()));

#if PBR_MEMORY_POOL_DIAGNOSTICS
    values.clear();
    // Every node returned to the pool: free count is back to the baseline.
    CHECK(::memory_pool_debug_free_count(pool.native_handle()) == baseline);
#endif
}

TEST_CASE("std::list<std::string> exercises non-trivial construct/destroy through the pool") {
    // std::string elements have real ctors/dtors; if construct/destroy were
    // mis-wired the leak would surface under ASan / Valgrind in CI.
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    std::list<std::string, PoolAllocator<std::string>> values{PoolAllocator<std::string>(pool)};

    values.emplace_back("alpha");
    values.emplace_back("beta");
    values.push_front(std::string(64, 'x'));  // long string → heap-backed payload
    REQUIRE(values.size() == 3U);
    CHECK(values.back() == "beta");

    values.pop_front();
    CHECK(values.front() == "alpha");
    values.clear();
    CHECK(values.empty());
}

TEST_CASE("std::vector<int> round-trips through PoolAllocator on the fallback path") {
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    std::vector<int, PoolAllocator<int>> values{PoolAllocator<int>(pool)};

    constexpr int COUNT = 200;  // far exceeds BLOCK_COUNT — must be heap-backed
    values.reserve(8);
    for (int i = 0; i < COUNT; ++i) {
        values.push_back(i);
    }
    REQUIRE(values.size() == static_cast<std::size_t>(COUNT));
    CHECK(values.front() == 0);
    CHECK(values.back() == COUNT - 1);
    CHECK(std::accumulate(values.begin(), values.end(), 0LL) == static_cast<long long>(COUNT - 1) * COUNT / 2);

    std::reverse(values.begin(), values.end());
    CHECK(values.front() == COUNT - 1);
}

TEST_CASE("PoolAllocator-backed container copy uses select_on_container_copy_construction") {
    // POCCA is false and SOCCC is the default (returns a copy), so the copy
    // keeps using the source's pool. Contents must be independent and equal.
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);
    std::vector<int, PoolAllocator<int>> original{PoolAllocator<int>(pool)};
    for (int i = 0; i < 5; ++i) {
        original.push_back(i * i);
    }

    std::vector<int, PoolAllocator<int>> copy = original;
    REQUIRE(copy.size() == original.size());
    CHECK(std::equal(copy.begin(), copy.end(), original.begin()));

    copy.push_back(999);
    CHECK(copy.size() == original.size() + 1U);  // independent storage
}

TEST_CASE("custom ForwardList works with std::allocator (generic, pool-agnostic)") {
    int live = 0;
    {
        ForwardList<Tracked> list;
        for (int i = 0; i < 5; ++i) {
            list.push_front(Tracked{&live, i});
        }
        REQUIRE(list.size() == 5U);
        CHECK(live == 5);

        int sum = 0;
        for (const Tracked& t : list) {
            sum += t.value();
        }
        CHECK(sum == 0 + 1 + 2 + 3 + 4);
    }
    // Destructor ran destroy on every node — no live instances leak.
    CHECK(live == 0);
}

TEST_CASE("custom ForwardList works with PoolAllocator on the pool fast path") {
    int live = 0;
    Pool pool(BLOCK_SIZE, BLOCK_COUNT);

    {
        ForwardList<Tracked, PoolAllocator<Tracked>> list{PoolAllocator<Tracked>(pool)};

#if PBR_MEMORY_POOL_DIAGNOSTICS
        const std::size_t baseline = ::memory_pool_debug_free_count(pool.native_handle());
#endif

        constexpr int COUNT = 8;
        for (int i = 0; i < COUNT; ++i) {
            list.push_front(Tracked{&live, i});
        }
        REQUIRE(list.size() == static_cast<std::size_t>(COUNT));
        CHECK(live == COUNT);

#if PBR_MEMORY_POOL_DIAGNOSTICS
        const std::size_t after = ::memory_pool_debug_free_count(pool.native_handle());
        CHECK(baseline - after == static_cast<std::size_t>(COUNT));  // each node from the pool
#endif

        list.clear();
        CHECK(live == 0);  // destroy ran on every element
    }
}
