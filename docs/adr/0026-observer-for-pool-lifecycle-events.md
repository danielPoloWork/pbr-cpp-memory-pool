# ADR-0026: Observer for Pool-Lifecycle Events

- **Status:** Accepted
- **Date:** 2026-06-13
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0025](0025-decorator-for-instrumented-pool.md) (the `InstrumentedPool` Decorator this extends, and which already intercepts every `alloc`/`free`), [ADR-0024](0024-dynamic-growth-synchronization-and-creation-surface.md) §1 (the `grow_pool` slow path the growth counter hooks), [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) (the compile-time Strategy, contrasted here with the runtime Observer), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the introspection-accessor pattern `memory_pool_growths` follows; the 192-byte budget the new field fits), [ROADMAP](../../ROADMAP.md) §6.2 (this item) ← §6.1 (ADR-0025) → §6.3 (the zero-overhead-when-disabled verification).

## Context

Milestone 6.2 adds the **Observer** pattern: notify registered observers of pool-lifecycle events — **exhaustion**, **growth**, and **destruction**. The constraints from Milestone 6's goal (and the project's architecture) are firm: the C core's hot path must stay zero-overhead, the C ABI stays exception-free, and a plain `Pool` must pay nothing.

Two questions:

1. **Where does the Observer live**, and how are the three events detected without touching the C hot path?
2. **How is "growth" observed** at all — it happens *inside* the C `pop_head`, invisible above the C boundary?

## Decision

### 1. The Observer is wired into `InstrumentedPool`, not a separate wrapper

`InstrumentedPool` (the M6.1 Decorator) already intercepts every allocation verb and owns the wrapper-level view of the pool. Making it the Observer **Subject** — adding `add_observer(PoolObserver&)` and notifying at its existing interception points — reuses that machinery and avoids a separate `ObservablePool` that could not *stack* with `InstrumentedPool` (each would own the move-only `Pool`). So the Decorator (stats) and the Observer (events) compose in one cohesive observability type. A program that wants neither uses plain `Pool` and pays nothing.

The Observer is the **classic GoF runtime-polymorphic** form — a `PoolObserver` abstract base with `virtual void on_pool_event(PoolEvent, const PoolStats&)`, and observers registered dynamically. This is deliberately *unlike* the compile-time Strategy (ADR-0020): observers are user-defined and attached at run time, the notifications fire on rare events (not the hot path), so virtual dispatch is the right, idiomatic tool here.

```cpp
enum class PoolEvent { exhausted, grew, destroyed };
struct PoolObserver { virtual void on_pool_event(PoolEvent, const PoolStats&) = 0; /* + rule-of-five */ };
```

### 2. Event detection

- **Exhaustion** — `try_allocate()` returning `nullptr` or `allocate()` throwing `std::bad_alloc` (for a fixed pool, or a dynamic pool whose growth allocation failed). Detected at the wrapper; notified after counting the failure.
- **Destruction** — `InstrumentedPool`'s destructor (now user-provided) notifies `destroyed` once. A *moved-from* instance notifies nobody: the move transfers the observer list (leaving the source's list empty), so the moved-from destructor's notify is a no-op — no spurious event.
- **Growth** — detected via a new **O(1)** core counter (next decision), compared after each successful allocation; a rise means the dynamic pool grew, so `grew` is notified.

### 3. A core `memory_pool_growths` counter makes growth observable in O(1)

Growth happens inside the C `pop_head` (`grow_pool`, ADR-0024 §1) and is invisible above the C boundary. Detecting it by diffing `metadata_bytes` per allocation would be **O(chunks) per alloc** — unacceptable. Instead the core gains a single counter:

- `struct memory_pool` gains `std::atomic<std::size_t> grow_count_`, incremented (relaxed) inside `grow_pool` — i.e. on the **rare growth slow path only**, never on the hot pop. The atomic keeps the wrapper's read data-race-free when observing a thread-safe pool.
- a new public accessor `size_t memory_pool_growths(const memory_pool_t*)` returns it in **O(1)** — `NULL`-tolerant, ANSI C C89-compatible, **always present** (not diagnostics-gated, since the Observer is usable in any build), the same shape as `memory_pool_metadata_bytes` / `memory_pool_block_size`.

`InstrumentedPool` reads `memory_pool_growths` after each successful allocation (an O(1) atomic load) and notifies `grew` when it rises. The hot path of a plain `Pool` is untouched — only `grow_pool` writes the counter, only the opt-in wrapper reads it. The field adds 8 bytes (NONE struct → 64, MUTEX → 144), comfortably inside the ADR-0015 192-byte budget.

### 4. Zero overhead and the concurrency caveat

Plain `Pool` is unchanged (no observer, no counter read, no virtual call). When an `InstrumentedPool` has *no* observers, `notify` is an empty-vector check; the per-allocation growth read is a single relaxed atomic load on the opt-in path. Observer **registration and notification are not internally synchronized** — observers are notified on the calling thread, and concurrent registration or notification needs external synchronization (the standard Observer caveat); register observers before concurrent use and make them thread-safe. The growth *counter* itself is atomic, so observing a thread-safe pool is data-race-free; the wrapper's `last_growths_` watermark is an approximate, eventually-consistent trigger under contention.

## Alternatives Considered

- **A separate `ObservablePool` wrapper.** Rejected — it could not stack with `InstrumentedPool` (both own the move-only `Pool`), and it would duplicate the alloc/free interception points the Decorator already has. Folding Observer into `InstrumentedPool` keeps one observability type.
- **Notify directly from the C core (a C callback in `pop_head` / `grow_pool` / `destroy`).** Rejected — it forces a C++ callback across the exception-free C ABI and taxes the hot path with a per-op callback check, violating the §6 zero-overhead goal. Observation belongs above the C boundary.
- **Detect growth by diffing `metadata_bytes` per allocation.** Rejected — O(chunks) per alloc. The O(1) `grow_count_` counter + accessor is the fix (and the counter sits on the slow path).
- **`std::function` observers instead of an interface.** Rejected — the GoF interface (`PoolObserver`) gives clear observer lifetime/ownership (the wrapper stores non-owning pointers), avoids per-observer heap allocation, and is the textbook Observer; a `std::function` list adds allocation and obscures lifetime.
- **Compile-time (policy) Observer.** Rejected — observers are inherently runtime/dynamic, and the events are off the hot path, so runtime virtual dispatch is correct here (unlike the on-hot-path thread-safety Strategy).
- **A `growing` event fired from inside the lock-free path.** N/A — `LOCKFREE` pools never grow (ADR-0024 §2), so `grow_count_` stays 0 and `grew` never fires there; consistent and needs no special case.

## Consequences

**Positive**

- Lifecycle observability (exhaustion / growth / destruction) with zero cost to plain `Pool` and near-zero cost to an observer-less `InstrumentedPool`.
- The growth counter is a small, reusable O(1) introspection accessor (`memory_pool_growths`) on the slow path; the hot path is untouched.
- Decorator (stats) and Observer (events) compose in one type — usable together, no stacking problem.
- Runtime virtual Observer cleanly contrasts the compile-time Strategy, rounding out the pattern catalogue.

**Negative**

- Observer notification is not internally synchronized — concurrent observable use needs external synchronization / thread-safe observers (documented). The `last_growths_` watermark is approximate under contention.
- `struct memory_pool` grows by 8 bytes (the atomic counter), now NONE 64 / LOCKFREE 80 / MUTEX 144 — still within the 192 budget, but the budget continues to tighten.
- Mixing two patterns (Decorator + Observer) in `InstrumentedPool` trades single-pattern-per-type purity for a cohesive, stackable observability surface — a deliberate call recorded here.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0026.
- [`docs/patterns/README.md`](../patterns/README.md) — **Observer** moves to *Adopted* (status `Implemented`).
- [`ROADMAP.md`](../../ROADMAP.md) §6.2 → `[x]` with the inline summary.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — *Added (M6.2)* entry.

[`instrumented_pool.hpp`](../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp) gains `PoolEvent`, `PoolObserver`, and the `InstrumentedPool` observer surface; [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h) / [`memory_pool.cpp`](../../src/main/cpp/it/d4np/memorypool/memory_pool.cpp) gain `grow_count_` + `memory_pool_growths`; [`c_consumer_min.c`](../../src/test/c/it/d4np/memorypool/c_consumer_min.c) exercises the accessor; [`instrumented_pool_test.cpp`](../../src/test/cpp/it/d4np/memorypool/instrumented_pool_test.cpp) gains the observer cases. The zero-overhead verification is M6.3.

## References

- E. Gamma et al., *Design Patterns* — Observer (define a one-to-many dependency so dependents are notified of state changes); here the runtime, virtual-dispatch form.
- [ADR-0025](0025-decorator-for-instrumented-pool.md) — the `InstrumentedPool` Decorator the Observer extends.
- [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) — the `memory_pool_*` O(1) introspection-accessor precedent.
