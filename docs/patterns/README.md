# Design Patterns Catalogue

Living index of every design pattern that has been **adopted**, is **planned**, was **considered and rejected**, or is **under evaluation** for `pbr-cpp-memory-pool`. The catalogue is mandatory reading whenever a PR introduces or removes a pattern, and it is updated in the same PR.

- **Policy** — [ADR-0003 — Design Patterns Policy](../adr/0003-design-patterns-policy.md).
- **Rules summary** — [`AGENTS.md`](../../AGENTS.md) §8.
- **Canonical taxonomy** — [`design-patterns.md`](design-patterns.md). The full enterprise pattern list across the eight categories (Creational, Structural, Behavioral, EIP, Architectural, Concurrency, Cloud/Distributed, Data & Persistence). All pattern names used in this catalogue, in ADRs, and in commit messages must match the spelling there.

## How to use this catalogue

- **Adding a pattern.** When a PR adopts a pattern, add a row to *Adopted* (or *Planned* if the implementation is staged across milestones), with the ADR link and the code location.
- **Refining a pattern.** When an existing pattern's implementation shifts substantially, update the row and link the new ADR if one is introduced.
- **Rejecting a pattern.** When a pattern was a credible candidate for a given problem and was ruled out, add it to *Rejected* with the reason. Do not silently drop it — the rejection has didactic value.
- **Removing a pattern.** When a previously adopted pattern is supplanted, move its row to *Superseded*, link the superseding ADR, and keep the historical entry.

Status vocabulary:

| Status      | Meaning                                                                                    |
|-------------|--------------------------------------------------------------------------------------------|
| Planned     | Adoption decided in an ADR; implementation not yet landed.                                 |
| Implemented | Pattern is present in `src/main/cpp/...`; the linked ADR is `Accepted`.                    |
| Considered  | Pattern was evaluated for a specific problem; outcome captured in the row.                 |
| Rejected    | Considered and ruled out — reason captured.                                                |
| Superseded  | Was implemented; replaced by a different approach in a later PR.                           |

## Adopted / Planned

| # | Pattern | Status  | Problem it addresses                                                                                          | Code location                                                                                                                                                                  | ADR / PR |
|---|---------|---------|---------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------|
| 1 | RAII    | Implemented | The C++ wrapper owns the C handle's lifetime: ctor calls `memory_pool_create`, dtor calls `memory_pool_destroy`; copy deleted, move-only. Makes a forgotten `_destroy` impossible from C++. | Declaration: [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp). Bodies (ctor / dtor / move ops / forwarders) in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp); the C functions called by those bodies are real from M2.3 (`create` / `destroy`) onward — `alloc` / `free` complete in M2.4. | [ADR-0010](../adr/0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) |
| 2 | Pimpl   | Implemented | `struct memory_pool`'s layout is opaque to every consumer (C and C++): the public C header sees only a forward declaration, the C++ wrapper holds the resulting `memory_pool_t*` as its single state, the struct is defined exclusively in the implementation file. C-style Pimpl; the C handle *is* the Impl. | Forward declaration: [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h). Full struct definition (the five ADR-0009 §6 fields) lives in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) from M2.3. | [ADR-0010](../adr/0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) |
| 3 | Factory Method | Implemented | `static std::optional<Pool> Pool::make(size_t, size_t)` returns an engaged optional on successful construction or `std::nullopt` on any failure (precondition violation per ADR-0009 §2 / §3 or OOM). Cleaner failure surface than the ctor's silent-empty-state; sets up the hook point for Milestone 4's polymorphic thread-safe / lock-free variants. | Declaration: [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp). Body: [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp). | [ADR-0011](../adr/0011-factory-method-and-builder-for-pool-construction.md) |
| 4 | Builder | Implemented | `class PoolBuilder` with fluent `.with_block_size().with_block_count().build()` returning `std::optional<Pool>` via `Pool::make`. Const-qualified `build()` enables reusing the same configured builder for multiple identically-configured pools. Absorbs future configuration explosion (thread-safety strategy in M4, growth policy in M5) without breaking the `Pool` ctor signature. | Declaration: [`memory_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.hpp). Body: [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp). | [ADR-0011](../adr/0011-factory-method-and-builder-for-pool-construction.md) |
| 5 | Adapter | Implemented | `PoolAllocator<T>` bridges the pool's fixed-block `void*` interface to the variable-size `std::allocator_traits` (Cpp17Allocator) contract every standard container expects. A non-owning back-reference to a `Pool`; single-block requests route to the pool (O(1)), everything else (`n > 1`, oversized / over-aligned `T`) falls back to over-aligned `::operator new`. Deterministic `(n, T)` routing means `deallocate` needs no per-pointer bookkeeping. Propagation traits all `false`; equality by pool identity. | Header-only template: [`pool_allocator.hpp`](../../src/main/cpp/it/d4np/memorypool/pool_allocator.hpp). Relies on the new `memory_pool_block_size` accessor in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) / [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp). | [ADR-0018](../adr/0018-stl-allocator-adapter.md) |
| 6 | Iterator | Implemented | `FreeListIterator` + `FreeListView` — a read-only LegacyForwardIterator walking the implicit free list for diagnostics (count, ordering, membership, integrity). Traversal delegated to three `memory_pool_debug_*` C accessors so the ADR-0009 §1 layout stays behind the Pimpl boundary. The whole surface is gated behind `PBR_MEMORY_POOL_DIAGNOSTICS` — on in debug, compiled out of release builds unless the `PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS` CMake option forces it on. | Header-only: [`free_list_iterator.hpp`](../../src/main/cpp/it/d4np/memorypool/free_list_iterator.hpp). C accessors in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) / [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp). | [ADR-0019](../adr/0019-free-list-diagnostic-iterator.md) |
| 7 | Strategy | Implemented | Pluggable thread-safety policy selecting how `alloc`/`free` synchronize: `SingleThreadedPolicy` (default, no-op — the v0.3.0 fast path), `MutexPolicy` (a `std::mutex`), `LockFreePolicy` (ABA-tagged Treiber-stack CAS). Bound **at compile time** (policy-based, not runtime-virtual) so the single-threaded build pays nothing, selected by `PBR_MEMORY_POOL_THREAD_SAFETY` (CMake option, fixed library-wide). Exactly one policy is compiled and aliased to `ActivePolicy`. Per-thread caches deferred. | All three policies + the macro selector in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp); macro constants in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h); CMake option in [`CMakeLists.txt`](../../CMakeLists.txt). | [ADR-0020](../adr/0020-thread-safety-strategy-and-compile-time-knob.md) |
| 8 | Template Method | Implemented | `alloc_skeleton` / `free_skeleton` define the invariant allocation/deallocation frame — the null-pool / null-block / foreign-pointer guards (ADR-0012, reading only immutable fields) — and delegate the synchronized free-list head mutation to two policy hook points, `Policy::pop_head` / `Policy::push_head`. The exhaustion test lives inside `pop_head` so a future lock-free policy can re-test inside its CAS loop. Compile-time (not runtime-virtual) hooks; hosts the ADR-0020 thread-safety **Strategy**. | Anonymous-namespace templates + `SingleThreadedPolicy` in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp). | [ADR-0021](../adr/0021-template-method-allocation-skeleton.md) |
| 9 | Composite | Implemented | The pool composes its inline first chunk (`struct memory_pool`'s `backing_`/`block_count_`) with a forward-linked list of overflow `Chunk` leaf nodes (`overflow_` head), under one shared implicit free list. Lets `alloc`/`free`-push stay O(1) across any chunk count; only the foreign-pointer check and `destroy` walk the list (O(log N) in dynamic mode). The representation is in place and dormant (`overflow_` null) until dynamic growth populates it in M5.3. | `struct Chunk` + `overflow_` + the chunk-walking `is_block_in_range` / `destroy` / `metadata_bytes` in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp). | [ADR-0023](../adr/0023-composite-chunk-list-representation.md) |
| 10 | Decorator | Implemented | `InstrumentedPool` composes a `Pool` and re-exposes its surface, counting allocations / deallocations / exhaustion failures and tracking the live-block high-water mark (`peak_live_`). Decorator by composition (Pool is concrete + move-only, no virtual interface); opt-in by type, so undecorated pools pay nothing. Relaxed-atomic counters make it safe over any thread-safety policy; a hand-written move keeps it factory-returnable. A size histogram is degenerate for a fixed-block pool, so `peak_live_` is the occupancy signal; event-stream logging is the M6.2 Observer's job. | Header-only: [`instrumented_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp). | [ADR-0025](../adr/0025-decorator-for-instrumented-pool.md) |
| 11 | Observer | Implemented | The GoF runtime Observer: a `PoolObserver` interface (virtual `on_pool_event`) is registered with `InstrumentedPool::add_observer`, which notifies on lifecycle events — `exhausted` (alloc found the pool empty), `grew` (a dynamic pool acquired a chunk, detected via the O(1) `memory_pool_growths` core counter), `destroyed` (dtor). Wired into the M6.1 Decorator's interception points (no separate, non-stackable wrapper); zero cost to plain `Pool`, off the hot path. Contrast with the compile-time **Strategy** — observers are runtime/dynamic. | `PoolEvent` / `PoolObserver` + the subject in [`instrumented_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp); the `memory_pool_growths` counter in [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp). | [ADR-0026](../adr/0026-observer-for-pool-lifecycle-events.md) |

## Rejected

*No rejections have been recorded yet.*

| # | Pattern | Considered for | Rejected because | ADR / PR |
|---|---------|----------------|------------------|----------|
| — | —       | —              | —                | —        |

## Superseded

*No superseded patterns yet.*

| # | Pattern | Superseded by | When | ADR / PR |
|---|---------|---------------|------|----------|
| — | —       | —             | —    | —        |

## Candidate patterns to consider

The taxonomy in [`design-patterns.md`](design-patterns.md) lists every pattern that is in scope for the PBR series as a whole. This section narrows that universe to **patterns plausibly applicable to *this* artifact** — a fixed-block memory-pool library. Each remains a candidate until either adopted or explicitly rejected in a future ADR; the list is **not** a commitment to apply them.

Patterns are grouped by the taxonomy category they belong to in `design-patterns.md`. Categories not listed here (EIP, Architectural application styles, Cloud & Distributed, Data & Persistence) are treated as **out of scope** for this artifact — see *Out-of-scope categories* below.

### Creational

| Pattern             | Possible application                                                                         |
|---------------------|----------------------------------------------------------------------------------------------|
| Object Pool         | The project as a whole *is* this pattern.                                                    |
| Factory Method      | Construct configured pool variants (thread-safe vs. single-threaded, growable vs. fixed). *Initial M2 adoption → Adopted #3 (named-constructor specialisation); polymorphic dispatch arrives with M4 / M5.* |
| Abstract Factory    | Coherent families of variants (e.g. a debug family pairing instrumented pool + tracer).      |
| Builder             | Fluent configuration: `PoolBuilder().with_block_size(64).with_count(1024).build()`. *M2 adoption → Adopted #4.* |
| Prototype           | Clone an existing pool configuration (not its state) to spin sister pools.                   |
| Lazy Initialization | Deferred backing-storage allocation until the first `alloc()`.                               |
| Singleton           | Only if a defensibly-scoped default pool emerges. Default stance: **avoid**.                 |
| Multiton            | Per-block-size registry of named pools. Plausible only if a registry use-case emerges.       |
| Dependency Injection| Inject the pool into consumers via the STL allocator adapter (overlaps with Adapter).        |

### Structural

| Pattern             | Possible application                                                                         |
|---------------------|----------------------------------------------------------------------------------------------|
| Adapter             | STL-compatible allocator over the underlying pool. *M3.3 adoption → Adopted #5 (`PoolAllocator<T>`).* |
| Bridge              | Decouple the public `Pool` abstraction from interchangeable backend implementations.         |
| Composite           | Linked-chunk implementation for the dynamic-growth variant. *M5.2 adoption → Adopted #9 (`Chunk` list + `overflow_`).* |
| Decorator           | Logging / tracing / statistics wrapper around a pool instance. *M6.1 adoption → Adopted #10 (`InstrumentedPool`).* |
| Facade              | High-level convenience entry points over the granular C API.                                 |
| Flyweight           | Already implicit in block reuse; possible explicit role for shared per-pool metadata.        |
| Proxy               | Smart-handle types over raw block pointers for bounds-checked debug builds.                  |
| Private Class Data  | Encapsulate pool internals — overlaps strongly with **Pimpl** (see *Idioms*).                |

### Behavioral

| Pattern               | Possible application                                                                       |
|-----------------------|--------------------------------------------------------------------------------------------|
| Strategy              | Pluggable thread-safety policy (no-lock / mutex / lock-free). *M4.1 adoption → Adopted #7 (compile-time policy); impl M4.2/M4.3.* |
| Template Method       | Allocation algorithm skeleton with hook points for the Strategy. *M4.2 adoption → Adopted #8 (`alloc_skeleton` / `free_skeleton`).* |
| Iterator              | Read-only walk of the free list for diagnostics and tests. *M3.4 adoption → Adopted #6 (`FreeListIterator` / `FreeListView`).* |
| State                 | Explicit pool states: empty / partial / exhausted; transitions drive observer events.      |
| Observer              | Pool-event notifications (exhaustion, growth, destruction). *M6.2 adoption → Adopted #11 (`PoolObserver` / `InstrumentedPool::add_observer`).* |
| Memento               | Snapshot a pool's free-list state for deterministic test scenarios.                        |
| Null Object           | No-op pool for tests that need to exercise the API without allocating.                     |
| Command               | Defer alloc/free operations for batched or replayable execution (niche; debug tooling).    |
| Chain of Responsibility | Fallback chain across multiple pools of different block sizes.                           |
| Specification         | Declarative predicate over candidate blocks for diagnostics queries.                       |

### Concurrency

| Pattern                | Possible application                                                                      |
|------------------------|-------------------------------------------------------------------------------------------|
| Monitor Object         | Default lock-based thread-safe Strategy implementation.                                   |
| Immutable Object       | Configuration objects (block size, count, policy flags) are immutable after construction. |
| Guarded Suspension     | Block-on-exhaustion variant (`alloc_blocking`) if/when a waiting API is added.            |
| Active Object          | Async pool service; unlikely for an allocator but kept as a consideration.                |
| Thread Pool            | Not the pool's concern, but a likely *consumer* — relevant for benchmark design.          |

### C++-idiomatic complements (not in `design-patterns.md` but reflexively expected)

These are not classical GoF patterns but are idiomatic in modern C++ and effectively required:

- **RAII** — every owning C++ type manages its resource lifetime through its destructor.
- **Pimpl** — opaque pointer to hide the C++ class layout for ABI insulation; overlaps with *Private Class Data*.

When a candidate moves to *Planned* or *Implemented*, it gets its own ADR and its row migrates to the *Adopted / Planned* table above.

## Out-of-scope categories

The following categories from [`design-patterns.md`](design-patterns.md) are pre-classified as **not applicable** to this artifact. They are recorded here so the policy of explicit rejection is honoured without filling the *Rejected* table with N/A noise. If a surprising fit for any of these emerges in a future PR, file an ADR and migrate the relevant pattern out of this section.

| Category                                        | Reason                                                                                       |
|-------------------------------------------------|----------------------------------------------------------------------------------------------|
| Enterprise Integration Patterns (EIP)           | This is a library, not a messaging system.                                                   |
| Architectural application styles (MVC/MVP/MVVM, Hexagonal, Clean, DDD, …) | This is a building block, not an application. Consumers may apply these around it. |
| Cloud & Distributed Systems Patterns            | A memory allocator is process-local by definition.                                            |
| Data & Persistence Patterns                     | No persistence layer; pool state is volatile by design.                                       |
