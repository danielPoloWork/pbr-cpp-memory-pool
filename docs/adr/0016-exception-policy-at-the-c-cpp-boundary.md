# ADR-0016: Exception Policy at the C/C++ Boundary

- **Status:** Accepted
- **Date:** 2026-06-12
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 (no exceptions across the C ABI — the rule this ADR builds on), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§3 (the preconditions whose violations collapse to `NULL` at the C boundary), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §2 (the silent-empty-state ctor semantic this ADR amends), [ADR-0011](0011-factory-method-and-builder-for-pool-construction.md) (the non-throwing construction path that remains in force), [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §2.2 (*"restituire `NULL` (o lanciare un'eccezione in C++)"* — the requirement this ADR satisfies), [ROADMAP](../../ROADMAP.md) §3.1 (this ADR's roadmap item).

## Context

Spec §2.2 names two acceptable exhaustion behaviours — return `NULL`, or throw an exception in C++ — and the roadmap phrases the C++ half as *"behind a configurable knob"*. Until this ADR, the project has only implemented the `NULL` half:

- The **C API** returns `NULL` from `memory_pool_create` on any precondition violation or backing-storage OOM, and `NULL` from `memory_pool_alloc` on a null or exhausted pool. `memory_pool_create` already catches `std::bad_alloc` from `::operator new` internally — an exception never crosses the `extern "C"` boundary (ADR-0005 §3).
- The **C++ wrapper** mirrors the C semantics one-to-one: a failed construction leaves the wrapper's handle null (the *silent empty state* of ADR-0010 §2), and `Pool::allocate()` returns `nullptr` on exhaustion. Both [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) Doxygen blocks explicitly defer the throwing behaviour to *"post Milestone 3.1"*.

Milestone 3 makes the deferral untenable for three converging reasons:

1. **The spec clause is unimplemented.** The §2.2 *"o lanciare un'eccezione in C++"* alternative has no code behind it; the Spec Coverage Map row is still ⏳ on the C++-side half.
2. **M3.3's STL allocator Adapter requires a throwing `allocate`.** The C++17 *Cpp17Allocator* requirements ([allocator.requirements]) state that `allocate(n)` *throws* on failure — returning `nullptr` is not an option for an allocator plugged into `std::vector`. Whatever surface the Adapter forwards to must be able to throw `std::bad_alloc`.
3. **M3.2's `TypedPool<T>` is specified with `try_allocate` / `allocate` variants** — the roadmap item presupposes a project-wide convention for which verb throws and which does not. That convention must be fixed once, here, and reused verbatim by every later surface.

The design question has two halves: **where exceptions are permitted** (never in C, where in C++), and **what the "configurable knob" is** (a build-time macro? a template policy? a per-call-site choice?).

## Decision

### 1. The C ABI is exception-free — forever

No exception ever propagates out of an `extern "C"` function. Every C-side failure is reported in-band: `NULL` from `memory_pool_create` and `memory_pool_alloc`, silent no-op from `memory_pool_free` / `memory_pool_destroy` / `memory_pool_metadata_bytes` on degenerate input. This codifies what ADR-0005 §3 already requires and what the implementation already does; it becomes normative API contract here so later milestones (threading, growth) inherit it as a hard rule rather than an implementation accident.

### 2. The C++ side adopts a dual-verb surface — the knob is the call site

The *configurable knob* from spec §2.2 is resolved **per call site, not per build**: both policies are always available, side by side, on every C++ allocation surface in the project:

| Verb            | Signature                            | Failure behaviour                          |
|-----------------|--------------------------------------|--------------------------------------------|
| `allocate()`    | `[[nodiscard]] void* allocate()`     | throws `std::bad_alloc`                    |
| `try_allocate()`| `[[nodiscard]] void* try_allocate() noexcept` | returns `nullptr`                  |

- `Pool::allocate()` **changes behaviour** (pre-1.0 breaking change, recorded in `CHANGELOG.md`): it now throws `std::bad_alloc` on exhaustion — and on a moved-from / empty wrapper, whose null handle is indistinguishable from exhaustion at the C boundary. The name `allocate` is deliberately assigned to the *throwing* verb because that matches `std::allocator::allocate` and the *Cpp17Allocator* requirements M3.3 must satisfy — the Adapter will forward to it unchanged.
- `Pool::try_allocate()` is **new**: a `noexcept` forwarder with the exact semantics `Pool::allocate()` had through v0.2.0. The `try_` prefix follows the established standard-library convention (`try_lock`, `try_emplace`) — *attempt, report failure in-band, never throw*.
- `Pool::deallocate()` remains `noexcept` — deallocation never throws anywhere in the project (it forwards to `memory_pool_free`, whose every failure mode is a silent no-op per ADR-0012).

M3.2's `TypedPool<T>` and M3.4's diagnostic surfaces reuse the same verb pair with the same semantics; no future ADR needs to re-litigate the naming.

### 3. Construction: the ctor throws; `make` / `PoolBuilder` stay non-throwing

`Pool(block_size, block_count)` now **throws `std::bad_alloc`** when `memory_pool_create` returns `NULL`, fulfilling the *"post Milestone 3.1"* promise that has been in the ctor's Doxygen since ADR-0010. This **amends ADR-0010 §2**: the silent-empty-state semantic is retired for the public ctor (it survives only as the moved-from state, which move semantics require regardless). An RAII type whose constructor can complete while owning nothing is a known wart — every method call on such an object is a latent surprise; after this ADR a successfully constructed `Pool` is always usable.

The **non-throwing construction path remains** `Pool::make` (engaged `std::optional<Pool>` or `std::nullopt`, ADR-0011 §1) and `PoolBuilder::build()` (delegates to `make`). Internally, `make` no longer routes through the throwing ctor: a new **private adopt-handle ctor** `Pool(memory_pool_t*) noexcept` lets `make` call `memory_pool_create` directly and wrap the result, so the non-throwing path contains no `try` / `catch`.

Construction-failure causes are **deliberately collapsed**: a precondition violation (ADR-0009 §2/§3) and a genuine OOM both surface as `std::bad_alloc` from the ctor, because the C boundary reports both as `NULL` and the wrapper cannot tell them apart. Callers who need to discriminate validate their inputs (the preconditions are public, documented constants) or use `Pool::make`.

### 4. Bench fairness: the benchmark times `try_allocate`

[`pool_vs_malloc_bench.cpp`](../../src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench.cpp) switches its timed loops from `allocate()` to `try_allocate()`. This is the apples-to-apples comparison — `malloc` reports failure in-band with `NULL`, so the pool side must too — and it is byte-for-byte the code path that produced the committed v0.2.0 numbers ([`docs/bench/v0.2.0-windows-msvc-x64.md`](../bench/v0.2.0-windows-msvc-x64.md)), which therefore remain valid.

## Alternatives Considered

- **Global compile-time macro flipping `allocate()` between throwing and returning `nullptr`.** Rejected. A macro forks the *semantics* of the same expression across builds: the same line of consumer code means two different things depending on a `-D` flag, every consumer binary must agree on the flag (ODR hazard for mixed static-library consumers), and the test matrix doubles for every cell. The spec's literal *"configurabile tramite macro di compilazione"* phrase belongs to §2.4 (thread safety, where the fork is structural and M4 will use exactly that mechanism) — §2.2 only requires that both behaviours exist.
- **Policy template parameter — `Pool<ThrowingPolicy>` / `Pool<NullPolicy>`.** Rejected. The policy infects every type signature that mentions the pool (function parameters, member declarations, the future `TypedPool<T, Policy>` would need two parameters), bifurcates the ABI of the wrapper, and buys nothing the two-verb surface doesn't already provide at zero structural cost.
- **Keep `allocate()` returning `nullptr`; add a separate `allocate_or_throw()`.** Rejected. It inverts the standard-library convention — `std::allocator::allocate`, `operator new`, every *Cpp17Allocator* model throws by default, and M3.3's Adapter would forward to the awkwardly-named verb forever. The `try_` prefix is the idiomatic marker for the non-throwing variant, not the other way around.
- **`std::invalid_argument` from the ctor for precondition violations, `std::bad_alloc` for OOM.** Rejected. Discriminating requires re-validating the ADR-0009 §2/§3 preconditions inside the wrapper — a second copy of the validation logic that *will* drift from the C-side copy (the single-source-of-truth argument from ADR-0009 §7). The collapse to `std::bad_alloc` is honest about what the C boundary actually reports.
- **`std::expected<Pool, Error>`-style result type.** Rejected. `std::expected` is C++23; the project contract is C++17 (spec §3.3). Hand-rolling an `expected` clone is exactly the kind of external-dependency-shaped wheel the zero-dependency rule exists to discourage. `std::optional<Pool>` (already shipped, ADR-0011) covers the non-throwing construction use case.
- **Runtime per-pool flag (`set_throwing(bool)`).** Rejected. Adds a branch on state to the allocation hot path, makes the failure behaviour of a given call site impossible to determine by local reading, and the flag itself is metadata against the ADR-0015 budget for zero benefit over the two-verb surface.

## Consequences

**Positive**

- Spec §2.2's C++-side clause is implemented and testable; the Spec Coverage Map row moves to 🚧 (the remaining half of that row is M5 dynamic growth).
- M3.2 (`TypedPool<T>`) and M3.3 (STL allocator Adapter) inherit a fixed, documented verb convention — the Adapter's `allocate(n)` can forward to the throwing verb and satisfy [allocator.requirements] without translation glue.
- A successfully constructed `Pool` is always usable — the silent-empty-state foot-gun (construct, then every `allocate` mysteriously fails) is retired. The compile-visible alternatives (`make`, `PoolBuilder`) serve callers who want failure as a value.
- Both policies coexist in one binary at zero cost: no macro fork, no doubled CI matrix, no ODR hazard.

**Negative**

- **Pre-1.0 breaking change**: code written against v0.2.0 that relied on `Pool::allocate()` returning `nullptr` on exhaustion now gets a throw; code that relied on the ctor's silent empty state now gets `std::bad_alloc`. Mechanical migration: `allocate()` → `try_allocate()`, ctor → `Pool::make`. Recorded under *Changed* in `CHANGELOG.md` per ADR-0004.
- A moved-from `Pool`'s `allocate()` throws `std::bad_alloc` even though the proximate cause is a null handle, not memory pressure. Documented in the Doxygen; the alternative (a dedicated exception type) fails the same single-source-of-truth test as the `std::invalid_argument` split.
- The wrapper's `allocate()` adds one branch (null-check + throw) over the raw C call. Allocation-throughput-sensitive callers use `try_allocate()`, which compiles to exactly the v0.2.0 forwarder.

**Required documentation updates landing in the same PR as this ADR**

- [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) — Doxygen drops every *"deferred to Milestone 3.1"* qualifier; ctor `@throw` becomes unconditional; `try_allocate` documented; class-level exception-policy paragraph points here.
- [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) — Status line annotated *"ctor failure semantics amended by ADR-0016"* (the record itself stays immutable).
- [`docs/adr/README.md`](README.md) — index row for ADR-0016.
- [`ROADMAP.md`](../../ROADMAP.md) §3.1 → `[x]` with the inline summary; Spec Coverage Map §2.2 ⏳ → 🚧.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M3.1)* + *Changed (M3.1)* entries including the breaking-change call-out.

[`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp), [`pool_smoke_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp), and [`pool_vs_malloc_bench.cpp`](../../src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench.cpp) gain the new bodies, the new / updated `TEST_CASE`s, and the `try_allocate` switch respectively.

## References

- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §2.2 — the requirement (*"restituire `NULL` (o lanciare un'eccezione in C++)"*).
- ISO C++17 [allocator.requirements] — `allocate` throws on failure; the constraint that fixes the verb naming.
- [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 — no exceptions across the C ABI.
- [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §2 — the silent-empty-state semantic amended here.
- [ADR-0011](0011-factory-method-and-builder-for-pool-construction.md) — the non-throwing construction path that remains in force.
- `std::mutex::try_lock`, `std::map::try_emplace` — the standard-library `try_` naming precedent.
