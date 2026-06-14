# ADR-0009: Free-List Layout, block_size Constraints, and Alignment Guarantee

- **Status:** Accepted
- **Date:** 2026-06-10
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §2.1 (pre-allocate contiguous pool) + §3.1 (no memory leak at destroy) + §3.2 (minimal metadata overhead) + §4 (Free List algorithm), [`src/main/cpp/it/d4np/memorypool/memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) (the public C contract this ADR turns from "ADR pending" into a frozen contract), [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 (C++17 baseline + ANSI C interop), [ROADMAP](../../ROADMAP.md) §2.1 (this ADR's roadmap item) + §2.7 (foreign-pointer policy that consumes the range information this ADR requires the pool struct to hold), `docs/patterns/README.md` (Object Pool catalogue entry whose "implementation notes" point here).

## Context

Milestone 1 left the public C API as a frozen four-function contract with stub implementations, deferring the *how* to Milestone 2. Before any code lands in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp), the project must lock down the structural choices that downstream items depend on:

- The Doxygen comments on [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) name "ADR (to be filed in Milestone 2.1)" three times — for the `block_size` minimum, for the alignment of `memory_pool_alloc`'s return value, and (implicitly through the wrapper) for what the `Pool` constructor's parameter contract means.
- The Milestone 2 implementation items (M2.3 `create`/`destroy`, M2.4 `alloc`/`free`, M2.5 RAII wrapper, M2.7 correctness tests) all need a single agreed-upon layout to target.
- The Milestone 2.10 metadata-overhead instrumentation is comparing the chosen layout against an asserted CI budget; without the layout fixed, the budget cannot be set.
- The Milestone 4 thread-safety strategies (lock-free, mutex, per-thread caches) operate on top of this layout — relaxing the free-list shape later means changing the contract that strategy ADRs build on.

The spec is explicit on the *algorithm* (§4 — implicit free list with the next-free pointer stored in the free block's own first bytes) but silent on the *operational details* that decide whether the implementation is correct and ergonomic in practice:

- How does the user know what `block_size` is legal? The spec's example says "first bytes hold the pointer" — that gives a *lower* bound (`sizeof(void*)`) but no upper bound and no alignment requirement.
- What alignment does the returned `void*` carry? Drop-in compatibility with `malloc` means `alignof(std::max_align_t)`; tighter or weaker contracts are possible and must be chosen, not defaulted into.
- What happens on illegal input — silent rounding, error code, undefined behaviour? The spec's `NULL`-on-exhaustion clause for `memory_pool_alloc` suggests a *no-exception, return-NULL* idiom for the C side; this ADR extends that to invalid construction too.
- How is the contiguous backing storage actually obtained? `malloc` does not give per-allocation alignment guarantees beyond `alignof(std::max_align_t)` and is not allowed in this project's no-external-dependency contract for non-stdlib code anyway. The implementation must pick one portable C++17 mechanism that satisfies the ANSI-C public surface above.

This ADR settles those points so M2.3 onwards is an implementation of a frozen contract, not a discovery exercise.

## Decision

### 1. Free-list layout (codifies spec §4)

The pool is a single contiguous buffer of `block_count` slots of equal size. Free slots are linked into a singly-linked **implicit free list** whose link pointer occupies the first `sizeof(void*)` bytes of each free slot itself; live (in-use) slots carry **zero** metadata overhead. The pool's opaque struct (`struct memory_pool`) holds a `head` field pointing at the first free slot (or `NULL` when exhausted); allocation is the classic *pop the head, advance head to `*head`*; deallocation is the inverse — *write the current head into `*block`, then move head to `block`*. Both operations are `O(1)`, branch-light, and touch one cache line on the hot path.

At construction the free list is initialised in **ascending address order**: the first slot points to the second, the second to the third, and so on until the last slot, which carries `NULL`. The order is observable to consumers only through the *sequence* in which `memory_pool_alloc` returns pointers; locking it down here makes microbenchmark results reproducible across runs and across the M2.9 benchmark CI cells.

### 2. `block_size` preconditions (strict, not lenient)

`memory_pool_create(block_size, block_count)` requires **all** of the following on `block_size`:

- `block_size > 0`;
- `block_size >= sizeof(void *)` — the implicit free-list link must fit;
- `block_size % alignof(std::max_align_t) == 0` — every slot must be naturally aligned for any standard type, matching the alignment users get from `malloc`.

If any precondition is violated, `memory_pool_create` returns `NULL`. The implementation does **not** silently round `block_size` up. The reasoning is documented under *Alternatives Considered* — short version: a reference implementation that teaches alignment must surface it, not hide it.

### 3. `block_count` precondition and overflow guard

`block_count` must be `> 0` (a pool with zero slots is degenerate and provides no value over `nullptr`). The product `block_size * block_count` must not overflow `size_t`; the check is `block_count > SIZE_MAX / block_size`. On either violation `memory_pool_create` returns `NULL`.

The overflow guard is mandatory rather than optional because the public API takes two independent user-supplied `size_t` parameters. Skipping the check turns an arithmetic surprise into a heap-corruption surprise — exactly the class of defect this project is supposed to teach how to *avoid*.

### 4. Backing allocation: aligned C++17 placement new

The contiguous buffer is obtained via the C++17 aligned allocation overloads of the global `operator new`:

```cpp
void* backing = ::operator new(block_size * block_count,
                               std::align_val_t{alignof(std::max_align_t)});
// matching release in memory_pool_destroy:
::operator delete(backing,
                  std::align_val_t{alignof(std::max_align_t)});
```

`operator new`/`operator delete` from `<new>` is the only standard C++17 mechanism that gives over-aligned allocation with a portable release path on every Tier-1 platform listed in ADR-0005 §1 — `aligned_alloc` (C11) is missing from MSVC's CRT in the supported floor, `posix_memalign` is POSIX-only, `_aligned_malloc` is MSVC-only and requires the matching `_aligned_free`. The choice keeps the implementation **zero-external-dependency** (spec §3.3, asserted by the `deps / zero external dependencies` CI job from [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §4) and **single-code-path**, avoiding the typical `#ifdef _MSC_VER` ladder.

The C public surface in `memory_pool.h` is unaffected — implementation-language choices are not part of the ABI. The ANSI C / C99 verification jobs continue to exercise only the header.

### 5. Returned pointer alignment guarantee

Every pointer returned by `memory_pool_alloc` is aligned to **`alignof(std::max_align_t)`** — the same guarantee as `malloc`. Because the backing buffer is over-aligned to that boundary (point 4) and `block_size` is required to be a multiple of it (point 2), every slot at offset `i * block_size` from the buffer base inherits the alignment trivially. Consumers can therefore use the returned pointer for any standard type, including `std::max_align_t`, `long double`, and `std::int64_t`, without further alignment juggling.

The Doxygen comments on `memory_pool.h` will be updated to reference this ADR (currently they say "aligned per ADR (Milestone 2.1)") in the same PR that lands the M2.3 implementation.

### 6. `struct memory_pool` field set (Pimpl layout decided in M2.2; field *list* decided here)

The opaque struct is hidden behind the `memory_pool_t*` forward declaration in the public header. **Where** it lives (statically inside `memory_pool.cpp` vs behind a `unique_ptr<Impl>` Pimpl in the C++ wrapper) is the M2.2 ADR's call. **What it needs to carry** is fixed here, because every subsequent milestone depends on the contents:

| Field           | Type          | Purpose                                                                                |
|-----------------|---------------|----------------------------------------------------------------------------------------|
| `backing`       | `void *`      | Base of the contiguous buffer; needed for `operator delete` at destroy time and for the foreign-pointer range check below. |
| `head`          | `void *`      | Current head of the implicit free list; `NULL` when the pool is exhausted.             |
| `block_size`    | `size_t`      | The effective slot size after validation. Needed for `free`'s range check and for the M2.10 metadata-overhead measurement. |
| `block_count`   | `size_t`      | Pool capacity. Needed for the range check (`backing` ≤ `block` < `backing + block_size * block_count`) the M2.7 correctness tests will exercise. |
| `alignment`     | `size_t`      | Recorded alignment used for `operator new`/`delete`. Kept for future flexibility when M5's dynamic-growth ADR may pick a different alignment per chunk. |

The struct is **pure data** at this stage; methods and synchronisation primitives belong to M2.5 (RAII wrapper) and M4 (threading) respectively. No padding optimisation work is in scope for M2 — the M2.10 instrumentation will measure whatever the natural layout produces.

### 7. Failure semantics

`memory_pool_create` returns `NULL` for **every** failure path:

- any of the preconditions in §2 / §3 violated;
- `block_size * block_count` overflows `size_t`;
- `::operator new` throws `std::bad_alloc` (the implementation catches it and converts to `NULL` — exceptions never cross the C ABI boundary, [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3).

The C++ `Pool` wrapper's translation of `NULL` into `std::bad_alloc` is a **separate** decision deferred to [ROADMAP](../../ROADMAP.md) §3.1; this ADR does not pre-empt it.

`memory_pool_alloc` returns `NULL` when `pool == NULL`, when `pool->head == NULL` (exhausted), and never reads memory it does not own. `memory_pool_free` is a no-op when `pool == NULL` or `block == NULL`; the M2.7 ADR will decide what to do for an in-range-but-misaligned `block` and for an out-of-range `block`. This ADR commits only to the *range check being possible* (the `backing` + `block_size * block_count` fields above are sufficient).

## Alternatives Considered

- **Explicit free list (separate metadata block — bitmap or linked-list table).** Rejected. The spec mandates the implicit layout in §4 (*"i suoi primi byte vengono utilizzati per memorizzare un puntatore al blocco libero successivo"*); even setting that aside, an explicit bitmap costs `ceil(block_count / 8)` bytes per pool and an explicit linked-list table costs `sizeof(size_t) * block_count`, both of which violate spec §3.2 (*"L'uso di metadati interni... deve essere minimo"*). Implicit gives zero per-block overhead while live, which is the strongest possible answer to §3.2.
- **Lenient `block_size`: silently round up to the next multiple of `alignof(std::max_align_t)`.** Rejected. The argument *for* is ergonomics — `memory_pool_create(8, 1000)` would Just Work on a platform where `alignof(std::max_align_t) == 16` by quietly becoming a 16-byte pool. The argument *against* is that a *reference implementation* whose audience is engineers learning fixed-block allocation must teach alignment, not hide it; silently doubling a user's per-slot footprint without warning is exactly the kind of friendly footgun the project should refuse to ship. The strict path makes the alignment requirement testable, surface-checkable from CI, and visible in the first failure rather than the hundredth performance measurement.
- **Weaker alignment guarantee: return pointers aligned only to `min(block_size, alignof(std::max_align_t))`.** Considered briefly because it matches what tcmalloc/jemalloc size classes do. Rejected for this project because the spec's public API is one signature `void* memory_pool_alloc(memory_pool_t*)` — no per-pool alignment parameter, no type information. A weaker guarantee would force consumers to cast the result and remember their pool's effective alignment manually; strong-and-uniform alignment makes the wrapper `TypedPool<T>` in M3.2 a trivial cast, no `alignas` gymnastics required.
- **Allocate per-chunk via `malloc`.** Rejected on two counts: (a) the spec's zero-external-dependency contract is implemented by the M1.11 CI audit, which currently asserts the static archive contains only project-local objects — adding a runtime dependency on `malloc` is technically fine (it's stdlib) but inconsistent with the C++17 internals + C public-surface split the project has otherwise committed to; (b) `malloc` only guarantees `alignof(std::max_align_t)` on the *first* byte of its return value, not on any chosen interior offset — buying the over-aligned backing via `operator new(..., std::align_val_t)` makes the slot-alignment proof a one-line argument instead of a platform-dependent footnote.
- **Initialise the free list in descending or randomised order.** Rejected. Ascending is the simplest invariant to verify in correctness tests (M2.7), and a deterministic order is required for the M2.9 benchmark to produce comparable numbers across CI runs. Randomisation has a use for *security* (defeating heap-grooming attacks) — out of scope for an in-process fixed-block pool used by trusted code; if it becomes relevant later it earns a separate ADR.
- **Lock the struct layout (Pimpl vs static) in this ADR.** Rejected — that's the next item ([ROADMAP](../../ROADMAP.md) §2.2). Scoping the layout decision here would either duplicate or pre-empt the Pimpl + RAII patterns ADR; both ADRs land before any code in M2.3 so neither blocks the other in practice. The field *contents* are decided here because every downstream item needs to know what data is present; *where* the struct lives is orthogonal.

## Consequences

**Positive**

- The contract is now testable end-to-end. M2.3 implements one well-defined function each for `create` and `destroy`; M2.4 implements `alloc` and `free` against one well-defined free-list shape; M2.7 writes correctness tests against an enumerated list of failure modes (`block_size = 0`, `block_size < sizeof(void *)`, `block_size = 17` on a `alignof(std::max_align_t) = 16` host, `block_count = 0`, `block_size * block_count` overflow, in-range / out-of-range / misaligned `block` in `free`). Every row in that table is reachable from this ADR's text.
- The "zero metadata overhead per live block" property (§3.2 of the spec) is preserved at its strongest form — live slots are pure user memory, no header, no footer, no out-of-band table. M2.10's metadata-overhead instrumentation has a tiny budget to assert against: the `struct memory_pool` itself plus the over-alignment slack at the head of the backing buffer (at most `alignof(std::max_align_t) - 1` bytes per pool).
- Microbenchmarks are deterministic. The ascending free-list order means M2.9's `1,000,000`-iteration loop touches addresses in a predictable sequence; differences between CI runs (or between `Debug` and `Release` builds) come from compiler / scheduler effects, not from per-pool layout randomness.
- The choice of C++17 over-aligned `operator new` keeps the implementation single-path on the three Tier-1 platforms (ADR-0005 §1) — no `#ifdef _MSC_VER` for `_aligned_malloc`, no `#ifdef __linux__` for `posix_memalign`. CI cells across GCC / Clang / MSVC / Apple Clang exercise the same code.

**Negative**

- Strict `block_size` validation makes the API slightly less ergonomic than common malloc-like allocators. Consumers who don't know `alignof(std::max_align_t)` for their platform get `NULL` on construction and must read the Doxygen / spec to understand why. Mitigation: the Doxygen on `memory_pool_create` (updated in M2.3) will spell out the three preconditions inline; the M2.7 test names will read `block_size_must_be_at_least_sizeof_void_p` and `block_size_must_be_aligned` so a failing test points at the precondition directly.
- The strong alignment guarantee costs memory when `block_size` is small. On a 64-bit host with `alignof(std::max_align_t) == 16`, a user who asks for `block_size = 8` gets `NULL` and is forced to choose `block_size = 16` — doubling the per-slot footprint. The cost is unavoidable for the "drop-in malloc alignment" property; users with tighter per-slot budgets can use a separate, weaker-alignment allocator (outside the scope of *this* reference). The trade-off is recorded explicitly so M3.2's `TypedPool<T>` can later expose `alignof(T)` as a configurable knob if real consumer pressure demands it.
- The decision to use C++17 `operator new(..., std::align_val_t)` ties the implementation file to C++17 internals. The public C header stays ANSI-C-clean (the M1.10 CI job verifies this on every PR); but consumers who want a *pure C* implementation can no longer use the `.cpp` as-is. The project already commits to C++17 internals per ADR-0005 §3, so this is consistent rather than novel; a pure-C variant would be a separate fork, not a configuration of this one.

## References

- [Specification §4](../specs/01_spec_cpp_memory_pool.md#4-logical-architecture--algorithm-free-list) — the algorithm this ADR turns into a layout.
- [Specification §2.1](../specs/01_spec_cpp_memory_pool.md#2-functional-requirements), [§3.1](../specs/01_spec_cpp_memory_pool.md#3-non-functional-requirements), [§3.2](../specs/01_spec_cpp_memory_pool.md#3-non-functional-requirements) — the requirements this ADR's layout satisfies.
- [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) — the public C contract whose Doxygen comments name this ADR as the resolver of the `block_size` / alignment / `head` semantics.
- [ADR-0005 §3](0005-toolchain-matrix-and-supported-platforms.md) — the C++17 baseline and the ANSI C interop contract that constrain the implementation language choices.
- ISO C++ Standard `[basic.align]`, `[new.delete]` — the over-aligned `operator new(size_t, std::align_val_t)` overload introduced in C++17 (P0035R4).
