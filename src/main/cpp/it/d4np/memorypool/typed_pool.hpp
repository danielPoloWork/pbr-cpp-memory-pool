// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_TYPED_POOL_HPP_
#define IT_D4NP_MEMORYPOOL_TYPED_POOL_HPP_

/**
 * @file typed_pool.hpp
 * @brief Type-safe C++17 pool vending `T`-typed blocks — ADR-0017.
 *
 * `TypedPool<T>` composes the untyped `Pool` (ADR-0010) and derives a
 * spec-§2.1-conformant `block_size` from `T` at compile time, so the
 * ADR-0009 §2 preconditions are satisfied by construction rather than by
 * caller diligence. The exception policy is inherited verbatim from
 * ADR-0016: `allocate` / `construct` throw `std::bad_alloc`,
 * `try_allocate` is `noexcept` and reports failure in-band.
 *
 * Header-only by necessity (class template); adds zero object code to
 * the static library and zero per-pool metadata (ADR-0015 unaffected).
 */

#include <it/d4np/memorypool/memory_pool.hpp>

#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace it::d4np::memorypool {

/**
 * @brief Move-only, RAII, type-safe pool of fixed `T`-sized slots.
 *
 * Two-layer surface per ADR-0017 §3 — pairing across layers is undefined
 * behaviour:
 *
 * - **Storage verbs** — ::allocate / ::try_allocate return a `T*` aimed
 *   at *uninitialized storage* (no `T` object exists there yet);
 *   ::deallocate returns such a slot without running any destructor.
 * - **Object-lifetime verbs** — ::construct placement-news a `T` into a
 *   fresh slot; ::destroy runs `~T()` and returns the slot.
 *
 * Lifetime, move-only semantics, and `sizeof(TypedPool<T>) ==
 * sizeof(void*)` are inherited structurally from the composed `Pool`
 * (ADR-0010 §2): the implicitly-generated move operations forward to
 * `Pool`'s, copy is implicitly deleted, and a moved-from `TypedPool` is
 * a valid empty shell whose verbs report exhaustion (ADR-0016 §2).
 *
 * @tparam T Object type to vend. Must not be over-aligned — the pool's
 *           alignment guarantee is `alignof(std::max_align_t)`
 *           (ADR-0009 §5); stricter alignment is rejected at compile
 *           time per ADR-0017 §2.
 */
template <typename T>
class TypedPool {
    static_assert(!std::is_void_v<T>, "TypedPool<T>: T must be an object type; use Pool for raw void* blocks");
    static_assert(!std::is_reference_v<T>, "TypedPool<T>: T must be an object type, not a reference");
    static_assert(alignof(T) <= alignof(std::max_align_t),
                  "TypedPool<T>: over-aligned types are not supported — the pool guarantees "
                  "alignof(std::max_align_t) only (ADR-0009 §5, ADR-0017 §2)");

public:
    /**
     * The per-slot footprint in bytes — `max(sizeof(T), sizeof(void*))`
     * rounded up to the next multiple of `alignof(std::max_align_t)`, so
     * every ADR-0009 §2 precondition holds by construction (ADR-0017 §2).
     * Public and `constexpr` so capacity planning can be exact: a
     * `TypedPool<int>` slot costs this many bytes, not `sizeof(int)`.
     */
    static constexpr std::size_t block_size() noexcept {
        constexpr std::size_t floor = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
        constexpr std::size_t align = alignof(std::max_align_t);
        return ((floor + align - 1U) / align) * align;
    }

    /**
     * Construct a typed pool with capacity for @p block_count slots.
     *
     * @param block_count Number of `T` slots; must be greater than zero
     *                    and `block_size() * block_count` must not
     *                    overflow `size_t` (ADR-0009 §3).
     *
     * @throw std::bad_alloc when the underlying `memory_pool_create`
     *        fails — degenerate @p block_count, `size_t` overflow, or
     *        backing-storage OOM (ADR-0016 §3). The ADR-0009 §2
     *        `block_size` preconditions cannot fail here: ::block_size
     *        satisfies them by construction.
     */
    explicit TypedPool(std::size_t block_count) : pool_(block_size(), block_count) {}

    /**
     * Factory mirroring `Pool::make` (ADR-0011, restructured per
     * ADR-0016 §3): failure as a value instead of an exception.
     *
     * @param block_count Same contract as the ctor.
     * @return Engaged optional on success, `std::nullopt` on any failure.
     */
    [[nodiscard]] static std::optional<TypedPool> make(std::size_t block_count) {
        std::optional<Pool> pool = Pool::make(block_size(), block_count);
        if (!pool.has_value()) {
            return std::nullopt;
        }
        return {TypedPool{std::move(*pool)}};
    }

    /**
     * Allocate one slot of uninitialized storage in O(1) — throwing verb
     * (ADR-0016 §2).
     *
     * @return Pointer to storage suitably sized and aligned for one `T`.
     *         **No `T` object exists at the address** — placement-new one
     *         before use, or call ::construct instead.
     *
     * @throw std::bad_alloc on exhaustion or on an empty (moved-from)
     *        pool.
     */
    [[nodiscard]] T* allocate() {
        return static_cast<T*>(pool_.allocate());
    }

    /**
     * Allocate one slot of uninitialized storage in O(1) — non-throwing
     * verb (ADR-0016 §2).
     *
     * @return Pointer to storage for one `T` (same caveat as ::allocate),
     *         or `nullptr` on exhaustion / empty pool.
     */
    [[nodiscard]] T* try_allocate() noexcept {
        return static_cast<T*>(pool_.try_allocate());
    }

    /**
     * Return a storage slot obtained from ::allocate / ::try_allocate to
     * the pool in O(1). Runs **no destructor** — a `T` constructed in the
     * slot must be destroyed first (or use ::destroy, which does both).
     *
     * @param block Slot to release, or `nullptr` (no-op). Foreign or
     *              misaligned pointers are a silent no-op per ADR-0012.
     */
    void deallocate(T* block) noexcept {
        pool_.deallocate(block);
    }

    /**
     * Allocate a slot and placement-new a `T` into it — the
     * object-lifetime verb (ADR-0017 §3).
     *
     * Strong exception guarantee: if `T`'s constructor throws, the slot
     * is returned to the free list before the exception propagates, so
     * pool capacity is invariant across a failed construct.
     *
     * @param args Forwarded to `T`'s constructor.
     * @return Pointer to the live `T`. Release with ::destroy.
     *
     * @throw std::bad_alloc on exhaustion or on an empty (moved-from)
     *        pool; anything `T(args...)` throws is propagated unchanged.
     */
    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        void* const raw = pool_.allocate();
        try {
            return ::new (raw) T(std::forward<Args>(args)...);
        } catch (...) {
            pool_.deallocate(raw);
            throw;
        }
    }

    /**
     * Destroy a `T` obtained from ::construct and return its slot to the
     * pool in O(1).
     *
     * `noexcept`: like every standard allocator-aware component, the pool
     * assumes `~T()` does not throw — a throwing destructor terminates.
     *
     * @param obj Object to destroy, or `nullptr` (no-op).
     */
    void destroy(T* obj) noexcept {
        if (obj == nullptr) {
            return;
        }
        obj->~T();
        pool_.deallocate(obj);
    }

    /**
     * @return The underlying C handle (the wrapper retains ownership) —
     *         interop escape hatch, same contract as `Pool::native_handle`.
     */
    [[nodiscard]] memory_pool_t* native_handle() noexcept {
        return pool_.native_handle();
    }

    /**
     * Per-pool metadata overhead in bytes — forwards through `Pool` to
     * ::memory_pool_metadata_bytes (spec §3.2 / ADR-0015). O(1) in both
     * `block_count` and `sizeof(T)`; 0 on a moved-from pool.
     */
    [[nodiscard]] std::size_t metadata_bytes() const noexcept {
        return pool_.metadata_bytes();
    }

private:
    /** Adopt an already-constructed Pool — used by ::make (ADR-0016 §3). */
    explicit TypedPool(Pool&& pool) noexcept : pool_(std::move(pool)) {}

    Pool pool_;
};

}  // namespace it::d4np::memorypool

#endif  // IT_D4NP_MEMORYPOOL_TYPED_POOL_HPP_
