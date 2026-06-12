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

#include <cstddef>
#include <cstdint>
#include <limits>
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
// NOLINTNEXTLINE(readability-identifier-naming)
struct memory_pool {
    void* backing_;
    void* head_;
    std::size_t block_size_;
    std::size_t block_count_;
    std::size_t alignment_;
};

// ADR-0015 §3 — per-pool metadata budget. The current struct is 40 bytes
// on every Tier-1 64-bit host; the 128-byte ceiling gives 88 bytes of
// headroom (room for ~11 additional 8-byte fields) before a future
// milestone has to renegotiate. Compile-time gate so every cell of the
// 14-cell build matrix asserts the budget on every PR.
constexpr std::size_t METADATA_BUDGET_BYTES = 128U;
static_assert(sizeof(memory_pool) <= METADATA_BUDGET_BYTES,
              "ADR-0015 §3: per-pool metadata budget exceeded — update the ADR before raising");

namespace {

// ADR-0012 — the foreign-pointer / out-of-range pointer detection that
// gates memory_pool_free. The struct memory_pool is in scope here (the
// definition above precedes this namespace), so the helper can read
// the backing pointer, slot size, and slot count directly.
//
// Comparing `block_addr < base_addr` and `block_addr >= end_addr` via
// std::uintptr_t avoids C++17 [expr.rel]/4 unspecified behaviour on
// cross-allocation pointer `<` comparison — when `block` is genuinely
// foreign, the direct pointer-comparison form is UB, the uintptr_t
// form is portable. The two NOLINT annotations are narrow and pointed
// at exactly the use case the standard documents as the workaround.
bool is_block_in_range(const memory_pool* pool, const void* block) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto base_addr = reinterpret_cast<std::uintptr_t>(pool->backing_);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto block_addr = reinterpret_cast<std::uintptr_t>(block);
    const std::uintptr_t end_addr = base_addr + (pool->block_size_ * pool->block_count_);
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

    // Step 3 — initialise the implicit free list per ADR-0009 §1.
    initialize_free_list(backing, block_size, block_count);
    pool->head_ = backing;

    return pool;
}

void memory_pool_destroy(memory_pool_t* pool) {
    // ADR-0009 §7: nullptr is a documented no-op.
    if (pool == nullptr) {
        return;
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
    // ADR-0015 §1 — per-pool metadata is exactly the struct's footprint;
    // per-block metadata is 0 by construction (implicit free list,
    // ADR-0009 §1). The static_assert above gates `sizeof(memory_pool)`
    // against METADATA_BUDGET_BYTES at compile time so the value
    // returned here is always under budget. NULL is a defined no-op
    // returning 0 (no metadata), matching the rest of the API's
    // NULL-tolerance posture.
    if (pool == nullptr) {
        return 0U;
    }
    return sizeof(memory_pool);
}

std::size_t memory_pool_block_size(const memory_pool_t* pool) {
    // ADR-0018 §2 — introspection for capacity planning and for the STL
    // allocator adapter's fits-a-slot decision. The stored value is the
    // caller's original request verbatim (no silent rounding, ADR-0009
    // §2). NULL is a defined no-op returning 0, matching the C surface's
    // NULL-tolerance posture.
    if (pool == nullptr) {
        return 0U;
    }
    return pool->block_size_;
}

int memory_pool_owns(const memory_pool_t* pool, const void* block) {
    // ADR-0018 §2 — the ADR-0012 range + alignment check promoted to a
    // public predicate. Reports address ownership only (a free slot and
    // a live slot answer identically); never dereferences `block`.
    if (pool == nullptr) {
        return 0;
    }
    if (block == nullptr) {
        return 0;
    }
    return is_block_in_range(pool, block) ? 1 : 0;
}

void* memory_pool_alloc(memory_pool_t* pool) {
    // Pop the head of the implicit free list in O(1) (spec §2.2). NULL on
    // either a null pool or an exhausted pool — fixed-size mode per
    // ADR-0009 §7; dynamic growth on exhaustion arrives in Milestone 5.
    if (pool == nullptr) {
        return nullptr;
    }
    if (pool->head_ == nullptr) {
        return nullptr;
    }
    void* const block = pool->head_;
    // Advance the head to the next-link stored in this slot. The block
    // returned to the caller still carries the link value in its first
    // sizeof(void*) bytes; that is acceptable because the slot is now
    // user-owned and the contents are documented as indeterminate.
    pool->head_ = *static_cast<void**>(block);
    return block;
}

void memory_pool_free(memory_pool_t* pool, void* block) {
    // Push the block onto the head of the implicit free list in O(1)
    // (spec §2.3). Null pool, null block, and foreign-pointer /
    // out-of-range pointer are all silent no-ops per ADR-0012.
    if (pool == nullptr) {
        return;
    }
    if (block == nullptr) {
        return;
    }
    if (!is_block_in_range(pool, block)) {
        return;
    }
    // Store the current head into the block's first sizeof(void*) bytes,
    // then make this block the new head. Mirrors the
    // *static_cast<void**>(slot) = ptr idiom used in initialize_free_list.
    *static_cast<void**>(block) = pool->head_;
    pool->head_ = block;
}

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

PoolBuilder& PoolBuilder::with_block_size(std::size_t block_size) noexcept {
    block_size_ = block_size;
    return *this;
}

PoolBuilder& PoolBuilder::with_block_count(std::size_t block_count) noexcept {
    block_count_ = block_count;
    return *this;
}

std::optional<Pool> PoolBuilder::build() const {
    // Builder per ADR-0011 §2 — delegate to the Factory Method so the
    // failure semantics flow through a single point (Pool::make). build()
    // is const so the same configured builder can produce multiple
    // identically-configured pools without resetting state.
    return Pool::make(block_size_, block_count_);
}

}  // namespace it::d4np::memorypool
