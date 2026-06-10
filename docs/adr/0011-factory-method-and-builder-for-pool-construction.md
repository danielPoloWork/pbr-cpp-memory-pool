# ADR-0011: Factory Method and Builder for `Pool` Construction

- **Status:** Accepted
- **Date:** 2026-06-11
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0003](0003-design-patterns-policy.md) (design-patterns policy that requires every adoption to be justified in an ADR), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §7 (the failure semantics this ADR re-exposes through a cleaner C++ surface), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §2 (the "deliberately small surface" stance this ADR extends), [`docs/patterns/README.md`](../patterns/README.md), [ROADMAP](../../ROADMAP.md) §2.6, [ROADMAP](../../ROADMAP.md) §3.1 (the exception-policy decision this ADR deliberately does *not* pre-empt).

## Context

The `Pool` wrapper introduced by [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) exposes a two-parameter constructor `Pool(std::size_t block_size, std::size_t block_count)` that:

- on success, holds a non-null `memory_pool_t*` produced by `memory_pool_create`;
- on **failure** (precondition violation per [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§3 or OOM caught at the C ABI boundary), silently leaves `handle_ == nullptr` so the destructor is a safe no-op and `allocate()` returns `nullptr`.

The silent-empty-state pattern is deliberate — [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §Decision item 2 commits to it explicitly, and [ROADMAP](../../ROADMAP.md) §3.1 reserves the eventual `NULL` → `std::bad_alloc` translation for its own dedicated ADR. But the pattern has two ergonomic costs that surface as the project grows:

1. **Failure detection is implicit.** A caller writing `Pool p(64, 1024);` cannot tell at the call site whether construction succeeded; they must reach into `native_handle()` or notice that the first `allocate()` returns `nullptr`. There is no idiomatic C++17 yes/no signal at the construction expression.
2. **Construction parameters will grow.** Today the ctor takes two `size_t` values. Milestone 4 introduces a thread-safety strategy parameter (mutex, lock-free, per-thread cache — ADR-0011's sibling family pending); Milestone 5 introduces a dynamic-growth policy; later milestones may add alignment override, allocator delegate, debug instrumentation. Adding parameters to the ctor is a series of breaking changes; adding *optional* parameters with defaults turns the ctor signature into a maintenance hazard and forces callers to remember positional argument order.

The Creational section of [`docs/patterns/design-patterns.md`](../patterns/design-patterns.md) lists three patterns that target exactly these two concerns:

- **Factory Method** — replace direct construction with a static factory function whose return type can encode failure (e.g., `std::optional<T>`) and whose body is the natural extension point for polymorphic / variant construction once variants appear.
- **Builder** — separate the configuration of a complex object from its construction; the Builder accumulates parameters via fluent `.with_X(value)` calls and produces the constructed object on a final `.build()`.
- **Named Constructor** (idiomatic complement, not in the GoF taxonomy but listed in `design-patterns.md` under C++ idioms) — give the static factory a descriptive name. For M2 we adopt only one make-style factory so the distinction collapses; "Factory Method" remains the name in the catalogue because Milestone 4's polymorphic variants will turn it into a genuine Factory Method.

The two are **co-introduced and interdependent** here: Builder's `build()` is the natural caller of Factory Method's `make`, and adopting one without the other would leave the wrapper either ergonomically deficient (Factory only, no fluent config) or structurally awkward (Builder calling the ctor directly, missing the failure-signal value-add of `std::optional<Pool>`). Bundling both into a single ADR is the [`AGENTS.md`](../../AGENTS.md) §8 #5 allowance for co-introduced patterns.

## Decision

### 1. Factory Method — `static std::optional<Pool> Pool::make(...)`

Add a `static` member function on `Pool`:

```cpp
static std::optional<Pool> make(std::size_t block_size, std::size_t block_count);
```

The body constructs a `Pool` from the same two parameters and inspects `handle_`:

- if `handle_ != nullptr`, returns `std::optional<Pool>` engaged with the constructed instance (moved into the optional);
- if `handle_ == nullptr` (any precondition violation from [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§3 or OOM), returns `std::nullopt`.

The factory's value-add is the **explicit yes/no signal at the construction expression**:

```cpp
if (auto pool = Pool::make(64, 1024); pool) {
    void* block = pool->allocate();
    // ...
}
```

Compared to the ctor:

```cpp
Pool pool(64, 1024);
if (pool.native_handle() != nullptr) {  // implicit, requires a wrapper-internal check
    // ...
}
```

The ctor is **kept** — both construction paths coexist. The ctor is the lower-level primitive; the factory is the recommended path for callers that want a clear failure signal. ADR-0010's silent-empty-state semantic for the ctor stays unchanged.

The function is marked `[[nodiscard]]` so callers cannot accidentally drop the returned optional.

### 2. Builder — `class PoolBuilder` with fluent `.with_X().build()`

A separate class `PoolBuilder` in the `it::d4np::memorypool` namespace:

```cpp
class PoolBuilder {
public:
    PoolBuilder& with_block_size(std::size_t block_size) noexcept;
    PoolBuilder& with_block_count(std::size_t block_count) noexcept;
    [[nodiscard]] std::optional<Pool> build() const;
private:
    std::size_t block_size_ = 0;
    std::size_t block_count_ = 0;
};
```

Semantics:

- `with_block_size` and `with_block_count` are setters that mutate the builder and return `*this` by reference, enabling the fluent chain. They are `noexcept` — they only assign two `size_t` fields.
- `build()` is `const` and `[[nodiscard]]`. It calls `Pool::make(block_size_, block_count_)` and returns the resulting `std::optional<Pool>`. Calling `build()` does **not** mutate the builder, so the same builder can produce multiple identically-configured pools.
- Default-constructed builder has `block_size_ = 0` and `block_count_ = 0`, both of which violate [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2/§3 — `.build()` on an unconfigured builder returns `std::nullopt`. This is the deliberate fail-loud behaviour for forgotten configuration.

Builder is a separate class (not a nested `Pool::Builder`) so the type name is visible in the namespace alongside `Pool` and `TypedPool<T>` (the latter arrives in Milestone 3). Nested-builder is also a common pattern; the choice between the two is stylistic. We pick separate-class because:

- the builder doesn't access any private members of `Pool` — it could just as well live in a sibling header in a more granular layout;
- a separate class is easier to forward-declare from consumer code that doesn't care about the wrapper itself;
- the symmetry with future `ThreadSafePoolBuilder` / `GrowablePoolBuilder` (Milestones 4 / 5) is cleaner with sibling classes than with nested ones inside variant types.

### 3. Where the failure semantics live

| Surface                              | Behaviour on failure                                       |
|--------------------------------------|------------------------------------------------------------|
| `memory_pool_create` (C, ADR-0009 §7) | returns `NULL` — never throws across the C ABI            |
| `Pool` ctor (ADR-0010 §2)            | silent empty state (`handle_ == nullptr`)                  |
| `Pool::make` (this ADR)              | returns `std::nullopt` — explicit at the call site         |
| `PoolBuilder::build` (this ADR)      | returns `std::nullopt` (delegates to `Pool::make`)         |
| `Pool` ctor under M3.1 (pending)     | TBD — may throw `std::bad_alloc` behind a compile-time knob; this ADR does **not** pre-empt that decision |

The four surfaces compose: a Builder chains through Factory Method, which constructs a ctor-constructed `Pool` and inspects its handle. There is exactly one truth — the C handle's nullness — propagating through the chain with different idioms at each layer.

### 4. Patterns catalogue migration

In `docs/patterns/README.md`:

- **Factory Method** moves from the Creational *candidate* list to the *Adopted / Planned* table, status `Implemented`, code location `memory_pool.hpp` / `memory_pool.cpp`, ADR link to this document.
- **Builder** moves the same way.
- The "candidate" rows for both stay (they describe the broader use case — future polymorphic variants in M4 / M5 — that the present implementation prepares for); they are annotated to point at the now-adopted row.

## Alternatives Considered

- **Factory Method returning `Pool` directly (named constructor with the same silent-empty-state).** Rejected. The whole point of adding a Factory Method on top of the existing ctor is to *change* the failure surface — same return type as the ctor means the factory is a tautology. The `std::optional<Pool>` return is the value-add that justifies the pattern adoption.
- **Throw `std::bad_alloc` from the factory on failure.** Rejected for M2. This conflates the Factory Method adoption with the exception-policy decision that [ROADMAP](../../ROADMAP.md) §3.1 reserves for its own ADR. The `std::optional<Pool>` path is no-throw and orthogonal — M3.1's eventual exception-policy ADR can adopt throwing, keep no-throw, or expose both behind a configurable knob without amending this ADR.
- **`std::variant<Pool, std::error_code>` instead of `std::optional<Pool>`.** Considered. The variant approach carries an error category for the failure path, useful when callers want to distinguish "precondition violation" from "OOM" from "exhausted" etc. Rejected for M2 because (a) the C API does not currently distinguish these — `memory_pool_create` returns `NULL` for every failure mode — and (b) the `std::optional` API is more idiomatic C++17 for binary success/failure. If a future need to distinguish failure modes emerges (e.g., a diagnostic facade in M6), a parallel `Pool::make_with_diagnostic` returning `std::variant` can be added without amending this ADR.
- **Skip Factory Method; only adopt Builder.** Rejected. Builder needs *something* to construct from; without a factory, `build()` would either call the ctor directly (no failure-signal improvement) or duplicate the optional-wrapping logic. Factoring the construction-with-explicit-failure into `Pool::make` keeps Builder a thin orchestration layer over a single piece of failure logic.
- **Skip Builder; only adopt Factory Method.** Rejected. Without Builder, the fluent configuration pattern that the [ROADMAP](../../ROADMAP.md) §2.6 item explicitly calls out (and that the candidate list at [`docs/patterns/README.md`](../patterns/README.md) §Creational documents) would have no place to land. The two parameters today are easy to remember; the four / five / six parameters of M4 / M5 / M6 will not be.
- **Nest `Builder` inside `Pool` as `Pool::Builder`.** Rejected (mildly). Both placements compile to identical code; the choice is stylistic. We pick the sibling-class form because future builder variants for thread-safe / growable pools (M4 / M5) will share the namespace and look more uniform when they're all sibling classes than when half are nested.
- **Mark the builder rvalue-only (`build() &&`).** Considered for forcing one-shot use. Rejected because the temporary-throwaway use case (`PoolBuilder().with_X().build()`) works equally well with the const-qualified `build()` we adopted, and the const form additionally enables the "configure once, build several" pattern — useful for tests and for benchmark setup where the same configuration is exercised under different runtime conditions.

## Consequences

**Positive**

- Callers gain an explicit yes/no signal at the construction expression. The optional-based path eliminates the `if (pool.native_handle() != nullptr)` boilerplate at every construction site, which currently every test case (and every future consumer) has to write.
- The wrapper's surface acquires a hook point for [ROADMAP](../../ROADMAP.md) §4 (thread-safety strategy) and §5 (dynamic growth). Once those variants exist, `Pool::make` becomes the dispatch site that chooses the runtime variant from the configuration — Factory Method in its full polymorphic sense rather than the named-constructor specialisation it inhabits today.
- The Builder absorbs the future configuration-explosion pressure without ABI churn. Adding a `with_thread_safety(Strategy)` setter in M4 is non-breaking: existing callers that don't call it get the default; new callers opt into the knob with one fluent step.
- The patterns catalogue grows by two adoptions both with concrete code locations. The didactic value of the project's "Reference Implementation" framing is exercised — Factory Method + Builder are paradigmatic textbook patterns whose interaction the catalogue can now demonstrate alongside the RAII + Pimpl pair from ADR-0010.

**Negative**

- Two construction paths now coexist (`Pool(...)` and `Pool::make(...)`). The Doxygen on the ctor will explicitly point at `Pool::make` as the recommended path so the choice is not silently confusing; the catalogue's Factory Method row records the rationale. The cost is documentation-load on the wrapper's class block, not API confusion at the call site.
- `std::optional<Pool>` requires `Pool` to be move-constructible. It already is (ADR-0010 §2 commits to move-only with deleted copy); the optional inherits the same semantics. Strictly: `std::optional<MoveOnly>` is itself move-only, which propagates to `PoolBuilder::build()`'s return — callers cannot copy the result. This is the only sensible behaviour for an owning allocator wrapper; no special handling required.
- The Builder's `build() const` style allows multiple invocations on the same configuration, which is *intentional* but creates an opportunity for callers to share a builder across threads — a no-op in itself but worth recording for the M4 thread-safety discussion. The builder fields are `noexcept` writes of plain `size_t`, so concurrent setter calls are racy but not catastrophic; future ADRs can revisit if real-world usage surfaces a need for thread-safe builders.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/patterns/README.md`](../patterns/README.md) — two new rows in *Adopted / Planned* (Factory Method, Builder), both status `Implemented`. The corresponding Creational *candidate* rows are annotated to point at the adopted rows so the trail stays discoverable.
- [`docs/adr/README.md`](README.md) — index row for ADR-0011.
- [`ROADMAP.md`](../../ROADMAP.md) §2.6 — checkbox flipped with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entries for `Pool::make` and `PoolBuilder` referencing this ADR.

[`src/main/cpp/it/d4np/memorypool/memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp) and [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) gain the declarations and bodies. [`src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp) gains TEST_CASEs covering the happy path of each pattern, the failure cases (`Pool::make` returning `nullopt`, `PoolBuilder::build` on default-constructed and partially-configured states), and the multi-build property of the const `build()`.

## References

- [ADR-0003](0003-design-patterns-policy.md) — design-patterns policy requiring every adoption to be justified.
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2, §3, §7 — the preconditions and `NULL`-on-failure contract that the optional-returning factory reflects up into the C++ surface.
- [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §2 — the "deliberately small surface" decision that this ADR extends with two complementary creational hooks.
- [`docs/patterns/design-patterns.md`](../patterns/design-patterns.md) — canonical taxonomy; *Factory Method* and *Builder* sit in the Creational category.
- [`docs/patterns/README.md`](../patterns/README.md) — project-scoped catalogue; the *Creational* candidate list for this project explicitly anticipated both patterns.
- [`AGENTS.md`](../../AGENTS.md) §8 #5 — the policy clause allowing two co-introduced patterns to share a single ADR.
- C++17 `[optional.optional]` — the standard's `std::optional` semantics, in particular move-construction and the `nullopt`-as-empty contract.
