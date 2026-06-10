# ADR-0010: RAII for the `Pool` Wrapper and Pimpl Across the C/C++ Boundary

- **Status:** Accepted
- **Date:** 2026-06-10
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0003](0003-design-patterns-policy.md) (design-patterns policy — requires every adoption to be justified in an ADR), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §6 (the `struct memory_pool` field list this ADR decides *where* to put), [`src/main/cpp/it/d4np/memorypool/memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) (the public C header that already forward-declares `struct memory_pool`), [`src/main/cpp/it/d4np/memorypool/memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) (the C++ wrapper this ADR formalises), [`docs/patterns/README.md`](../patterns/README.md) (catalogue updated in the same PR), [ROADMAP](../../ROADMAP.md) §2.2.

## Context

The C++ surface of `pbr-cpp-memory-pool` is two thin layers stacked on the public C API:

- An **opaque type** — `typedef struct memory_pool memory_pool_t;` in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) — that hides the pool's internal layout from every consumer that includes the public header.
- A **C++17 wrapper** — `it::d4np::memorypool::Pool` declared in [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) — that turns the C lifetime contract into an owning, exception-safe handle.

The Milestone 1 scaffolding committed to *some* shape for both layers without ever writing down the rationale:

- The C public header forward-declares `struct memory_pool` and never defines it; consumers can only manipulate it through the four spec §5 functions. The struct's definition is reserved for `memory_pool.cpp`. This is **Pimpl** under a different name — the C-language equivalent of the classical *"opaque pointer to a private implementation type defined only in the .cpp"* pattern.
- The `Pool` class has `memory_pool_t* handle_` as its only data member, with explicit constructor / destructor / move-constructor / move-assignment, copy operations deleted, and `allocate` / `deallocate` / `native_handle` forwarders. This is **RAII** — the C++17 idiom in which an object's destructor owns the resource's lifetime.

Both patterns are already *shaped* in the M1 skeleton; the work this ADR does is *formalising* them — recording the rationale, the boundary, and the alternatives — so the [design-patterns policy](0003-design-patterns-policy.md) is honoured and so M2.3's implementation lands against a frozen contract rather than incidental scaffolding choices.

The two patterns are **co-introduced and interdependent**, which is why they share a single ADR (the threshold from [`AGENTS.md`](../../AGENTS.md) §8 #5 — *"Multi-pattern PRs may bundle into a single ADR only when the patterns are co-introduced and interdependent"*). The dependency is direct: the RAII wrapper's *resource* **is** the Pimpl handle. Designing one without the other would create awkward seams — a Pool that owns nothing, or a Pimpl with no owner.

[ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §6 fixed the *contents* of `struct memory_pool` (the five fields `backing` / `head` / `block_size` / `block_count` / `alignment`); this ADR fixes *where* the struct lives and how the C++ side owns its lifetime.

## Decision

### 1. C-style Pimpl across the C/C++ boundary

`struct memory_pool` is **defined exclusively in `memory_pool.cpp`** (the full definition lands in M2.3 — replacing the M1 stubs). The public C header [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) carries only the forward declaration:

```c
typedef struct memory_pool memory_pool_t;
```

Every consumer of the public C API sees the type as an *opaque handle* — its layout is unknown, its size is unknown, it cannot be allocated on the stack, embedded as a value member, or `sizeof`-d. The only legitimate operations are the four spec §5 functions, all of which take a `memory_pool_t*`.

This is the C-language equivalent of the classical C++ Pimpl idiom. In classical C++ Pimpl, a public class holds `std::unique_ptr<Impl>` pointing at an `Impl` struct fully defined in the .cpp; the public header sees only `class Impl;`. In our setup, the **C handle is the Impl** — the forward declaration in the header gives the same encapsulation property, and the C++ wrapper can reuse the handle directly instead of introducing a second layer of indirection.

### 2. RAII for the C++ `Pool` wrapper

[`it::d4np::memorypool::Pool`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) is an **owning, move-only RAII handle** around `memory_pool_t*`. The contract is fixed as follows:

| Member                                | Semantics                                                                                                            |
|---------------------------------------|----------------------------------------------------------------------------------------------------------------------|
| `Pool(size_t bsz, size_t bct)`        | Calls `memory_pool_create(bsz, bct)`; stores the returned handle in `handle_`. On failure (ADR-0009 §7 conditions or `std::bad_alloc` caught at the C ABI boundary) `handle_` is `nullptr`; the wrapper is in a valid empty state. |
| `~Pool() noexcept`                    | Calls `memory_pool_destroy(handle_)`. The C function tolerates `nullptr`, so the destructor is unconditionally safe. |
| `Pool(const Pool&)`                   | **Deleted.** A pool handle has unique ownership of its backing buffer; copying would double-free at destruction.     |
| `Pool& operator=(const Pool&)`        | **Deleted.** Same reason as copy-construction.                                                                       |
| `Pool(Pool&& o) noexcept`             | Takes `o.handle_`; sets `o.handle_ = nullptr`. Moved-from state is the *valid empty Pool* (subsequent `allocate` / `deallocate` are well-defined no-ops; the destructor is safe). |
| `Pool& operator=(Pool&& o) noexcept`  | Self-assignment guard (`if (this != &o)`), `memory_pool_destroy(handle_)`, then take `o.handle_` and null `o.handle_`. |
| `void* allocate()`                    | Forwards to `memory_pool_alloc(handle_)`. Returns `nullptr` on null handle or pool exhaustion. The `NULL` → `std::bad_alloc` translation is deferred to ROADMAP §3.1. |
| `void deallocate(void* b) noexcept`   | Forwards to `memory_pool_free(handle_, b)`. No-op when either argument is null.                                      |
| `memory_pool_t* native_handle() noexcept` | Returns `handle_`. The wrapper retains ownership; the caller must not call `memory_pool_destroy` on the returned pointer. |

`Pool` carries **exactly one** data member — `memory_pool_t* handle_`. No mutex, no atomic refcount, no allocator instance. The class is `sizeof(void*)` and a moved-from instance is indistinguishable from a default-empty one at the binary level. Thread-safety is M4's concern and will arrive either through a wrapper subclass / template parameter or through a side-channel mutex held by the C handle — not by changing this layout.

`Pool` is **not** itself the public C API. Consumers who want a pool without C++ continue to call `memory_pool_create` / `_alloc` / `_free` / `_destroy` directly; the wrapper exists for C++ ergonomics on top of the unchanged C surface.

### 3. Boundary between the two patterns

The two decisions interlock at one point:

```
public C header (memory_pool.h)
└── forward-declares struct memory_pool                          ← Pimpl
        ↑
        └── implementation file (memory_pool.cpp)
                ├── defines struct memory_pool                   ← Pimpl
                └── defines Pool member functions                ← RAII bodies
                        which forward to memory_pool_*           ← uses the handle
                            (NULL-safe per the C contract)

public C++ header (memory_pool.hpp)
└── declares class Pool { memory_pool_t* handle_; };             ← RAII interface
```

The RAII wrapper *never* dereferences `handle_` directly — it goes through the C API. The C functions check for `nullptr` (per [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §7), so the wrapper inherits null-safety for free. There is exactly **one** code path that knows the layout of `struct memory_pool` — the implementation file — and the rest of the world (including the wrapper) goes through the four-function C surface.

## Alternatives Considered

- **Classical C++ Pimpl: `Pool` holds `std::unique_ptr<Pool::Impl>` to a separate `Impl` struct.** Rejected. Would force two parallel state holders — the C `struct memory_pool` (for C consumers) and `Pool::Impl` (for C++ consumers) — duplicating the field list from [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §6 and creating two allocation paths to keep in sync. The C handle is already opaque to consumers (Pimpl property satisfied) and already encapsulates the layout (the goal of Pimpl in the first place); inventing a second Impl is overhead with no encapsulation gain.
- **Embed `struct memory_pool` as a value member of `Pool`.** Rejected. Would require the struct's full definition to be visible in `memory_pool.hpp`, defeating Pimpl: every internal field change becomes an ABI break for every C++ consumer, and the C handle could no longer be passed to or from C code that has only the forward declaration. The "no Pimpl" path is exactly the one we are rejecting in this ADR.
- **`std::shared_ptr<memory_pool_t>` instead of move-only ownership.** Rejected. The pool's ownership model is unique by design — sharing a pool handle across multiple owners is not a use case the spec calls for, and the atomic refcount overhead on every wrapper copy would harm the very `O(1)` allocation property the project is built to demonstrate. If shared ownership ever becomes a real need, a separate `SharedPool` wrapper around `std::shared_ptr<Pool>` can be added without amending this ADR.
- **Make `Pool` copyable by deep-cloning the pool state.** Rejected. The backing buffer is the heavy state, and "what does copy mean for a pool with outstanding allocated blocks?" has no defensible single answer — re-issuing the same addresses on the copy is unsafe, returning the blocks to the new pool's free list is a silent semantic change, and tracking which blocks are live requires the explicit metadata that [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) was at pains to avoid. Move-only is the only sane stance for an owning allocator wrapper.
- **Strong exception guarantee on `Pool(size_t, size_t)`: throw `std::bad_alloc` immediately on `NULL` return.** Rejected *for M2*. The C++ wrapper's exception policy at the C/C++ boundary is deliberately left to [ROADMAP](../../ROADMAP.md) §3.1, where it gets its own ADR. Pre-empting that decision here would force one of `bad_alloc` / `noexcept` / configurable-via-macro without considering the interplay with the `std::allocator` adapter (ROADMAP §3.3) — same reason ADR-0009 §7 explicitly defers the translation.
- **Skip the patterns catalogue update — Pimpl and RAII are "C++-idiomatic complements" already noted in `docs/patterns/README.md`.** Rejected. The catalogue's policy from [ADR-0003](0003-design-patterns-policy.md) is that every *adoption* migrates from the candidate list (or the idiomatic-complements list) to the *Adopted / Planned* table, with the ADR linked. The "effectively required" note in the candidate list documents that they were on the radar; an ADR + table row documents that the project committed to them.

## Consequences

**Positive**

- The `Pool` wrapper makes the C lifetime contract impossible to violate from C++: a forgotten `memory_pool_destroy` becomes a compile-time absence of a destructor, not a runtime leak. The Valgrind job in M2.8 will exercise the wrapper as well as the C surface and benefit directly from this property.
- Pimpl insulates every consumer (C and C++) from `struct memory_pool` evolution. M4's thread-safety strategies can add a `std::mutex` or atomic state to the struct without changing the public C header, without touching `memory_pool.hpp`'s binary layout, without breaking ABI for any consumer that vendored a `v0.x` artifact.
- The RAII wrapper's move-only semantics mirror `std::unique_ptr`, which every C++ engineer recognises. The pattern is one of the most teachable elements of modern C++ and demonstrates it in the cleanest possible setting — a single owning pointer, no specialised behaviour.
- The C and C++ surfaces share one source of state truth. The implementation file is the *only* place that knows the struct layout; the wrapper and the C functions both manipulate it through the same field accesses (in M2.3's body). No two-sided invariants to keep in sync.
- The wrapper is `sizeof(void*)` — passable in registers on every Tier-1 platform. A moved-from `Pool` is binary-equal to a default-empty one, simplifying the M2.7 correctness tests for "wrapper is valid after move".

**Negative**

- The wrapper cannot perform any C++-side state inspection — every observation (head pointer, exhaustion, block count) must go through the C API. M3's diagnostic Iterator (ROADMAP §3.4) will need a dedicated C-side helper if it wants efficient read-only access to the free list, rather than calling `allocate` / `deallocate` in a probing loop. We accept the cost because the inspection use cases are rare and limited to diagnostics.
- Self-move-assignment (`p = std::move(p)`) requires the standard `if (this != &other)` guard — already present in `memory_pool.cpp`. Removing it would self-destroy the handle and leave the wrapper holding a dangling pointer; the M2.7 correctness tests will exercise the guard explicitly.
- The wrapper is deliberately minimal (`allocate` / `deallocate` / `native_handle` / move ops). Users wanting more — typed allocation, allocator integration, statistics — must wait for M3 (`TypedPool<T>`, allocator-aware) and M6 (Decorator). The minimal-surface choice is a feature: it keeps the M2 contract testable without speculating on future ergonomics.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/patterns/README.md`](../patterns/README.md) — two rows added to the *Adopted / Planned* table: `RAII` with code locations `memory_pool.hpp` (declaration) + `memory_pool.cpp` (forwarders) and `Pimpl` with `memory_pool.h` (forward declaration) + `memory_pool.cpp` (struct definition, lands in M2.3). Status `Planned` for both — the Pool class skeleton and the forward declaration are in place from M1.6, but the meaningful semantics arrive when the C functions get real bodies in M2.3. Status flips to `Implemented` in that PR.
- [`docs/adr/README.md`](README.md) — ADR-0010 index row.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry pointing at ADR-0010 and the catalogue rows.
- [`ROADMAP.md`](../../ROADMAP.md) §2.2 — checkbox flipped to `[x]` with the inline summary of the two patterns adopted.

The Doxygen comments on `memory_pool.h` (which currently say "the policy for detecting and reporting [foreign pointer] misuse is set in Milestone 2.7") and on `memory_pool.hpp` (which currently say "ADR pending" for the Pool wrapper) are **not** edited in this PR. Those updates land alongside the M2.3 implementation when the contract becomes binding on the bodies.

## References

- [ADR-0003](0003-design-patterns-policy.md) — design-patterns policy that mandates an ADR for every pattern adoption.
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §6 — the `struct memory_pool` field list this ADR places in the implementation file.
- [`docs/patterns/README.md`](../patterns/README.md) — the catalogue table where the two new rows live; the *C++-idiomatic complements* section at the bottom names RAII and Pimpl as "effectively required" — this ADR is what turns "expected" into "adopted".
- [`docs/patterns/design-patterns.md`](../patterns/design-patterns.md) — canonical taxonomy; *Pimpl* appears as a Structural / idiomatic complement, *RAII* under C++ idioms.
- [`AGENTS.md`](../../AGENTS.md) §8 #5 — the policy clause that allows two co-introduced patterns to share a single ADR.
- [`src/main/cpp/it/d4np/memorypool/memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) — the forward declaration that is the Pimpl mechanism's anchor.
- [`src/main/cpp/it/d4np/memorypool/memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) and [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) — the RAII wrapper's declaration and bodies (the latter forward to the C API which is still stub until M2.3 / M2.4).
