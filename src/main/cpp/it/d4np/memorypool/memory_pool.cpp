// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file memory_pool.cpp
 * @brief Implementation of the public C surface (`memory_pool_create` and
 *        `memory_pool_destroy` per ADR-0009) and the C++ RAII forwarders
 *        for `it::d4np::memorypool::Pool` (per ADR-0010).
 *
 * `memory_pool_alloc` and `memory_pool_free` are still Milestone 1 stubs;
 * they are replaced in Milestone 2.4 with their O(1) free-list bodies.
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
#include <cstring>
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

    // Step 3 — initialise the implicit free list in ascending address
    // order (ADR-0009 §1). Each free slot stores the address of the next
    // free slot in its own first `sizeof(void*)` bytes; the last slot
    // stores a null pointer.
    //
    // We use `std::memcpy` rather than `*reinterpret_cast<void**>(slot)`
    // because the slot is still raw storage with no current object — the
    // memcpy path is unambiguously well-defined under C++17 lifetime
    // rules and compiles to the same single store on every Tier-1
    // toolchain (verified locally on GCC 13 / Clang 18 / MSVC 19.30).
    // The explicit `static_cast<const void*>(&next_slot)` is required
    // by clang-tidy's bugprone-multi-level-implicit-pointer-conversion
    // check: `&next_slot` is `void* const*` and the implicit conversion
    // to memcpy's `const void*` parameter would be a multi-level pointer
    // conversion — a common source of "I meant to copy the pointed-to
    // thing, not the pointer" bugs. The cast says explicitly that we
    // intend to copy `sizeof(void*)` bytes from the storage of the
    // local variable into the slot.
    auto* const base = static_cast<unsigned char*>(backing);
    for (std::size_t i = 0; i + 1U < block_count; ++i) {
        void* const this_slot = base + (i * block_size);
        void* const next_slot = base + ((i + 1U) * block_size);
        std::memcpy(this_slot, static_cast<const void*>(&next_slot), sizeof(void*));
    }
    // Terminate the chain on the last slot.
    void* const last_slot = base + ((block_count - 1U) * block_size);
    void* const null_link = nullptr;
    std::memcpy(last_slot, static_cast<const void*>(&null_link), sizeof(void*));

    pool->head_ = base;

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

void* memory_pool_alloc(memory_pool_t* /*pool*/) {
    // M2.4 will pop the head of the implicit free list in O(1).
    return nullptr;
}

void memory_pool_free(memory_pool_t* /*pool*/, void* /*block*/) {
    // M2.4 will push the block back onto the implicit free list in O(1).
}

}  // extern "C"

namespace it::d4np::memorypool {

Pool::Pool(std::size_t block_size, std::size_t block_count) : handle_(::memory_pool_create(block_size, block_count)) {}

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
    return ::memory_pool_alloc(handle_);
}

void Pool::deallocate(void* block) noexcept {
    ::memory_pool_free(handle_, block);
}

memory_pool_t* Pool::native_handle() noexcept {
    return handle_;
}

}  // namespace it::d4np::memorypool
