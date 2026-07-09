# ADR-0044: A coverage-guided fuzzing harness for the pool surface

- **Status:** Accepted
- **Date:** 2026-07-09
- **Deciders:** Daniel Polo (maintainer / project architect)
- **Related:** [ADR-0007](0007-test-framework-doctest.md) (the doctest/CTest test tier this complements), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) (the free-list invariants the harness asserts), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the foreign / NULL / out-of-range free semantics the harness exercises), [ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) / [ADR-0024](0024-dynamic-growth-synchronization-and-creation-surface.md) (the dynamic-growth path fuzzed alongside the fixed one), [ADR-0025](0025-decorator-for-instrumented-pool.md) (the `InstrumentedPool` used as the accounting oracle), [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) (the sanitizer / platform split this inherits), [ADR-0039](0039-bug-ledger-and-triage-protocol.md) (where a fuzzer-found defect is filed), [ADR-0037](0037-new-feature-roadmap-placement.md) (roadmap Milestone 9), [ADR-0004](0004-versioning-and-release-policy.md) (the SemVer classification), issue #108, origin issue #105, spec [§6.4](../specs/01_spec_cpp_memory_pool.md#64-sanitizers--ci)

## Context

The test tier is example-based (doctest, [ADR-0007](0007-test-framework-doctest.md)) and the benchmark is a fixed loop ([ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md)); there was **no fuzz target anywhere in the repo**. An allocator is exactly the component where fuzzing earns its keep: the interesting defects live in *sequences* — allocate-to-exhaustion, LIFO vs. FIFO frees, free-then-reallocate across a growth event, foreign-pointer rejections interleaved with valid traffic — not in any single call. The in-repo bug ledger ([ADR-0039](0039-bug-ledger-and-triage-protocol.md)) already shows this defect class matters. The spec review (#105) flagged the gap directly.

Two forces shape the design:

1. **It must run everywhere the project is developed, and gate on every PR.** libFuzzer is a Clang runtime and unavailable on MSVC ([ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3) — the same platform split the ASan/UBSan presets already live with. But the seed corpus should still be a regression gate on *every* platform, including the maintainer's MSVC box, so a check-in that breaks the harness or a known-good input is caught locally, not only on the Clang CI leg.
2. **It must never perturb the release build or the [ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md) benchmark numbers.** The coverage-guided engine and its instrumentation are strictly opt-in.

## Decision

Add a single harness translation unit, [`pool_fuzz.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_fuzz.cpp), that is **engine-agnostic**: it exposes the libFuzzer entry point `LLVMFuzzerTestOneInput` *and* a standalone replay `main`. The build decides which role it plays.

### The harness — a stateful opcode interpreter with a shadow oracle

`LLVMFuzzerTestOneInput` treats the input buffer as a tiny program. The leading three bytes configure a pool — `block_size = alignof(max_align_t) * (1 + (b & 3))` (valid by construction under [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2), a small `block_count`, and a fixed-or-dynamic mode with a growth factor — and each subsequent byte is an opcode over a state machine: `alloc`, `free(valid)` (an index byte picks which live block, so LIFO / FIFO / interleaved orders all arise), `free(NULL)`, and `free(foreign)`. A dynamic pool grows implicitly as allocation runs past its initial capacity ([ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md)).

The harness keeps its own **shadow model** of the live blocks and asserts, on every step, the invariants the pool promises — turning a silent corruption into an immediate crash the fuzzer can minimise and save as a reproducer:

- **No aliasing** — a freshly vended block is never already live (a free list that vends a block twice is caught here).
- **Canary integrity** — each block is stamped with a per-block byte on allocation and verified intact at free time, so any overlap or stray write through a live block is detected even without a sanitizer.
- **Defined foreign/NULL frees** — `free(NULL)` and `free(&stack_object)` are no-ops that never corrupt the free list ([ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md)); the foreign free is issued through the C core so it does not perturb the accounting oracle.
- **Accounting** — an [`InstrumentedPool`](../../src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp) ([ADR-0025](0025-decorator-for-instrumented-pool.md)) is the oracle: its `live_` count tracks the shadow set exactly after every operation, and after the final drain the pool reports zero live, balanced allocation/deallocation counts, and a peak-live matching the observed high-water mark.

A **double-free of an in-range block is deliberately *not* exercised.** The default build does not detect it — an accepted, documented trade-off ([ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md), spec §5.3) — so feeding one to this ASan/UBSan build would flag a documented decision as a bug. Double-free detection is instead proven by the opt-in hardening suite ([ADR-0043](0043-opt-in-debug-hardening.md), `pool_hardening_test`).

### Two build roles from one source

- **libFuzzer target `pool_fuzz`** — opt-in behind the CMake option `PBR_MEMORY_POOL_BUILD_FUZZERS` (**OFF** by default) and **Clang-only** (a `FATAL_ERROR` guards other compilers). The `fuzz` CMake preset compiles every TU (library + harness) with `-fsanitize=fuzzer-no-link,address,undefined` for edge coverage and links the target with `-fsanitize=fuzzer` — the standard split that keeps coverage feedback spanning the library under test, not just the harness.
- **Standalone replay target `pool_fuzz_replay`** — built with the *normal* test suite (no fuzzing engine, no option needed). Its `main` replays each file named on the command line (the OSS-Fuzz `StandaloneFuzzTargetMain` shape), guarded by `PBR_MEMORY_POOL_LIBFUZZER`. This makes the seed corpus a **portable regression gate** that runs under CTest on every platform — including MSVC — and, being an ordinary test target, keeps `pool_fuzz.cpp` inside the clang-tidy diff gate. The libFuzzer `main` is otherwise supplied by the fuzzer runtime.

A small **seed corpus** lives beside the harness ([`pool_fuzz_corpus/`](../../src/test/cpp/it/d4np/memorypool/pool_fuzz_corpus/)) with a README documenting the byte format and a short local run.

### CI

A dedicated `fuzz` job (Clang, POSIX) builds the target, replays the seed corpus as a regression gate, then fuzzes for a bounded 60 s on every PR; a crash fails the job and uploads the offending input as an artifact. It is kept off the MSVC leg, consistent with the existing sanitizer-preset platform split. Any confirmed defect is filed in the bug ledger ([ADR-0039](0039-bug-ledger-and-triage-protocol.md)) with the crashing input as the reproducer.

### Compatibility

Test-only and additive — no product code changes. The knob and its instrumentation are compiled only under the `fuzz` preset / the opt-in option, so the release build and the [ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md) numbers are untouched. SemVer-neutral; it can ride any release ([ADR-0004](0004-versioning-and-release-policy.md); roadmap item 9.3 under [ADR-0037](0037-new-feature-roadmap-placement.md)).

## Alternatives Considered

- **A libFuzzer-only target, no standalone replay.** Rejected. The seed corpus would then gate only on the Clang CI leg; a broken harness or a regressed input would slip past the maintainer's MSVC box, and `pool_fuzz.cpp` would fall outside the clang-tidy diff gate (it would not appear in the default build's compile database). The standalone replay costs one small `main` and buys portable, always-on validation.
- **Instrument only the harness (`-fsanitize=fuzzer` on the target, library uncovered).** Rejected as the default: coverage feedback would stop at the harness boundary, blinding the fuzzer to the library's own branches. The `fuzz` preset instruments every TU with `fuzzer-no-link` so the guidance spans the code under test.
- **Exercise double-free in the fuzzer.** Rejected — see above; it would flag the documented [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) trade-off as a defect. Hardening ([ADR-0043](0043-opt-in-debug-hardening.md)) owns double-free detection.
- **A separate fuzzing framework (AFL++, standalone) or a new dependency.** Rejected. libFuzzer ships with the Clang the sanitizer matrix already uses (spec §3.3 zero-external-dependency posture), and the harness is written against the plain `LLVMFuzzerTestOneInput` ABI, so it also builds under OSS-Fuzz or AFL++'s libFuzzer-compatible mode without change.
- **Unbounded CI fuzzing / a nightly soak.** Deferred. A bounded 60 s per-PR run is the fast regression gate; a longer soak (or OSS-Fuzz onboarding) can layer on later without changing the harness.

## Consequences

**Positive**

- Sequence-level defects in the allocator — aliasing, overlap, foreign-pointer corruption, growth-boundary bugs, accounting drift — are explored automatically under ASan/UBSan and caught deterministically by the shadow oracle, on every PR.
- The seed corpus is a regression gate on **every** platform via `pool_fuzz_replay`, and the harness stays inside the clang-tidy gate.
- Both fixed and dynamic pools, and the [ADR-0025](0025-decorator-for-instrumented-pool.md) decorator, are exercised by construction. A found crash drops straight into the [ADR-0039](0039-bug-ledger-and-triage-protocol.md) ledger workflow with a ready reproducer.
- Zero effect on the release build and the benchmark numbers — the engine and its instrumentation are strictly opt-in.

**Negative**

- Coverage-guided fuzzing runs only on Clang/POSIX (libFuzzer's platform reach); MSVC gets the corpus replay but not exploration. This is the same split the ASan/UBSan tiers already accept ([ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3).
- The harness models the invariants it knows to assert; a defect it has no oracle for (e.g. one only a new opcode would reach) is invisible until the harness is extended. The opcode set and oracles are meant to grow as the surface does.
- A 60 s per-PR budget is a shallow search; it catches regressions and shallow bugs, not deep ones. A soak / OSS-Fuzz tier is deferred.

**Testing / tooling / documentation (landing in the same PR)**

- [`pool_fuzz.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_fuzz.cpp) + [`pool_fuzz_corpus/`](../../src/test/cpp/it/d4np/memorypool/pool_fuzz_corpus/) (seeds + README).
- [`CMakeLists.txt`](../../CMakeLists.txt) / [test `CMakeLists.txt`](../../src/test/cpp/it/d4np/memorypool/CMakeLists.txt) — the `PBR_MEMORY_POOL_BUILD_FUZZERS` option, the always-built `pool_fuzz_replay` (CTest `pool_fuzz_replay`), and the Clang-gated `pool_fuzz` (CTest `pool_fuzz_corpus`).
- [`CMakePresets.json`](../../CMakePresets.json) — the `fuzz` preset (POSIX-only).
- [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) — the `fuzz` job.
- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §6.4 / §7 and [`ROADMAP.md`](../../ROADMAP.md) item 9.3, [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased`. The `README.md` test-strategy refresh is **deferred to the `v1.2.0` release PR** to keep this PR off the translated docs surface (the same M8.8 / [ADR-0042](0042-pmr-memory-resource-adapter.md) sequencing used by [ADR-0043](0043-opt-in-debug-hardening.md)).

## References

- LLVM libFuzzer — the `LLVMFuzzerTestOneInput` entry point and `-fsanitize=fuzzer[-no-link]` instrumentation model.
- OSS-Fuzz `StandaloneFuzzTargetMain` — the engine-less replay `main` shape mirrored here so the corpus is portable.
- [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) — the foreign / NULL / double-free semantics the harness respects.
- [ADR-0039](0039-bug-ledger-and-triage-protocol.md) — where a fuzzer-found defect is recorded.
