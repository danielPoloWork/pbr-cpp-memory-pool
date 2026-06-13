// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file memory_pool.cpp
 * @brief Implementation of the public C surface (`memory_pool_create` and
 *        `memory_pool_destroy` per ADR-0009) and the C++ RAII forwarders
 *        for `it::d4np::memorypool::Pool` (per ADR-0010).
 *
 * All four spec §5 functions carry their real O(1) free-list bodies
 * (Milestones 2.3 / 2.4). The C++ wrapper implements the ADR-0016
 * exception policy: the C surface never throws, `Pool::allocate` and the
 * `Pool` ctor translate `NULL` into `std::bad_alloc`, and
 * `Pool::try_allocate` / `Pool::make` keep the non-throwing path.
 *
 * The `struct memory_pool` definition lives in this translation unit — it
 * is forward-declared in [`memory_pool.h`](memory_pool.h) and never visible
 * to any consumer, satisfying the C-style Pimpl boundary recorded in
 * ADR-0010. Its field list is fixed by ADR-0009 §6.
 *
 * The implementation is single-code-path across every Tier-1 toolchain
 * (ADR-0005 §1): C++17 over-aligned `::operator new(size, std::align_val_t)`
 * for the contiguous backing buffer + a plain `new` for the metadata
 * struct, both released by their matching `delete` overload.
 */

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>

namespace {

// Drop-in `malloc` alignment is the contract ADR-0009 §5 commits to.
constexpr std::size_t POOL_ALIGNMENT = alignof(std::max_align_t);

bool is_valid_block_size(std::size_t block_size) noexcept {
    // ADR-0009 §2 — three preconditions, evaluated in cheapest-first order
    // so an obvious zero short-circuits before the modulo.
    if (block_size == 0U) {
        return false;
    }
    if (block_size < sizeof(void*)) {
        return false;
    }
    if ((block_size % POOL_ALIGNMENT) != 0U) {
        return false;
    }
    return true;
}

bool would_overflow_product(std::size_t lhs, std::size_t rhs) noexcept {
    // ADR-0009 §3 mandates the overflow guard. The check is symmetric in
    // its arguments; we keep `lhs == 0 || rhs == 0` as an early return so
    // the division below is never against zero.
    if (lhs == 0U || rhs == 0U) {
        return false;
    }
    return lhs > (std::numeric_limits<std::size_t>::max() / rhs);
}

void release_backing(void* backing) noexcept {
    // The matching `::operator delete` overload for the over-aligned
    // allocation in `memory_pool_create`. C++17 [expr.delete]/p10 requires
    // the same alignment argument be passed at deallocation as at
    // allocation; passing the same `std::align_val_t` keeps the contract
    // closed.
    ::operator delete(backing, std::align_val_t{POOL_ALIGNMENT});
}

void initialize_free_list(void* backing, std::size_t block_size, std::size_t block_count) noexcept {
    // Implicit free list per ADR-0009 §1, ascending address order. Each
    // free slot stores the address of the next free slot in its own first
    // `sizeof(void*)` bytes; the last slot stores a null pointer.
    //
    // The `*static_cast<void**>(slot) = ptr` idiom is the canonical
    // pool-allocator write: it expresses "treat the slot's first bytes
    // as storage for a void* and assign the next-link value there" with
    // a single, named conversion. It is more concise than std::memcpy,
    // sidesteps clang-tidy's bugprone-multi-level-implicit-pointer-
    // conversion check (no const-void* destination conversion in the
    // expression), and compiles to the same single store. The slots are
    // raw storage from `::operator new`; writing a `void*` through a
    // `void**` lvalue is the well-defined implicit-object-creation path
    // every Tier-1 toolchain accepts.
    auto* const base = static_cast<unsigned char*>(backing);
    for (std::size_t i = 0; i + 1U < block_count; ++i) {
        void* const this_slot = base + (i * block_size);
        void* const next_slot = base + ((i + 1U) * block_size);
        *static_cast<void**>(this_slot) = next_slot;
    }
    void* const last_slot = base + ((block_count - 1U) * block_size);
    *static_cast<void**>(last_slot) = nullptr;
}

}  // namespace

// Pimpl per ADR-0010 — the struct's definition is exclusive to this
// translation unit, the public header sees only the forward declaration
// `typedef struct memory_pool memory_pool_t;`. The field list mirrors
// ADR-0009 §6 verbatim.
//
// The struct tag must match the C-language identifier exported by
// memory_pool.h's forward declaration (`struct memory_pool`). The
// .clang-tidy StructCase rule (CamelCase) is the right default for new
// C++ types; this is an explicit exception for the C-interop boundary,
// not a baseline deviation. The fields carry the project's trailing-
// underscore MemberSuffix convention — they are private to this TU.
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE
// ADR-0020 §3 — the ABA-tagged free-list head for the lock-free Treiber
// stack. The tag is bumped on every successful CAS so a recycled head
// pointer never compares equal to a stale snapshot. Two scalar members,
// no padding on Tier-1 64-bit (8 + 8) or 32-bit (4 + 4) hosts, so the
// std::atomic<TaggedHead> compare_exchange (a bitwise compare) has no
// padding-bit hazard.
struct TaggedHead {
    void* ptr_;
    std::uintptr_t tag_;
};
#endif

// ADR-0022 / ADR-0023 — the Composite chunk-list node for dynamic growth.
// Each overflow chunk is one contiguous backing of `block_count_` slots,
// forward-linked through `next_`. The pool's first chunk is the inline
// `backing_` / `block_count_` of `struct memory_pool` below; overflow
// chunks (acquired on exhaustion in dynamic mode — M5.3) are these nodes.
// The implicit free list is shared across all chunks, so only the
// foreign-pointer check and destroy walk the list (ADR-0022 §3).
struct Chunk {
    void* backing_;
    std::size_t block_count_;
    Chunk* next_;
};

// NOLINTNEXTLINE(readability-identifier-naming)
struct memory_pool {
    void* backing_;
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE
    std::atomic<TaggedHead> head_;
#else
    void* head_;
#endif
    std::size_t block_size_;
    std::size_t block_count_;
    std::size_t alignment_;
    // ADR-0023 — head of the Composite list of overflow chunks. nullptr
    // until dynamic growth links the first one (ADR-0024); the inline
    // backing_ above is then the pool's one and only chunk.
    Chunk* overflow_;
    // ADR-0024 — dynamic-growth factor: 0 means fixed mode (the default);
    // >= 2 means grow geometrically by this factor on exhaustion. A
    // runtime, per-pool value (ADR-0022 §1).
    std::size_t grow_factor_;
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_MUTEX
    // ADR-0020 §2 — the lock guarding the head pop/push in MutexPolicy.
    std::mutex mutex_;
#endif
};

// ADR-0015 §3 — per-pool metadata budget for the FIXED struct (the inline
// first chunk + bookkeeping). M5.3 added the dynamic-growth `grow_factor_`,
// taking the MUTEX struct from 128 to 136 bytes, so the budget is
// renegotiated per ADR-0015 §4 (and as ADR-0022/ADR-0023 anticipated) from
// 128 to 192: the struct is 56 bytes under NONE, 72 under LOCKFREE, 136 under
// MUTEX — all comfortably under, with headroom for future per-pool fields.
// Overflow Chunk descriptors are per-chunk metadata (O(chunks)) counted by
// memory_pool_metadata_bytes at run time, not in this compile-time gate;
// per-block overhead stays zero (ADR-0022 §3 / ADR-0024 §4).
constexpr std::size_t METADATA_BUDGET_BYTES = 192U;
static_assert(sizeof(memory_pool) <= METADATA_BUDGET_BYTES,
              "ADR-0015 §3: per-pool metadata budget exceeded — update the ADR before raising");

namespace {

// ADR-0012 — the foreign-pointer / out-of-range pointer detection that
// gates memory_pool_free. The struct memory_pool is in scope here (the
// definition above precedes this namespace), so the helpers can read
// the backing pointer, slot size, and slot count directly.
//
// Comparing `block_addr < base_addr` and `block_addr >= end_addr` via
// std::uintptr_t avoids C++17 [expr.rel]/4 unspecified behaviour on
// cross-allocation pointer `<` comparison — when `block` is genuinely
// foreign, the direct pointer-comparison form is UB, the uintptr_t
// form is portable. The two NOLINT annotations are narrow and pointed
// at exactly the use case the standard documents as the workaround.
//
// block_size comes from `pool` (not a parameter) so the four args carry no
// two adjacent same-typed parameters (bugprone-easily-swappable).
bool block_in_chunk(const memory_pool* pool, const void* backing, std::size_t count, const void* block) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto base_addr = reinterpret_cast<std::uintptr_t>(backing);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto block_addr = reinterpret_cast<std::uintptr_t>(block);
    const std::uintptr_t end_addr = base_addr + (pool->block_size_ * count);
    if (block_addr < base_addr) {
        return false;
    }
    if (block_addr >= end_addr) {
        return false;
    }
    if (((block_addr - base_addr) % pool->block_size_) != 0U) {
        return false;
    }
    return true;
}

// ADR-0022 / ADR-0023 — the block is valid if it falls inside any chunk:
// the inline first chunk, then each overflow chunk in the Composite list.
// O(chunks) = O(log N) in dynamic mode; O(1) in fixed mode (overflow_ is
// null, so only the first chunk is probed).
bool is_block_in_range(const memory_pool* pool, const void* block) noexcept {
    if (block_in_chunk(pool, pool->backing_, pool->block_count_, block)) {
        return true;
    }
    for (const Chunk* chunk = pool->overflow_; chunk != nullptr; chunk = chunk->next_) {
        if (block_in_chunk(pool, chunk->backing_, chunk->block_count_, block)) {
            return true;
        }
    }
    return false;
}

#if PBR_MEMORY_POOL_THREAD_SAFETY != PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE
// ADR-0024 §1 — the dynamic-growth slow path. Called from pop_head when the
// pool is exhausted AND dynamic (grow_factor_ >= 2), under whatever
// synchronization the policy already holds (none for NONE, the held mutex
// for MUTEX). Acquires a geometric overflow chunk (new total = current
// total × grow_factor_), initialises its slots into the now-empty shared
// free list, links it at the head of overflow_, and points head_ at it.
// noexcept: on any allocation failure it releases what it got and returns
// false, so the caller falls back to fixed-mode exhaustion (ADR-0024 §1).
// Not compiled under LOCKFREE — that policy never grows (ADR-0024 §2).
bool grow_pool(memory_pool* pool) noexcept {
    std::size_t total = pool->block_count_;
    for (const Chunk* chunk = pool->overflow_; chunk != nullptr; chunk = chunk->next_) {
        total += chunk->block_count_;
    }
    const std::size_t add = total * (pool->grow_factor_ - 1U);
    if (add == 0U) {
        return false;
    }
    if (would_overflow_product(add, pool->block_size_)) {
        return false;
    }
    const std::size_t bytes = add * pool->block_size_;

    void* backing = nullptr;
    try {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        backing = ::operator new(bytes, std::align_val_t{POOL_ALIGNMENT});
    } catch (const std::bad_alloc&) {
        return false;
    }
    Chunk* chunk = nullptr;
    try {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        chunk = new Chunk{backing, add, pool->overflow_};
    } catch (const std::bad_alloc&) {
        release_backing(backing);
        return false;
    }

    initialize_free_list(backing, pool->block_size_, add);
    pool->overflow_ = chunk;
    pool->head_ = backing;  // pool was empty; the new chunk is now the free list
    return true;
}
#endif

// ADR-0021 — the thread-safety Strategy (ADR-0020) plugs into the
// allocation / deallocation Template Method here. A policy is a struct of
// two static hooks owning the only place the head is read-modify-written:
//
//   void* Policy::pop_head(memory_pool*)              synchronized pop (nullptr if empty)
//   void  Policy::push_head(memory_pool*, void* blk)  synchronized push
//
// The exhaustion test lives inside pop_head (not the skeleton) so the
// lock-free policy can re-test the head inside its CAS retry loop
// (ADR-0021 §2). Exactly one policy is compiled — the one selected by
// PBR_MEMORY_POOL_THREAD_SAFETY (ADR-0020 §2) — and aliased to ActivePolicy,
// which the skeletons below are instantiated with from the C entry points.

#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_NONE

// No synchronization — the v0.3.0 fast path verbatim. Inlines to the exact
// previous instruction sequence; the single-thread build pays nothing.
struct SingleThreadedPolicy {
    static void* pop_head(memory_pool* pool) noexcept {
        if (pool->head_ == nullptr) {
            // ADR-0024 §1 — dynamic pools grow on exhaustion; fixed pools
            // (grow_factor_ < 2) and a failed growth report exhaustion.
            if (pool->grow_factor_ < 2U || !grow_pool(pool)) {
                return nullptr;
            }
        }
        void* const block = pool->head_;
        // The returned block still carries the next-link in its first
        // sizeof(void*) bytes; that is fine — the slot is now user-owned
        // and its contents are documented as indeterminate.
        pool->head_ = *static_cast<void**>(block);
        return block;
    }

    static void push_head(memory_pool* pool, void* block) noexcept {
        // Mirrors the *static_cast<void**>(slot) = ptr idiom used in
        // initialize_free_list: store the current head into the block's
        // first sizeof(void*) bytes, then make this block the new head.
        *static_cast<void**>(block) = pool->head_;
        pool->head_ = block;
    }
};
using ActivePolicy = SingleThreadedPolicy;

#elif PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_MUTEX

// Always-correct, always-portable: a std::mutex held across the O(1) head
// pop/push (ADR-0020 §2). The foreign-pointer guard runs in the skeleton,
// outside this lock (ADR-0021 §1).
struct MutexPolicy {
    static void* pop_head(memory_pool* pool) noexcept {
        const std::lock_guard<std::mutex> guard{pool->mutex_};
        if (pool->head_ == nullptr) {
            // ADR-0024 §1 — growth runs under the held mutex, so the chunk
            // append and free-list seeding are serialized for free.
            if (pool->grow_factor_ < 2U || !grow_pool(pool)) {
                return nullptr;
            }
        }
        void* const block = pool->head_;
        pool->head_ = *static_cast<void**>(block);
        return block;
    }

    static void push_head(memory_pool* pool, void* block) noexcept {
        const std::lock_guard<std::mutex> guard{pool->mutex_};
        *static_cast<void**>(block) = pool->head_;
        pool->head_ = block;
    }
};
using ActivePolicy = MutexPolicy;

#elif PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE

// Lock-free Treiber stack with an ABA-tagged head (ADR-0020 §3). The
// in-slot next-links stay plain pointers — only the head is atomic; a
// pushing thread writes block->next before the publishing CAS, and a
// popping thread's stale next read is discarded when its CAS fails (the
// backing is never returned to the OS during the pool's life, so the read
// is always of valid memory). The tag defeats ABA. compare_exchange_weak
// reloads `expected` on failure, which the loops rely on.
struct LockFreePolicy {
    // The 3-argument compare_exchange_weak uses acq_rel on success and the
    // derived acquire on failure (which re-reads `expected` for the retry) —
    // exactly the ordering the loops need, and short enough to stay on one
    // line.
    static void* pop_head(memory_pool* pool) noexcept {
        TaggedHead expected = pool->head_.load(std::memory_order_acquire);
        for (;;) {
            if (expected.ptr_ == nullptr) {
                return nullptr;
            }
            void* const next = *static_cast<void* const*>(expected.ptr_);
            const TaggedHead desired{next, expected.tag_ + 1U};
            if (pool->head_.compare_exchange_weak(expected, desired, std::memory_order_acq_rel)) {
                return expected.ptr_;
            }
        }
    }

    static void push_head(memory_pool* pool, void* block) noexcept {
        TaggedHead expected = pool->head_.load(std::memory_order_relaxed);
        TaggedHead desired{};
        do {
            *static_cast<void**>(block) = expected.ptr_;
            desired = TaggedHead{block, expected.tag_ + 1U};
        } while (!pool->head_.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));
    }
};
using ActivePolicy = LockFreePolicy;

#else
#error "PBR_MEMORY_POOL_THREAD_SAFETY has an unrecognised value (expected NONE / MUTEX / LOCKFREE)"
#endif

// Template Method skeleton for allocation (ADR-0021 §1). Invariant frame:
// validate the pool handle (a race-free read of an immutable field), then
// delegate the synchronized head pop to the policy. nullptr on a null or
// exhausted pool.
template <typename SyncPolicy>
void* alloc_skeleton(memory_pool* pool) noexcept {
    if (pool == nullptr) {
        return nullptr;
    }
    return SyncPolicy::pop_head(pool);
}

// Template Method skeleton for deallocation (ADR-0021 §1). Null pool, null
// block, and foreign / out-of-range pointers are silent no-ops (ADR-0012);
// those guards read only immutable post-creation fields, so they stay in
// the skeleton outside the policy's synchronized region. The synchronized
// head push is the policy's hook.
template <typename SyncPolicy>
void free_skeleton(memory_pool* pool, void* block) noexcept {
    if (pool == nullptr) {
        return;
    }
    if (block == nullptr) {
        return;
    }
    if (!is_block_in_range(pool, block)) {
        return;
    }
    SyncPolicy::push_head(pool, block);
}

}  // namespace

extern "C" {

memory_pool_t* memory_pool_create(std::size_t block_size, std::size_t block_count) {
    // Preconditions: every failure path returns nullptr (ADR-0009 §7).
    // Exceptions from `::operator new` are caught and translated to
    // nullptr so the C ABI boundary never propagates a C++ throw.
    if (!is_valid_block_size(block_size)) {
        return nullptr;
    }
    if (block_count == 0U) {
        return nullptr;
    }
    if (would_overflow_product(block_size, block_count)) {
        return nullptr;
    }

    const std::size_t total_bytes = block_size * block_count;

    // Step 1 — over-aligned contiguous backing for the slots (ADR-0009 §4).
    // The hand-managed owning pointer is unavoidable across the C ABI;
    // the matching `::operator delete` in `memory_pool_destroy` closes
    // the contract.
    void* backing = nullptr;
    try {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        backing = ::operator new(total_bytes, std::align_val_t{POOL_ALIGNMENT});
    } catch (const std::bad_alloc&) {
        return nullptr;
    }

    // Step 2 — metadata struct. Standard alignment is sufficient (all
    // fields are pointer- or size_t-aligned by default). On allocation
    // failure we must release the backing already obtained above before
    // returning nullptr — otherwise the contract from spec §3.1
    // ("no memory leaks") would be silently broken on the OOM path.
    // Same C-ABI ownership boundary as Step 1; the cppcoreguidelines-
    // owning-memory NOLINT covers the matching `delete pool` below.
    memory_pool_t* pool = nullptr;
    try {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        pool = new memory_pool{};
    } catch (const std::bad_alloc&) {
        release_backing(backing);
        return nullptr;
    }

    pool->backing_ = backing;
    pool->block_size_ = block_size;
    pool->block_count_ = block_count;
    pool->alignment_ = POOL_ALIGNMENT;
    // The pool starts as a single chunk (the inline backing_ above); the
    // Composite overflow list is empty, and grow_factor_ 0 means fixed mode
    // (memory_pool_create_dynamic overrides it — ADR-0024 §3).
    pool->overflow_ = nullptr;
    pool->grow_factor_ = 0U;

    // Step 3 — initialise the implicit free list per ADR-0009 §1. The
    // in-slot next-links are plain pointers in every thread-safety mode
    // (only the head is synchronized — ADR-0021 §1); the head is seeded
    // here per the active policy's representation.
    initialize_free_list(backing, block_size, block_count);
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE
    pool->head_.store(TaggedHead{backing, 0U}, std::memory_order_relaxed);
#else
    pool->head_ = backing;
#endif

    return pool;
}

// The three size_t parameters mirror the frozen memory_pool_create signature
// plus the growth factor; a config struct would diverge from the established
// create API, so the swappable-parameters check is suppressed at the source.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
memory_pool_t* memory_pool_create_dynamic(std::size_t block_size, std::size_t block_count, std::size_t growth_factor) {
    // ADR-0024 §2/§3 — dynamic-mode creation. Under the lock-free policy,
    // dynamic growth is not supported (safe concurrent chunk-list growth is
    // deferred); reject so the caller gets a clear NULL rather than a pool
    // that silently never grows.
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE
    static_cast<void>(block_size);
    static_cast<void>(block_count);
    static_cast<void>(growth_factor);
    return nullptr;
#else
    // A factor must actually grow (new total = current total × factor).
    if (growth_factor < 2U) {
        return nullptr;
    }
    memory_pool_t* const pool = memory_pool_create(block_size, block_count);
    if (pool != nullptr) {
        pool->grow_factor_ = growth_factor;
    }
    return pool;
#endif
}

void memory_pool_destroy(memory_pool_t* pool) {
    // ADR-0009 §7: nullptr is a documented no-op.
    if (pool == nullptr) {
        return;
    }
    // ADR-0022 / ADR-0023 — release the Composite overflow chunks first:
    // walk the list, freeing each chunk's backing and its descriptor. The
    // list is empty in fixed mode (the only mode until M5.3), so this loop
    // does nothing then and destroy stays the v0.4.0 single-backing path.
    Chunk* chunk = pool->overflow_;
    while (chunk != nullptr) {
        Chunk* const next = chunk->next_;
        release_backing(chunk->backing_);
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        delete chunk;
        chunk = next;
    }
    release_backing(pool->backing_);
    // Matches the `new memory_pool{}` in `memory_pool_create`. Same
    // C-ABI ownership boundary; `gsl::owner<>` cannot be applied to
    // memory_pool_t* without leaking the annotation into the public C
    // header, which the cross-language layout (ADR-0002) does not allow.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete pool;
}

std::size_t memory_pool_metadata_bytes(const memory_pool_t* pool) {
    // ADR-0015 §1 — per-pool metadata is the fixed struct footprint plus the
    // per-chunk Composite descriptors (ADR-0022 §3). The static_assert above
    // gates `sizeof(memory_pool)` (the fixed part) against
    // METADATA_BUDGET_BYTES at compile time. The overflow descriptors are
    // O(chunks) = O(log N) and zero in fixed mode (the only mode until M5.3),
    // so this returns exactly `sizeof(memory_pool)` then — unchanged from
    // v0.4.0. Per-block metadata stays 0. NULL is a defined no-op returning 0.
    if (pool == nullptr) {
        return 0U;
    }
    std::size_t total = sizeof(memory_pool);
    for (const Chunk* chunk = pool->overflow_; chunk != nullptr; chunk = chunk->next_) {
        total += sizeof(Chunk);
    }
    return total;
}

std::size_t memory_pool_block_size(const memory_pool_t* pool) {
    // ADR-0018 §3 — introspection companion to memory_pool_metadata_bytes.
    // The configured block_size is fixed for the pool's lifetime; the
    // STL allocator adapter reads it to decide whether an object fits a
    // single block. NULL is a defined no-op returning 0, matching the
    // rest of the API's NULL-tolerance posture.
    if (pool == nullptr) {
        return 0U;
    }
    return pool->block_size_;
}

void* memory_pool_alloc(memory_pool_t* pool) {
    // O(1) pop of the implicit free-list head via the ADR-0021 Template
    // Method skeleton and the active thread-safety policy (ADR-0020). NULL on
    // a null pool, or on exhaustion when the pool is fixed-mode or its growth
    // allocation fails; a dynamic pool grows on exhaustion inside pop_head
    // (ADR-0024 §1).
    return alloc_skeleton<ActivePolicy>(pool);
}

void memory_pool_free(memory_pool_t* pool, void* block) {
    // O(1) push onto the implicit free-list head via the ADR-0021 Template
    // Method skeleton and the active policy. Null pool, null block, and
    // foreign / out-of-range pointers are silent no-ops per ADR-0012.
    free_skeleton<ActivePolicy>(pool, block);
}

#if PBR_MEMORY_POOL_DIAGNOSTICS

const void* memory_pool_debug_free_list_head(const memory_pool_t* pool) {
    // ADR-0019 §2 — the free-list head. NULL pool or exhausted pool both
    // yield NULL. Read-only diagnostic surface, gated out of release builds.
    // Under the lock-free policy the head is an atomic tagged pointer; the
    // diagnostic walk reads a (best-effort, racy-under-contention) snapshot.
    if (pool == nullptr) {
        return nullptr;
    }
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE
    return pool->head_.load(std::memory_order_acquire).ptr_;
#else
    return pool->head_;
#endif
}

const void* memory_pool_debug_free_list_next(const memory_pool_t* pool, const void* current) {
    // ADR-0019 §2 — advance one link. The next-free pointer lives in the
    // first sizeof(void*) bytes of the current free slot (ADR-0009 §1);
    // reading it through `void* const*` is the const-correct mirror of the
    // `*static_cast<void**>(slot) = ptr` write idiom and provably casts
    // away no const ([expr.static.cast]: cv2 == cv1 here). Keeping the
    // layout knowledge in this TU honours the Pimpl boundary.
    if (pool == nullptr || current == nullptr) {
        return nullptr;
    }
    return *static_cast<void* const*>(current);
}

std::size_t memory_pool_debug_free_count(const memory_pool_t* pool) {
    // ADR-0019 §2 — O(free_count) walk. Cross-checked in the tests against
    // std::distance over the C++ FreeListView.
    if (pool == nullptr) {
        return 0U;
    }
    std::size_t count = 0U;
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE
    const void* slot = pool->head_.load(std::memory_order_acquire).ptr_;
#else
    const void* slot = pool->head_;
#endif
    while (slot != nullptr) {
        ++count;
        slot = *static_cast<void* const*>(slot);
    }
    return count;
}

#endif  // PBR_MEMORY_POOL_DIAGNOSTICS

}  // extern "C"

namespace it::d4np::memorypool {

Pool::Pool(std::size_t block_size, std::size_t block_count) : handle_(::memory_pool_create(block_size, block_count)) {
    // ADR-0016 §3 — the throwing construction path. Precondition
    // violations (ADR-0009 §2/§3) and backing-storage OOM both collapse
    // to NULL at the C boundary, so both surface as std::bad_alloc here.
    // Callers wanting failure as a value use Pool::make / PoolBuilder.
    if (handle_ == nullptr) {
        throw std::bad_alloc{};
    }
}

Pool::Pool(memory_pool_t* handle) noexcept : handle_(handle) {}

Pool::~Pool() noexcept {
    ::memory_pool_destroy(handle_);
}

Pool::Pool(Pool&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

Pool& Pool::operator=(Pool&& other) noexcept {
    if (this != &other) {
        ::memory_pool_destroy(handle_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

void* Pool::allocate() {
    // ADR-0016 §2 — throwing verb. Exhaustion and a null (moved-from)
    // handle are indistinguishable at the C boundary; both throw.
    void* const block = ::memory_pool_alloc(handle_);
    if (block == nullptr) {
        throw std::bad_alloc{};
    }
    return block;
}

void* Pool::try_allocate() noexcept {
    // ADR-0016 §2 — non-throwing verb; the exact v0.2.0 forwarder.
    return ::memory_pool_alloc(handle_);
}

void Pool::deallocate(void* block) noexcept {
    ::memory_pool_free(handle_, block);
}

memory_pool_t* Pool::native_handle() noexcept {
    return handle_;
}

std::size_t Pool::metadata_bytes() const noexcept {
    // ADR-0015 §2 — thin forwarder to the C accessor. The C function
    // already handles the null-handle case (returns 0) so the wrapper
    // adds no additional logic.
    return ::memory_pool_metadata_bytes(handle_);
}

std::size_t Pool::block_size() const noexcept {
    // ADR-0018 §3 — thin forwarder to the C accessor. Returns 0 on a
    // moved-from wrapper whose handle is null, like the C function.
    return ::memory_pool_block_size(handle_);
}

std::optional<Pool> Pool::make(std::size_t block_size, std::size_t block_count) {
    // Factory Method per ADR-0011 §1, restructured by ADR-0016 §3: the
    // public ctor now throws on failure, so the non-throwing factory
    // calls memory_pool_create directly and adopts the handle through
    // the private noexcept ctor — no try/catch on this path.
    memory_pool_t* const handle = ::memory_pool_create(block_size, block_count);
    if (handle == nullptr) {
        return std::nullopt;
    }
    return {Pool{handle}};
}

// Mirrors the C memory_pool_create_dynamic signature (three sizes); same
// rationale for suppressing the swappable-parameters check.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::optional<Pool> Pool::make_dynamic(std::size_t block_size, std::size_t block_count, std::size_t growth_factor) {
    // ADR-0024 §3 — the dynamic-mode Factory Method, mirroring make(). Returns
    // std::nullopt on any failure: an ADR-0009 §2/§3 violation, OOM, a
    // growth_factor < 2, or — under the lock-free policy — because dynamic
    // growth is unsupported there (ADR-0024 §2), which memory_pool_create_dynamic
    // signals as NULL.
    memory_pool_t* const handle = ::memory_pool_create_dynamic(block_size, block_count, growth_factor);
    if (handle == nullptr) {
        return std::nullopt;
    }
    return {Pool{handle}};
}

PoolBuilder& PoolBuilder::with_block_size(std::size_t block_size) noexcept {
    block_size_ = block_size;
    return *this;
}

PoolBuilder& PoolBuilder::with_block_count(std::size_t block_count) noexcept {
    block_count_ = block_count;
    return *this;
}

PoolBuilder& PoolBuilder::with_growth_factor(std::size_t growth_factor) noexcept {
    growth_factor_ = growth_factor;
    return *this;
}

std::optional<Pool> PoolBuilder::build() const {
    // Builder per ADR-0011 §2 — delegate to the Factory Method so the failure
    // semantics flow through a single point. A growth factor >= 2 routes to
    // the dynamic factory (ADR-0024 §3); otherwise the fixed one. build() is
    // const so the same configured builder can produce multiple pools.
    if (growth_factor_ >= 2U) {
        return Pool::make_dynamic(block_size_, block_count_, growth_factor_);
    }
    return Pool::make(block_size_, block_count_);
}

}  // namespace it::d4np::memorypool
