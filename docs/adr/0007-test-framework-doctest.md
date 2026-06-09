# ADR-0007: Test framework — doctest

- **Status:** Accepted
- **Date:** 2026-06-10
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §10; [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md); spec §6.1; ROADMAP §1.7

## Context

Milestone 1.7 wires CTest into the project and lands the first smoke test. CTest is the test-runner contract — it tells CMake which executables to run and gates `master` on their pass/fail. But CTest is not a test framework: it is agnostic to the assertions inside each test executable. The framework choice determines what an authored test file actually looks like.

Three plausible candidates dominate the modern C++ ecosystem: **doctest**, **Catch2 (v3)**, and **GoogleTest** (with optional GMock). Each is mature, well-supported, and CTest-compatible. The trade-offs differ in three axes that matter for this specific project:

1. **Build cost.** This is a reference implementation; the M1 stub library compiles in under a second. A test framework that itself takes 30 s to compile dominates the iteration loop.
2. **Dependency surface.** Spec §3.3 — "no external dependencies" — applies to the *runtime* contract, not test code, but the spirit transfers: the fewer moving parts a fresh-clone contributor must understand to add a test, the better.
3. **Macro vocabulary familiarity.** A reference project that wants to be read needs assertions that look unsurprising. `CHECK` / `REQUIRE` (Catch-family) and `EXPECT_EQ` / `EXPECT_THAT` (GoogleTest) are both fine; idiomatic preference matters.

## Decision

We adopt **doctest**, pinned at **`v2.4.11`** (the latest stable as of the date of this ADR), pulled into the build via CMake `FetchContent` at configure time.

The integration is contained in the top-level `CMakeLists.txt`:

```cmake
option(PBR_MEMORY_POOL_BUILD_TESTS "Build the test targets under src/test/cpp/" OFF)
if(PBR_MEMORY_POOL_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(doctest)
    add_subdirectory(src/test/cpp/it/d4np/memorypool)
endif()
```

Each test executable lives under `src/test/cpp/it/d4np/memorypool/`, links against `doctest::doctest` and `pbr::memory_pool`, defines `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` in exactly one translation unit per executable (so the test binary owns its own `main`), and registers itself with CTest via `add_test()`.

Version bumps for doctest go through a normal PR that updates the `GIT_TAG` and runs the full test matrix; pinning is intentional so contributors do not inherit a silent toolchain change.

## Alternatives Considered

- **Catch2 v3.** Sibling project to doctest with very similar macro vocabulary (`TEST_CASE`, `SECTION`, `CHECK`, `REQUIRE`). Strong feature set: generators, fixtures, microbenchmark support (`BENCHMARK`). Rejected as the *default* choice because v3 is no longer single-header — it ships as a real library with its own CMake target hierarchy. `FetchContent` works but the configure-time clone is larger and the first build adds ~10–15 s on a cold runner. The microbenchmark surface duplicates what we plan to ship via a dedicated `src/bench/` tree in Milestone 2.9. We may revisit if a specific Catch2-only feature becomes load-bearing.
- **GoogleTest (+ GMock).** Industry standard, excellent documentation, GMock for behaviour mocking, deep integration in many tooling pipelines. Rejected because:
  - The macro vocabulary (`EXPECT_EQ`, `ASSERT_NE`, `EXPECT_THAT` + matchers, `MOCK_METHOD` …) is more verbose than `CHECK(a == b)` / `REQUIRE(a == b)`; for a project whose pages should *read* easily, the noise floor is higher.
  - Compile time is the highest of the three. On a debug build of a single empty test file, GoogleTest's headers add roughly 5× the compile time of doctest's.
  - This project will not need GMock — its public surface is small enough that real fixtures, not mocked seams, are the right test boundary.
- **Custom (no framework — `main()` + `<cassert>`).** Maximally minimal and dep-free. Rejected because it forfeits the things a framework actually buys us: discoverable test cases, sub-section grouping, diagnostic messages on `CHECK` failure that print both sides of an expression, and the ability to skip / tag tests. A future contributor opening the file should see standard idioms, not a bespoke harness.
- **Bundled / vendored doctest header in `third_party/`.** Considered for full reproducibility independent of GitHub availability. Rejected for now: the `FetchContent` flow with a pinned tag and shallow clone gives the same reproducibility (the tag identifies a specific commit hash), and avoids the maintenance burden of refreshing a vendored header on every doctest release. If GitHub access ever becomes a constraint (air-gapped CI runners, etc.) we can flip to vendoring in a future ADR.

## Consequences

**Positive**

- Zero per-test-file boilerplate beyond `#include <doctest/doctest.h>` and `TEST_CASE("...") { ... }`. New contributors are productive immediately.
- doctest is single-header at the consumption side (the FetchContent makes a library target but each user TU sees one include path). Compile times stay tight.
- The test binary is self-contained — no separate test runner process, no test-data harness, no XML output server required. CTest invokes the binary; doctest prints structured progress; the exit code is the verdict.
- `FetchContent` with `GIT_SHALLOW TRUE` keeps the configure step well under the budget set by [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §4.
- Switching to a different framework later (if a strong reason emerges) is a local-to-the-test-tree change. The library itself never depends on doctest.

**Negative**

- Each test executable needs `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` exactly once — a one-line obligation, but easy to copy-paste wrong across files. We mitigate by keeping one test executable per concern in Milestone 1; multi-file test executables will be addressed by a dedicated `doctest_main.cpp` TU when they appear.
- `FetchContent` runs at configure time, requiring network access to `github.com`. CI runners have it; offline contributors do not. Mitigated by CMake caching the cloned source under `build/_deps/`; only the first configure pays the network cost.
- doctest's microbenchmark macros are weaker than Catch2's. Acceptable because microbenchmarks live in `src/bench/` (Milestone 2.9) under a separate tool to be picked later — possibly Google Benchmark.

## References

- doctest — <https://github.com/doctest/doctest>
- doctest documentation — <https://github.com/doctest/doctest/blob/master/doc/markdown/readme.md>
- CMake `FetchContent` — <https://cmake.org/cmake/help/latest/module/FetchContent.html>
- Catch2 v3 — <https://github.com/catchorg/Catch2>
- GoogleTest — <https://github.com/google/googletest>
- [`AGENTS.md`](../../AGENTS.md) §10 — "Unit tests pass" gate.
- spec §6.1 — correctness-test obligations (full exhaustion, null inputs, foreign pointers) addressed by tests starting at Milestone 2.7.
