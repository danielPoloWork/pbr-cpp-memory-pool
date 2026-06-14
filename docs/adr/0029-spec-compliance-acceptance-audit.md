# ADR-0029: Specification-compliance acceptance audit (v1.0.0 gate)

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) (the contract being accepted against), [ROADMAP](../../ROADMAP.md) §7.6 (the item this ADR records) and its **Spec Coverage Map**, and every ADR cited as evidence below (0005, 0009, 0010, 0012, 0014, 0015, 0016, 0020, 0021, 0022, 0023, 0024).

## Context

Milestone 7.6 is the **acceptance gate before `v1.0.0`**: walk every row of the [Spec Coverage Map](../../ROADMAP.md#spec-coverage-map) and confirm each normative requirement of [the specification](../specs/01_spec_cpp_memory_pool.md) is satisfied by a **passing test**, a **documented ADR**, or both. The Spec Coverage Map already shows all fifteen rows ✅, but those marks accumulated milestone-by-milestone; before committing to the SemVer 1.0 stability promise the project needs **one deliberate, end-to-end pass** that re-verifies the marks against the evidence as it stands today, and records the outcome durably. ADR-0004 §2 makes the closing release (M7.7) the moment the API is frozen — so this audit is the last checkpoint where a gap would still be cheap to fix.

The evidence base as audited: ten CTest targets registered by [`src/test/cpp/it/d4np/memorypool/CMakeLists.txt`](../../src/test/cpp/it/d4np/memorypool/CMakeLists.txt) — `pool_smoke` (41 cases), `typed_pool` (9), `pool_allocator` (7), `free_list_iterator` (6), `container_integration` (6), `concurrency_stress` (4), `dynamic_growth` (8), `instrumented_pool` (10), `zero_overhead` (4), and the `spec_6_2_valgrind` C program — plus the `c_consumer_min.c` ANSI-C consumer and the [`ci.yml`](../../.github/workflows/ci.yml) jobs (`build` matrix incl. ASan/UBSan, `ansi-c-compat` for `-std=c89`/`-std=c99`, `zero-external-deps`, `valgrind`, `thread-safety`, `tsan`, `bench-smoke`, `bench-policy-smoke`).

## Decision

**The implementation satisfies every normative clause of the specification; the project is acceptance-ready for `v1.0.0`.** Each row below was re-verified against live evidence; no row is ✅ without a passing test and/or an Accepted ADR, and **no gap, regression, or unsupported mark was found**. No Spec Coverage Map cell changes state (all fifteen remain ✅) — this ADR is the audit record that backs them.

### Row-by-row acceptance

| Spec clause | Requirement | Evidence (ADR + test / CI) | Verdict |
|-------------|-------------|----------------------------|:------:|
| **§2.1** | Pre-allocate a contiguous pool given `block_size` / `block_count` | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) (layout, `block_size` preconditions, alignment), [ADR-0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) (Pimpl). `pool_smoke`: create round-trip + one case per ADR-0009 §2/§3 precondition violation | ✅ |
| **§2.2** | O(1) allocation; `NULL` (C) **or** `std::bad_alloc` (C++); optional dynamic growth | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §7 (O(1) pop, `NULL` on exhaustion), [ADR-0016](0016-exception-policy-at-the-c-cpp-boundary.md) (dual-verb: `allocate` throws, `try_allocate` returns `nullptr`), [ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md)/[0023](0023-composite-chunk-list-representation.md)/[0024](0024-dynamic-growth-synchronization-and-creation-surface.md) (growth). `pool_smoke` (exhaustion → `NULL`), `typed_pool` (`bad_alloc`), `dynamic_growth` (grows instead of failing) | ✅ |
| **§2.3** | O(1) deallocation; block marked free without returning to the OS | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) (O(1) head push). `pool_smoke` (free + LIFO re-allocation), `free_list_iterator` (a freed block returns to the list head) | ✅ |
| **§2.4** | Optional, macro-configurable thread safety; single-thread fast path preserved | [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) (compile-time Strategy via `PBR_MEMORY_POOL_THREAD_SAFETY`), [ADR-0021](0021-template-method-allocation-skeleton.md). `concurrency_stress` (no over-vend / full recovery / exclusive ownership under MUTEX + LOCKFREE); CI `thread-safety` + `tsan`; M4.5 benchmark shows `NONE` unchanged | ✅ |
| **§3.1** | No leaks — destroy releases all pre-allocated memory to the OS | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §7. CI `valgrind` job gated on `ERROR SUMMARY: 0 errors from 0 contexts`; ASan/UBSan matrix cells; `dynamic_growth` drains/frees/destroys under ASan | ✅ |
| **§3.2** | Minimal per-block metadata overhead | [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (0 bytes/block; ≤ 192 bytes/pool, compile-time `static_assert` + runtime check). `pool_smoke` `metadata_bytes` cases (null / sanity / budget / O(1) in `block_count`) | ✅ |
| **§3.3** | ANSI C (C89) / C++17, no external dependencies | [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) (standards + platforms). CI `ansi-c-compat` (`-std=c89 -pedantic -Werror` and `-std=c99`) compiling `c_consumer_min.c`; CI `zero-external-deps` archive audit | ✅ |
| **§4** | Implicit free list — free blocks store the next-free pointer | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1. `free_list_iterator` walks the implicit list (count / ordering / LIFO / membership); `pool_smoke` distinct-and-aligned guarantee | ✅ |
| **§5 — `memory_pool_create`** | `memory_pool_t* memory_pool_create(size_t, size_t)` | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md)/[0010](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md). `pool_smoke`; `spec_6_2_valgrind/test_pool.c` + `c_consumer_min.c` (C ABI) | ✅ |
| **§5 — `memory_pool_alloc`** | `void* memory_pool_alloc(memory_pool_t*)` | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md). `pool_smoke` (happy path, null pool, exhaustion); `spec_6_2_valgrind` | ✅ |
| **§5 — `memory_pool_free`** | `void memory_pool_free(memory_pool_t*, void*)` | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md). `pool_smoke` (free, null no-ops, foreign-pointer); `spec_6_2_valgrind` | ✅ |
| **§5 — `memory_pool_destroy`** | `void memory_pool_destroy(memory_pool_t*)` | [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §7. `pool_smoke` (`destroy(NULL)` no-op); CI `valgrind` (no leak after destroy) | ✅ |
| **§6.1** | Correctness — exhaustion, null inputs, foreign / out-of-range pointers | [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md). `pool_smoke`: exhaustion, null-pool/null-block no-ops, and the five foreign-pointer scenarios (below-range, above-range, in-range-misaligned, sibling-pool heap, stack) | ✅ |
| **§6.2** | Valgrind clean: `ERROR SUMMARY: 0 errors from 0 contexts` | `spec_6_2_valgrind/test_pool.c` (the literal spec command, also a CTest target); CI `valgrind` job greps the exact success string | ✅ |
| **§6.3** | Benchmark `pool_alloc`/`free` vs `malloc`/`free` over 1,000,000 iterations | [ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md). `pool_vs_malloc_bench`; committed reports [`docs/bench/`](../bench/) (v0.2.0 fixed 4–11×, v0.4.0 threading, v0.5.0 growth ~2×); CI `bench-smoke` + `bench-policy-smoke` | ✅ |

Spec §1 is business context (no testable requirement) and is intentionally out of the map.

## Alternatives Considered

- **No formal acceptance audit — trust the incrementally-accumulated ✅ marks.** Rejected: the marks were set across seven milestones by different PRs; the value of a 1.0 gate is precisely the single deliberate re-verification that nothing silently regressed or was marked ✅ optimistically. The cost (this ADR) is small relative to the stability promise it backs.
- **Record the audit only in the PR body / commit message.** Rejected: the audit outcome is an architectural fact a future reader (or a 2.0 re-baseline) will want to find in the ADR index, not reconstruct from `git log`. ROADMAP §7.6 explicitly asks for an ADR.
- **Add a single automated "spec acceptance" meta-test.** Considered, rejected for now: the evidence already lives in named tests and CI jobs; a meta-test would mostly duplicate `ctest` + the CI gate. The Milestone 8.6 consistency lint is the better long-term home for *mechanically* asserting the Spec Coverage Map has no dangling rows — this ADR is the human acceptance record it would complement.

## Consequences

**Positive**

- The `v1.0.0` API-stability commitment rests on a recorded, row-by-row acceptance rather than on trust in accumulated checkboxes.
- The audit table is a single map from each spec clause to the exact test target / CI job / ADR that satisfies it — useful for onboarding, for a future 2.0 re-baseline, and as the seed for the M8.6 automated congruence check.
- Confirms there is **no outstanding spec gap** blocking M7.7; the release PR can proceed.

**Negative / limitations**

- The audit is a **point-in-time** record (the `v0.6.0`-plus-`Unreleased` state on 2026-06-14). It is not self-updating; a future change that alters behaviour must re-confirm the affected row. M8.6's lint is the intended mechanical backstop against the map drifting from reality.
- Deliberately-deferred items remain deferred and are **out of spec scope** (documented, not gaps): double-free detection, per-thread caches, lock-free dynamic growth, shrink-on-idle. None corresponds to a spec clause, so none affects this acceptance.

**Documentation updates landing in the same PR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0029.
- [ROADMAP](../../ROADMAP.md) §7.6 — checkbox flipped; a one-line audit pointer added under the Spec Coverage Map.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry recording the audit ADR.

## References

- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) — the accepted contract.
- [ROADMAP](../../ROADMAP.md) §7.6 + Spec Coverage Map — the item and the table this ADR backs.
- [`src/test/cpp/it/d4np/memorypool/CMakeLists.txt`](../../src/test/cpp/it/d4np/memorypool/CMakeLists.txt), [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) — the test targets and CI jobs cited as evidence.
