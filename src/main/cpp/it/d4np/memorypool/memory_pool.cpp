// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file memory_pool.cpp
 * @brief Milestone 1 stub implementations for the public C and C++ surface.
 *
 * Every function defined here is a no-op so that consumers can link against
 * `libpbr_memory_pool` immediately and exercise the API surface (compile-time
 * checks, IDE auto-complete, smoke tests) before the real implementation
 * lands. The real implementations replace these stubs in Milestone 2 — see
 * ROADMAP items 2.3 and 2.4 — and must satisfy the contracts documented in
 * `<it/d4np/memorypool/memory_pool.h>`.
 *
 * Each stub:
 *   - returns the documented "failure" value (`nullptr` for the alloc paths)
 *     so consumer code that does `if (pool == nullptr) ...` keeps working;
 *   - performs no observable side effect;
 *   - silences unused-parameter warnings by omitting the parameter name.
 */

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>

extern "C" {

memory_pool_t* memory_pool_create(size_t /*block_size*/, size_t /*block_count*/) {
    // M2.3 will allocate the contiguous backing store and initialise the
    // implicit free list per spec §4.
    return nullptr;
}

void* memory_pool_alloc(memory_pool_t* /*pool*/) {
    // M2.4 will pop the head of the implicit free list in O(1).
    return nullptr;
}

void memory_pool_free(memory_pool_t* /*pool*/, void* /*block*/) {
    // M2.4 will push the block back onto the implicit free list in O(1).
}

void memory_pool_destroy(memory_pool_t* /*pool*/) {
    // M2.3 will free the contiguous backing store per spec §3.1.
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
