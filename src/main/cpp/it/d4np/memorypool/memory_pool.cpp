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
#include <it/d4np/memorypool/pool_hardening.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>

#if PBR_MEMORY_POOL_HARDENING
#include <cstdlib>
#include <cstring>
#include <iostream>
#endif

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

// ---------------------------------------------------------------------------
// Opt-in debug hardening (ADR-0043). Everything here is compiled only when
// PBR_MEMORY_POOL_HARDENING is set; a release build is byte-for-byte unchanged.
// The next-link accessors read_next / write_next are used at EVERY free-list
// site below so the safe-linking transform is applied uniformly; when hardening
// is off they inline to the exact `*static_cast<void**>(slot)` load/store the
// pool has always used, so the codegen is identical.
// ---------------------------------------------------------------------------

// Physical slot stride. Hardening reserves a trailing guard word after the
// user-visible block_size and rounds the stride up to POOL_ALIGNMENT so every
// slot start stays aligned (ADR-0009 §5). Off, this is the identity — the
// stride IS block_size, exactly as before.
constexpr std::size_t slot_stride(std::size_t block_size) noexcept {
#if PBR_MEMORY_POOL_HARDENING
    // Guard both intermediate additions against size_t overflow: a block_size
    // within a guard-word (or a round-up) of SIZE_MAX would otherwise wrap and
    // collapse the stride to 0 / a tiny value, sailing past the caller's
    // would_overflow_product guard and causing an out-of-bounds guard/poison
    // write. Saturating to SIZE_MAX instead makes that guard reject the input
    // (return NULL / false) exactly as the non-hardened identity path does.
    constexpr std::size_t SIZE_LIMIT = std::numeric_limits<std::size_t>::max();
    if (block_size > SIZE_LIMIT - sizeof(std::uint64_t)) {
        return SIZE_LIMIT;
    }
    const std::size_t raw = block_size + sizeof(std::uint64_t);
    if (raw > SIZE_LIMIT - (POOL_ALIGNMENT - 1U)) {
        return SIZE_LIMIT;
    }
    return ((raw + POOL_ALIGNMENT - 1U) / POOL_ALIGNMENT) * POOL_ALIGNMENT;
#else
    return block_size;
#endif
}

#if PBR_MEMORY_POOL_HARDENING

namespace hardening_detail {

constexpr std::size_t SAFE_LINK_SHIFT = 12U;  // glibc PROTECT_PTR page shift
constexpr unsigned char POISON_BYTE = 0xDEU;  // freed-block payload fill
constexpr std::uint64_t GUARD_FREED = 0xF3EEF3EEF3EEF3EEULL;
constexpr std::uint64_t GUARD_ALLOCATED = 0xA110CA7EA110CA7EULL;

// glibc safe-linking transform (symmetric: protect == reveal). Keying the
// stored value on the slot's own address means an out-of-band leaked pointer is
// not directly usable, and a corrupted store reveals to a misaligned address.
// The (slot, value) pair models distinct roles (the keying address vs. the
// pointer being (un)masked); the transform is symmetric so a swap is harmless,
// and a config struct for two pointers would only obscure the call sites.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void* xor_link(const void* slot, void* value) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto key = reinterpret_cast<std::uintptr_t>(slot) >> SAFE_LINK_SHIFT;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto raw = reinterpret_cast<std::uintptr_t>(value);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    return reinterpret_cast<void*>(key ^ raw);
}

void report(const char* kind, const void* block) noexcept {
    it::d4np::memorypool::hardening_violation_handler()(kind, block);
}

std::uint64_t read_guard(const void* slot, std::size_t block_size) noexcept {
    std::uint64_t value = 0U;
    std::memcpy(&value, static_cast<const unsigned char*>(slot) + block_size, sizeof(value));
    return value;
}

void write_guard(void* slot, std::size_t block_size, std::uint64_t value) noexcept {
    std::memcpy(static_cast<unsigned char*>(slot) + block_size, &value, sizeof(value));
}

void poison_payload(void* slot, std::size_t block_size) noexcept {
    // The first sizeof(void*) bytes hold the (safe-linked) next-link; poison
    // only the payload beyond it.
    if (block_size > sizeof(void*)) {
        std::memset(static_cast<unsigned char*>(slot) + sizeof(void*), POISON_BYTE, block_size - sizeof(void*));
    }
}

bool payload_is_poison(const void* slot, std::size_t block_size) noexcept {
    const auto* const payload = static_cast<const unsigned char*>(slot) + sizeof(void*);
    const std::size_t count = (block_size > sizeof(void*)) ? (block_size - sizeof(void*)) : 0U;
    for (std::size_t i = 0; i < count; ++i) {
        if (payload[i] != POISON_BYTE) {
            return false;
        }
    }
    return true;
}

// A fresh (never-user-owned) slot from initialize_free_list: poison its payload
// and stamp it free. Its next-link is written by initialize_free_list itself.
void on_init_slot(void* slot, std::size_t block_size) noexcept {
    poison_payload(slot, block_size);
    write_guard(slot, block_size, GUARD_FREED);
}

// On free: validate the guard word — a still-freed stamp is a double-free, any
// other non-allocated value is a write past block_size — then poison the
// payload and re-stamp the slot free.
//
// @return `true` if the caller should enqueue the block, `false` on a detected
// double-free. A double-freed block is *already* on the free list (from its
// first, legitimate free), so re-linking it would duplicate it / cycle the
// list. The default handler aborts before returning, so this only matters for a
// returning handler — but there it upholds the documented "no-further-
// corruption" contract (pool_hardening.hpp) by dropping the redundant push.
[[nodiscard]] bool on_free(void* slot, std::size_t block_size) noexcept {
    const std::uint64_t guard = read_guard(slot, block_size);
    bool double_free = false;
    if (guard == GUARD_FREED) {
        report(it::d4np::memorypool::HARDENING_DOUBLE_FREE, slot);
        double_free = true;
    } else if (guard != GUARD_ALLOCATED) {
        report(it::d4np::memorypool::HARDENING_OVERFLOW, slot);
    }
    poison_payload(slot, block_size);
    write_guard(slot, block_size, GUARD_FREED);
    return !double_free;
}

// On alloc: validate the free stamp (free-list integrity) and the payload
// poison (use-after-free), then stamp the slot allocated.
void on_alloc(void* slot, std::size_t block_size) noexcept {
    if (read_guard(slot, block_size) != GUARD_FREED) {
        report(it::d4np::memorypool::HARDENING_FREELIST_CORRUPTION, slot);
    }
    if (!payload_is_poison(slot, block_size)) {
        report(it::d4np::memorypool::HARDENING_USE_AFTER_FREE, slot);
    }
    write_guard(slot, block_size, GUARD_ALLOCATED);
}

}  // namespace hardening_detail

#endif  // PBR_MEMORY_POOL_HARDENING

// Reveal the next-free link (the safe-linking transform when hardened, else the
// plain load) WITHOUT the integrity check. Used where the slot may be read
// speculatively: the lock-free pop reads the head's link *before* the CAS
// establishes ownership, so those bytes may momentarily be another thread's
// user data — running the alignment check there would false-positive and abort
// a correct program (ADR-0043). The stale value is discarded when the CAS fails.
void* reveal_next(const void* slot) noexcept {
#if PBR_MEMORY_POOL_HARDENING
    return hardening_detail::xor_link(slot, *static_cast<void* const*>(slot));
#else
    return *static_cast<void* const*>(slot);
#endif
}

// Read the next-free link out of a genuinely-free slot, adding the safe-linking
// integrity check when hardened. Callers must own or quiesce the slot: the
// single-thread and mutex pops hold exclusivity and the diagnostic walk runs on
// a stable list. The lock-free pop uses reveal_next instead (see above) and
// defers integrity to on_alloc's guard-word check once the slot is owned.
//
// [[maybe_unused]]: under the LOCKFREE policy the pop path uses reveal_next, so
// read_next has no caller unless the diagnostic surface is also compiled in
// (PBR_MEMORY_POOL_DIAGNOSTICS). Without this, a LOCKFREE + diagnostics-off
// build trips -Wunused-function under warnings-as-errors.
[[maybe_unused]] void* read_next(const void* slot) noexcept {
    void* const revealed = reveal_next(slot);
#if PBR_MEMORY_POOL_HARDENING
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (revealed != nullptr && (reinterpret_cast<std::uintptr_t>(revealed) % POOL_ALIGNMENT) != 0U) {
        hardening_detail::report(it::d4np::memorypool::HARDENING_FREELIST_CORRUPTION, slot);
    }
#endif
    return revealed;
}

// Write the next-free link into a free slot — applying the safe-linking protect
// transform when hardened, else the plain store.
void write_next(void* slot, void* next) noexcept {
#if PBR_MEMORY_POOL_HARDENING
    // `slot` is a free-list slot inside a pool backing sized `stride * count`
    // (stride >= sizeof(void*)); the first sizeof(void*) bytes are always in
    // bounds. The analyzer cannot prove this with a symbolic block_size, so it
    // reports a false out-of-bounds store — suppressed narrowly.
    // NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
    *static_cast<void**>(slot) = hardening_detail::xor_link(slot, next);
#else
    *static_cast<void**>(slot) = next;
#endif
}

// The two size_t parameters (block_size, block_count) mirror the frozen
// memory_pool_create argument order; a config struct would diverge from that
// established shape, so the swappable-parameters check is suppressed here as it
// is on the create/factory entry points.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void initialize_free_list(void* backing, std::size_t block_size, std::size_t block_count) noexcept {
    // Implicit free list per ADR-0009 §1, ascending address order. Each free
    // slot stores the address of the next free slot in its own first
    // `sizeof(void*)` bytes (via write_next — a plain store, or the safe-linked
    // store when hardened); the last slot stores a null pointer. `stride` is the
    // physical slot size (== block_size unless hardening reserves a guard word).
    const std::size_t stride = slot_stride(block_size);
    auto* const base = static_cast<unsigned char*>(backing);
    for (std::size_t i = 0; i + 1U < block_count; ++i) {
        void* const this_slot = base + (i * stride);
        void* const next_slot = base + ((i + 1U) * stride);
        write_next(this_slot, next_slot);
#if PBR_MEMORY_POOL_HARDENING
        hardening_detail::on_init_slot(this_slot, block_size);
#endif
    }
    void* const last_slot = base + ((block_count - 1U) * stride);
    write_next(last_slot, nullptr);
#if PBR_MEMORY_POOL_HARDENING
    hardening_detail::on_init_slot(last_slot, block_size);
#endif
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
    // ADR-0026 — number of times this pool has grown (acquired an overflow
    // chunk). Written only on the rare growth slow path (grow_pool); read in
    // O(1) by memory_pool_growths so the Observer can detect growth without
    // touching the hot path. Atomic so observing a thread-safe pool is
    // data-race-free.
    std::atomic<std::size_t> grow_count_;
#if PBR_MEMORY_POOL_THREAD_SAFETY == PBR_MEMORY_POOL_THREAD_SAFETY_MUTEX
    // ADR-0020 §2 — the lock guarding the head pop/push in MutexPolicy.
    std::mutex mutex_;
#endif
};

// ADR-0015 §3 — per-pool metadata budget for the FIXED struct (the inline
// first chunk + bookkeeping). M5.3 added the dynamic-growth `grow_factor_` and
// M6.2 the Observer `grow_count_`, so the budget was renegotiated per
// ADR-0015 §4 (as ADR-0022/ADR-0023 anticipated) from 128 to 192: the struct
// is now 64 bytes under NONE, 80 under LOCKFREE, 144 under MUTEX — all
// comfortably under. Overflow Chunk descriptors are per-chunk metadata
// (O(chunks)) counted by memory_pool_metadata_bytes at run time, not in this
// compile-time gate; per-block overhead stays zero (ADR-0022 §3 / ADR-0024 §4).
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
    // Slots are `stride` apart (== block_size_ unless hardening reserves a
    // guard word — ADR-0043); the range and modulo checks must use the physical
    // stride, not the user-visible block_size_.
    const std::size_t stride = slot_stride(pool->block_size_);
    const std::uintptr_t end_addr = base_addr + (stride * count);
    if (block_addr < base_addr) {
        return false;
    }
    if (block_addr >= end_addr) {
        return false;
    }
    if (((block_addr - base_addr) % stride) != 0U) {
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
    // Guard the growth-count product itself before computing it: total can be large
    // after several growths, and total * (grow_factor_ - 1) could wrap size_t,
    // feeding the block_size guard below an already-wrapped `add` (BUG-0004).
    if (would_overflow_product(total, pool->grow_factor_ - 1U)) {
        return false;
    }
    const std::size_t add = total * (pool->grow_factor_ - 1U);
    if (add == 0U) {
        return false;
    }
    // Allocate by the physical stride (== block_size_ unless hardening reserves
    // a guard word — ADR-0043), and guard that product against size_t overflow.
    const std::size_t stride = slot_stride(pool->block_size_);
    if (would_overflow_product(add, stride)) {
        return false;
    }
    const std::size_t bytes = add * stride;

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
    // ADR-0026 — record the growth so the Observer can detect it in O(1).
    pool->grow_count_.fetch_add(1U, std::memory_order_relaxed);
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
        // and its contents are documented as indeterminate. read_next reveals
        // the safe-linked link when hardened, else a plain load (ADR-0043).
        pool->head_ = read_next(block);
#if PBR_MEMORY_POOL_HARDENING
        hardening_detail::on_alloc(block, pool->block_size_);
#endif
        return block;
    }

    static void push_head(memory_pool* pool, void* block) noexcept {
        // Mirrors the *static_cast<void**>(slot) = ptr idiom used in
        // initialize_free_list, through write_next: store the current head into
        // the block's first sizeof(void*) bytes, then make this block the new
        // head. When hardened, on_free validates the guard (double-free /
        // overflow) and poisons the payload first; a detected double-free is
        // dropped rather than re-linked (ADR-0043).
#if PBR_MEMORY_POOL_HARDENING
        if (!hardening_detail::on_free(block, pool->block_size_)) {
            return;
        }
#endif
        write_next(block, pool->head_);
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
        pool->head_ = read_next(block);
#if PBR_MEMORY_POOL_HARDENING
        hardening_detail::on_alloc(block, pool->block_size_);
#endif
        return block;
    }

    static void push_head(memory_pool* pool, void* block) noexcept {
        const std::lock_guard<std::mutex> guard{pool->mutex_};
        // Hardening validation + poison run under the held lock; a detected
        // double-free is dropped rather than re-linked (ADR-0043). The early
        // return releases the lock on scope exit as usual.
#if PBR_MEMORY_POOL_HARDENING
        if (!hardening_detail::on_free(block, pool->block_size_)) {
            return;
        }
#endif
        write_next(block, pool->head_);
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
            // reveal_next, NOT read_next: this read is speculative — the slot
            // is not owned until the CAS below succeeds, so another thread may
            // have popped it and filled it with user data. Running the
            // safe-link integrity check here would false-positive; a genuinely
            // corrupt link is instead caught by on_alloc's guard check once the
            // slot is owned (ADR-0043).
            void* const next = reveal_next(expected.ptr_);
            const TaggedHead desired{next, expected.tag_ + 1U};
            if (pool->head_.compare_exchange_weak(expected, desired, std::memory_order_acq_rel)) {
#if PBR_MEMORY_POOL_HARDENING
                hardening_detail::on_alloc(expected.ptr_, pool->block_size_);
#endif
                return expected.ptr_;
            }
        }
    }

    static void push_head(memory_pool* pool, void* block) noexcept {
        // The guard check + poison depend only on `block`, so run them once
        // before the publish loop; the safe-linked next-link (write_next)
        // depends on `expected` and is rewritten on each CAS retry. A detected
        // double-free is dropped rather than re-linked (ADR-0043).
#if PBR_MEMORY_POOL_HARDENING
        if (!hardening_detail::on_free(block, pool->block_size_)) {
            return;
        }
#endif
        TaggedHead expected = pool->head_.load(std::memory_order_relaxed);
        TaggedHead desired{};
        do {
            write_next(block, expected.ptr_);
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
    // Allocate by the physical stride (== block_size unless hardening reserves
    // a trailing guard word — ADR-0043) and guard that product for overflow.
    const std::size_t stride = slot_stride(block_size);
    if (would_overflow_product(stride, block_count)) {
        return nullptr;
    }

    const std::size_t total_bytes = stride * block_count;

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
    pool->grow_count_.store(0U, std::memory_order_relaxed);  // ADR-0026

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

std::size_t memory_pool_growths(const memory_pool_t* pool) {
    // ADR-0026 — number of times the pool has grown (acquired an overflow
    // chunk). O(1), NULL-tolerant, always present. The Observer reads this
    // after each allocation to detect growth without touching the hot path.
    // Always 0 for a fixed pool and for the lock-free policy (which never
    // grows — ADR-0024 §2).
    if (pool == nullptr) {
        return 0U;
    }
    return pool->grow_count_.load(std::memory_order_relaxed);
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
    // read_next reveals the safe-linked next-link under hardening (else a plain
    // load), so the diagnostic walk is correct in both configurations.
    return read_next(current);
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
        slot = read_next(slot);
    }
    return count;
}

#endif  // PBR_MEMORY_POOL_DIAGNOSTICS

}  // extern "C"

namespace it::d4np::memorypool {

#if PBR_MEMORY_POOL_HARDENING

namespace {

// The handler is noexcept (it is called from the pool's noexcept (de)allocate
// path). `operator<<` on std::cerr is technically permitted to throw, which the
// analyzer flags as an exception escaping a noexcept function; here it cannot
// matter — a throw would call std::terminate, and this handler unconditionally
// std::abort()s one line later, so both outcomes are the same loud crash.
// NOLINTNEXTLINE(bugprone-exception-escape)
void default_hardening_violation_handler(const char* kind, const void* block) noexcept {
    // ADR-0043 / ADR-0012 — a hardening violation is a defined, loud failure:
    // emit a diagnostic and abort. std::cerr (not a vararg printf) keeps the
    // handler clang-tidy-clean. Tests install a recording handler to assert a
    // violation without terminating the process.
    std::cerr << "[pbr-memory-pool] hardening violation: " << kind << " at " << block << '\n';
    std::abort();
}

std::atomic<HardeningViolationHandler>& handler_slot() noexcept {
    // A function-local static avoids a mutable namespace-scope global
    // (cppcoreguidelines-avoid-non-const-global-variables) while keeping the
    // handler process-wide and safe to swap from any thread.
    static std::atomic<HardeningViolationHandler> slot{&default_hardening_violation_handler};
    return slot;
}

}  // namespace

HardeningViolationHandler set_hardening_violation_handler(HardeningViolationHandler handler) noexcept {
    if (handler == nullptr) {
        handler = &default_hardening_violation_handler;
    }
    return handler_slot().exchange(handler, std::memory_order_acq_rel);
}

HardeningViolationHandler hardening_violation_handler() noexcept {
    return handler_slot().load(std::memory_order_acquire);
}

#endif  // PBR_MEMORY_POOL_HARDENING

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
