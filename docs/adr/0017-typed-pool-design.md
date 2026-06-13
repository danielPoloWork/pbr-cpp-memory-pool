# ADR-0017: `TypedPool<T>` Design — Compile-Time Block-Size Derivation and the Two-Layer Typed Surface

- **Status:** Accepted
- **Date:** 2026-06-12
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§5 (the `block_size` preconditions and alignment guarantee the derivation satisfies by construction), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) (the `Pool` wrapper this type composes), [ADR-0011](0011-factory-method-and-builder-for-pool-construction.md) (the Factory Method mirrored on the typed surface), [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) (the dual-verb exception policy inherited verbatim), [ROADMAP](../../ROADMAP.md) §3.2 (this ADR's roadmap item) + §3.3 (the STL allocator Adapter that will sit beside this type).

## Context

The untyped `Pool` vends `void*` blocks: the caller carries the burden of choosing a spec-§2.1-conformant `block_size` for the objects it intends to store, of casting every returned pointer, and of pairing every allocation with object construction by hand. Milestone 3.2 adds the type-safe layer — `it::d4np::memorypool::TypedPool<T>` — with RAII lifetime and the `try_allocate` / `allocate` verb pair fixed by [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2.

Three design questions need an answer before the template is written:

1. **Who computes `block_size`?** `sizeof(T)` alone violates the ADR-0009 §2 preconditions for most types — `sizeof(int) == 4` is below the `sizeof(void*)` floor on every Tier-1 host, `sizeof(T)` is rarely a multiple of `alignof(std::max_align_t)` — so a naïve `Pool(sizeof(T), n)` fails its preconditions for the very types a typed pool exists to serve.
2. **What about over-aligned types?** The pool's alignment guarantee is `alignof(std::max_align_t)` (ADR-0009 §5). A `T` declared `alignas(64)` would silently receive under-aligned storage — undefined behaviour the type system should reject, not document.
3. **What does a typed pointer mean?** A `T*` aimed at raw, uninitialized storage is a loaded weapon: nothing at the address is a `T` yet, but every reader of the signature assumes one is. The surface must make the storage-vs-object distinction explicit instead of leaving it to a Doxygen footnote.

## Decision

### 1. Composition over `Pool`; header-only template

`TypedPool<T>` lives in the new header [`typed_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/typed_pool.hpp) and holds a single `Pool pool_` member — composition, not inheritance, and not a parallel implementation against the raw C API. Everything ADR-0010/0011/0016 decided is inherited structurally: move-only RAII lifetime (the implicitly-generated move operations forward to `Pool`'s), `sizeof(TypedPool<T>) == sizeof(void*)`, the throwing ctor, and the non-throwing `make` Factory Method mirrored through a private adopt-`Pool` ctor. Being a template, the type is header-only by necessity; it adds zero object code to the static library and zero per-pool metadata beyond `struct memory_pool` (ADR-0015 unaffected).

### 2. `block_size` is derived at compile time; over-aligned `T` is rejected at compile time

```cpp
static constexpr std::size_t block_size() noexcept {
    constexpr std::size_t floor = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
    constexpr std::size_t align = alignof(std::max_align_t);
    return ((floor + align - 1U) / align) * align;
}
```

`max(sizeof(T), sizeof(void*))` rounded up to the next multiple of `alignof(std::max_align_t)` satisfies every ADR-0009 §2 precondition **by construction** — a `TypedPool` construction can only fail on `block_count == 0`, on `size_t` overflow, or on genuine OOM. The function is `public` and `constexpr` so tests and capacity planners can see the real per-slot footprint (e.g. `TypedPool<int>::block_size()` is 8 or 16 depending on the host's `alignof(std::max_align_t)`, not 4).

Over-aligned types are rejected with `static_assert(alignof(T) <= alignof(std::max_align_t), ...)` — a compile error today rather than silent UB, and an explicit renegotiation point if a future milestone wants `std::align_val_t`-backed typed pools (that would amend ADR-0009 §4/§5, not this ADR).

### 3. Two-layer surface: storage verbs and object-lifetime verbs

| Layer | Members | Semantics |
|-------|---------|-----------|
| Storage | `allocate()` / `try_allocate()` / `deallocate(T*)` | ADR-0016 verbs, typed: the returned `T*` aims at **uninitialized storage** — no `T` exists there until the caller placement-news one. `deallocate` runs **no destructor**. |
| Object lifetime | `construct(Args&&...)` / `destroy(T*)` | `construct` allocates a slot and placement-news `T(std::forward<Args>(args)...)` in it; `destroy` runs `~T()` and returns the slot. |

Pairing is normative: `allocate`-family slots are released with `deallocate`; `construct`-ed objects are released with `destroy`. Crossing the layers (e.g. `destroy` on a never-constructed slot) is undefined behaviour, documented as such.

`construct` provides the **strong exception guarantee**: if `T`'s constructor throws, the slot is pushed back onto the free list before the exception propagates — pool capacity is invariant across a failed `construct`. Exhaustion still surfaces as `std::bad_alloc` per ADR-0016 §2. `destroy` is `noexcept` and assumes `~T()` does not throw — the same contract every standard allocator-aware container imposes; a throwing destructor terminates, by design.

## Alternatives Considered

- **Inheritance (`TypedPool<T> : Pool`).** Rejected. A `TypedPool` is not substitutable for a `Pool` — passing it where a `Pool&` is expected re-opens the untyped `void*` surface the type exists to close, and public inheritance would expose the raw verbs alongside the typed ones with no way to retire them.
- **`block_size = sizeof(T)` with runtime failure.** Rejected. Every `TypedPool<int>` would fail ADR-0009 §2 at runtime on every Tier-1 host — a guaranteed-broken default. The compile-time derivation moves the rule from "caller must know the ADR" to "the type system applies the ADR".
- **Silent support for over-aligned `T` (under-aligned storage).** Hard-rejected — that is UB by [basic.align], not a feature. The `static_assert` makes the limit loud; lifting it is a future ADR-0009 amendment with its own backing-allocation path.
- **Object-lifetime-only surface (hide `allocate` / `try_allocate`).** Rejected. The roadmap item names the storage verbs explicitly, and they are the right substrate for callers doing batched or deferred construction (and for the M3.3 Adapter's `allocate(n)` semantics, which hand out storage, not objects).
- **`try_construct` (in-band `nullptr` on exhaustion, but propagating `T`-ctor exceptions).** Rejected. A verb that is "noexcept for one failure mode, throwing for the other" satisfies nobody and reads as neither; callers wanting non-throwing composition use `try_allocate` + placement-new under their own policy.
- **`std::unique_ptr<T, PoolDeleter>`-returning factory.** Deferred, not adopted. The deleter must hold a back-pointer to the pool, which dangles if the pool is moved or destroyed while handles are outstanding — a lifetime contract that deserves its own design pass (a candidate for the Milestone 6 ergonomics/observability wave), not a footnote here.

## Consequences

**Positive**

- The ADR-0009 preconditions become unviolatable through the typed surface — the first layer of the project where the contract is enforced by construction instead of by runtime validation.
- ADR-0016's verb convention is reused verbatim on its first new surface, validating the "no future ADR needs to re-litigate the naming" claim.
- `construct`'s strong exception guarantee makes the typed pool safe for types with throwing constructors — pool capacity is invariant across failures, which the test suite proves with a deliberately-throwing type.
- Header-only: zero library-ABI impact, zero metadata-budget impact (ADR-0015 untouched).

**Negative**

- Padding is invisible at the call site: `TypedPool<int>` spends `block_size()` bytes per slot, not 4. The `constexpr` accessor is the mitigation — capacity planning can be exact and is `static_assert`-able by consumers.
- The storage-vs-object layer split puts a pairing rule on the caller (`deallocate` vs `destroy`); misuse is documented UB rather than detected. Detection (a debug-mode constructed-slot bitmap) belongs to the Milestone 6 instrumentation wave alongside double-free detection (ADR-0012's deferral).
- Two compile-time rejections (`static_assert` on alignment, reference/void types) mean some generic code must constrain before instantiating; this is deliberate fail-loud.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0017.
- [`ROADMAP.md`](../../ROADMAP.md) §3.2 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M3.2)* entry.

[`typed_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/typed_pool.hpp) and [`typed_pool_test.cpp`](../../src/test/cpp/it/d4np/memorypool/typed_pool_test.cpp) land the template and its dedicated doctest binary (registered with CTest as `typed_pool`).

## References

- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§5 — the preconditions and alignment guarantee the derivation encodes.
- [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) §2 — the `allocate` / `try_allocate` verb convention.
- ISO C++17 [basic.align], [expr.new] — placement new into suitably-aligned raw storage.
- ISO C++17 [allocator.requirements] — the `construct` / `destroy` member naming precedent (`std::allocator_traits`).
