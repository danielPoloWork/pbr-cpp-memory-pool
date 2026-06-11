# Roadmap

Plan of work for `pbr-cpp-memory-pool`, grouped by milestone. Each item is numbered and checkbox-driven — every PR that completes a roadmap item flips the corresponding checkbox in the same PR. Completed items remain in the list as a permanent record.

> Conventions: items are numbered `<milestone>.<task>`. New work that emerges mid-flight is appended at the end of its milestone with a fresh number; existing numbers are never reused or renumbered. Items that satisfy a clause of the functional/technical specification carry a `(spec §X.Y)` anchor so every line in `git log` is traceable to its contract.
>
> The full requirements catalogue lives in [`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md). A consolidated mapping from spec sections to roadmap items is in the **Spec Coverage Map** at the bottom of this file.

---

## Milestone 0 — Agent & Workflow Scaffolding

Goal: establish the documentation, agent configuration, source-tree shape, and quality bar before writing a single line of pool code.

- [x] 0.1 Persist the senior-C++-architect persona and English-only working language in the cross-tool agent configuration.
- [x] 0.2 Create `AGENTS.md` as the single source of truth for agent behavior.
- [x] 0.3 Add `CLAUDE.md` and `GEMINI.md` adapters that defer to `AGENTS.md`.
- [x] 0.4 Scaffold `docs/` with `README.md`, `adr/`, `specs/`, and `workflow/`.
- [x] 0.5 Add ADR template, ADR index, and ADR-0001 ("Record architecture decisions").
- [x] 0.6 Document the git workflow (branches, Conventional Commits, PR template) in `docs/workflow/git-workflow.md`.
- [x] 0.7 Document the documentation-maintenance rules in `docs/workflow/documentation.md`.
- [x] 0.8 Create `ROADMAP.md` with numbered, checkbox-driven milestones (this file).
- [x] 0.9 Refresh `README.md` with project description, status, and pointers to `AGENTS.md`, `ROADMAP.md`, `docs/`.
- [x] 0.10 Relocate the initial spec to `docs/specs/01_spec_cpp_memory_pool.md`.
- [x] 0.11 Codify the enterprise quality bar (warnings-as-errors, sanitizers, Valgrind, `clang-tidy`, Doxygen) in `AGENTS.md` §10.
- [x] 0.12 Adopt the Maven-style cross-language source layout under `src/main/cpp/it/d4np/memorypool/` — ADR-0002.
- [x] 0.13 Adopt the design-patterns policy and create the patterns catalogue — ADR-0003 + `docs/patterns/README.md`.
- [x] 0.14 Scaffold the production / test / benchmark source roots with placeholders and `src/README.md`.

## Milestone 1 — Build System & Project Skeleton

Goal: a clean, reproducible build with empty stubs that compile, link, and run a no-op test under CTest — built against the Maven-style source tree.

- [x] 1.1 ADR: C++17 toolchain matrix (MSVC, GCC, Clang — Debug & Release) and supported platforms (spec §3.3) — see [ADR-0005](docs/adr/0005-toolchain-matrix-and-supported-platforms.md).
- [x] 1.2 Add `CMakeLists.txt` exposing `src/main/cpp` as the public include root; declare `pbr_memory_pool` library target with sources globbed under `src/main/cpp/it/d4np/memorypool/`.
- [x] 1.3 Add `CMakePresets.json` with `debug`, `release`, `asan`, `ubsan`, `tsan` presets.
- [x] 1.4 Add `.clang-format` (LLVM derivative, 4-space indent, 120-col soft limit) — ADR for style baseline. See [ADR-0006](docs/adr/0006-code-style-and-static-analysis-baseline.md).
- [x] 1.5 Add `.clang-tidy` with the baseline check set declared in `AGENTS.md` §9 — ADR if checks deviate from that baseline. See [ADR-0006](docs/adr/0006-code-style-and-static-analysis-baseline.md).
- [x] 1.6 Add `src/main/cpp/it/d4np/memorypool/memory_pool.h` (public C API skeleton, signatures from spec §5), `memory_pool.hpp` (C++ wrapper skeleton), and `version.hpp` (single source of truth for the project version constants consumed by CMake's `project(... VERSION ...)`) — no-op definitions, fully documented (spec §5; version constants per ADR-0004). Stub implementations live in `memory_pool.cpp` so the library is linkable from M1; real algorithms replace them in M2.
- [x] 1.7 Add CTest wiring; create a no-op smoke test under `src/test/cpp/it/d4np/memorypool/`. Framework choice in [ADR-0007](docs/adr/0007-test-framework-doctest.md) (doctest v2.4.11 via FetchContent).
- [x] 1.8 Set up CI workflow: build matrix, `clang-tidy`, ASan + UBSan, CTest — gate `master` on all green. Implemented in `.github/workflows/ci.yml` per ADR-0005 §4: Linux × {GCC, Clang} × {Debug, Release, ASan, UBSan} + Windows × MSVC × {Debug, Release} + macOS × Apple Clang × {Debug, Release, ASan, UBSan}, plus `clang-format` repo-wide check and `clang-tidy` diff gate with `--warnings-as-errors='*'`.
- [x] 1.9 README quickstart: build / test commands verified on Windows and Linux. The README's *Build and test* section now spells out both the POSIX `&&`-chained one-liner and the Windows PowerShell variant (PS 5.1 has no `&&`), and points to the [CI workflow](.github/workflows/ci.yml) as the canonical "verified on Windows and Linux" surface — the same three commands run daily on Linux × {GCC, Clang}, Windows × MSVC, and macOS × Apple Clang.
- [x] 1.10 ANSI C compatibility verification: dedicated CI job compiling `memory_pool.h` and a minimal C TU under `-std=c89 -pedantic -Werror` and `-std=c99 -pedantic -Werror` to enforce the C interop contract (spec §3.3). The minimal C consumer lives at `src/test/c/it/d4np/memorypool/c_consumer_min.c` (matches the cross-language layout per ADR-0002 — `src/test/<lang>/...`).
- [x] 1.11 Zero-external-dependency verification: CI job that builds with `-nostdinc++` audit / `find_package(...)` introspection failing if any external package leaks into the build graph (spec §3.3). Configures with `PBR_MEMORY_POOL_BUILD_TESTS=OFF` so doctest is not fetched, then asserts no `find_package` in the library scope and inspects the resulting `libpbr_memory_pool.a` archive for stray third-party objects.
- [x] 1.12 Add the initial `CHANGELOG.md` at the repo root in Keep a Changelog 1.1.0 format. The `Unreleased` section is seeded with the user-visible changes accumulated across Milestones 0 and 1 (agent contract, source tree, build system, presets, header skeletons, code-style and static-analysis baselines, doctest test harness, the full enterprise CI workflow, ANSI C / C99 verification, zero-external-dependency audit, and the cross-platform README quickstart) so the Milestone 1.14 roll-up to `[0.1.0]` has real content. The "(once that file exists)" qualifier is also removed from the four places that referenced it (AGENTS.md PR body template + quality-bar table, `.github/PULL_REQUEST_TEMPLATE.md`, `docs/workflow/git-workflow.md`) — the file exists now, the rule is permanent (ADR-0004 §3).
- [x] 1.13 Add `.github/workflows/release.yml` triggered on `v*` tag push: re-run the full test matrix, build per-platform binaries (Linux x86_64, Windows x86_64, macOS arm64 — degrade gracefully where unavailable), emit `SHA256SUMS`, and create a **draft** GitHub Release with the corresponding `docs/releases/v<X.Y.Z>.md` as the body (ADR-0004 §4). Implemented via three jobs — `verify` (workflow_call into `ci.yml` for defense-in-depth re-verification of the tagged commit), `build-artifacts` (per-platform tar.gz packaging the static library + public headers + LICENSE + README + CHANGELOG), and `release` (download every artifact, emit `SHA256SUMS`, draft the GitHub Release with the body from `docs/releases/<tag>.md`). Pre-release suffixes (`-alpha.N` / `-beta.N` / `-rc.N`) are auto-detected and propagated to the GitHub Release. `workflow_dispatch` is exposed for idempotent re-runs (delete-then-recreate the draft) when an initial tag push needs to be replayed.
- [x] 1.14 **Close Milestone 1 → `v0.1.0`**: bump `version.hpp` to `0.1.0`, roll `CHANGELOG.md` Unreleased into a `[0.1.0]` block with ISO date, add `docs/releases/v0.1.0.md` release notes, open the release PR for the maintainer to tag and publish (see [`docs/workflow/release.md`](docs/workflow/release.md)). The version constants were set to `0.1.0` pre-emptively in Milestone 1.6 (under the assumption that M1 would close at `v0.1.0`), so the "bump" step is a no-op verification rather than a real edit; the `CHANGELOG.md` `[Unreleased]` block accumulated through Milestones 0 + 1 is rolled into `[0.1.0] — 2026-06-10`, the link references at the file's foot are rewritten (`[Unreleased]` → `compare/v0.1.0...HEAD`, new `[0.1.0]` → `releases/tag/v0.1.0`), and `docs/releases/v0.1.0.md` is added with human-prose release notes grouped by theme. The maintainer tags `v0.1.0` from `master` after this PR merges and clicks *Publish* on the draft GitHub Release produced by `release.yml`.
- [x] 1.15 CMake configure-smoke CI workflow (`.github/workflows/build-smoke.yml`) — early subset of §1.8. Runs `cmake --preset debug` and `--preset release` on every PR touching CMake / sources / configs. Catches latent `CMakeLists.txt` and preset breakage (like the version-regex zero bug fixed in PR #6) before it reaches a fresh-clone consumer. **Superseded by §1.8** — `build-smoke.yml` is removed in the same PR that introduces `ci.yml`; the full build matrix subsumes the smoke configure step.

## Milestone 2 — Core Memory Pool (Single-Threaded MVP)

Goal: a correct, leak-free, O(1) fixed-block pool matching the spec — single-threaded, no dynamic growth yet, with measured demonstrative patterns.

- [x] 2.1 ADR: implicit free-list layout, `block_size` minimum (≥ `sizeof(void*)`), and alignment guarantee (spec §4, spec §2.1). Implemented as [ADR-0009](docs/adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md): the free list is implicit (next-pointer in first `sizeof(void*)` bytes of free slots) and initialised in ascending address order; `block_size` is strictly validated against `block_size ≥ sizeof(void*)` AND `block_size % alignof(std::max_align_t) == 0` (no silent rounding — `NULL` on violation); `block_count > 0` with a mandatory `size_t` overflow guard on `block_size * block_count`; backing storage allocated via C++17 `::operator new(size, std::align_val_t)` so a single zero-external-dependency code path serves every Tier-1 platform; returned pointer alignment guarantee is `alignof(std::max_align_t)` (drop-in `malloc` parity). The `struct memory_pool` field list (`backing`, `head`, `block_size`, `block_count`, `alignment`) is fixed here so M2.7's foreign-pointer range check has the data it needs; *where* the struct lives (Pimpl vs in-file) is M2.2's call.
- [x] 2.2 ADR: introduce the **RAII** wrapper (`Pool`) and the **Pimpl** idiom across the C++/C boundary; update patterns catalogue. Implemented as [ADR-0010](docs/adr/0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md): the C++ `Pool` is a move-only RAII owner of `memory_pool_t*` (ctor → `memory_pool_create`, dtor → `memory_pool_destroy`, copy deleted, single `memory_pool_t* handle_` data member, `sizeof(Pool) == sizeof(void*)`); `struct memory_pool` is forward-declared in [`memory_pool.h`](src/main/cpp/it/d4np/memorypool/memory_pool.h) and defined exclusively in [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp) (C-style Pimpl — the C handle is the Impl, no separate `Pool::Impl` struct). Both patterns are co-introduced and interdependent, so they share a single ADR per [AGENTS.md](AGENTS.md) §8 #5. Catalogue updated: two new rows in [`docs/patterns/README.md`](docs/patterns/README.md) *Adopted / Planned*, status `Planned` until M2.3 lands the body of `struct memory_pool` and the meaningful semantics of the C functions. Five rejected alternatives recorded (classical C++ Pimpl with `unique_ptr<Impl>`, embed-struct-as-member, `shared_ptr<memory_pool>`, deep-clone copyable Pool, pre-empt the C/C++ exception policy).
- [x] 2.3 Implement `memory_pool_create` and `memory_pool_destroy` with contiguous backing allocation (spec §2.1, spec §5, spec §3.1 — destroy releases all pre-allocated memory). Bodies land in [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp) replacing the M1 stubs: `struct memory_pool` is defined (Pimpl per ADR-0010 — the C handle is the Impl), the three ADR-0009 §2 `block_size` preconditions and the `block_count > 0` + `size_t`-overflow guard from §3 are enforced (`NULL` on violation, never silent rounding), the contiguous backing is obtained via `::operator new(total, std::align_val_t{alignof(std::max_align_t)})` with `std::bad_alloc` caught at the boundary, and the implicit free list is initialised in ascending address order using `std::memcpy` for strict-aliasing safety. `memory_pool_destroy` releases the backing through the matching aligned `::operator delete` and then frees the metadata struct; `NULL` is a no-op. `memory_pool_alloc` / `memory_pool_free` remain M1 stubs — their O(1) bodies arrive in M2.4. Companion edits: `memory_pool.h` and `memory_pool.hpp` Doxygen spell out the three preconditions + the `alignof(max_align_t)` return-pointer alignment guarantee linking ADR-0009; `pool_smoke_test.cpp` is refactored with seven new `TEST_CASE`s covering the valid-args round-trip, `destroy(nullptr)` safety, and one case per precondition violation (block_size = 0, < `sizeof(void*)`, misaligned, block_count = 0, overflow); the RAII wrapper tests are tightened to verify handle-transfer on move and source-becomes-empty after move. Patterns catalogue flips both RAII and Pimpl from `Planned` to `Implemented`.
- [x] 2.4 Implement `memory_pool_alloc` and `memory_pool_free` against the implicit free list — both O(1); `alloc` returns `NULL` on exhaustion in fixed mode (spec §2.2, spec §2.3, spec §5). Bodies in [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp) replace the remaining M1 stubs: `memory_pool_alloc` is a constant-time pop of the implicit free-list head (`block = pool->head_; pool->head_ = *static_cast<void**>(block); return block;` with explicit `nullptr` returns on null pool and on exhausted pool); `memory_pool_free` is the symmetric constant-time push (`*static_cast<void**>(block) = pool->head_; pool->head_ = block;` with no-op guards for null pool and null block). The foreign-pointer / out-of-range detection is M2.7's concern and is documented as UB in the public Doxygen. Companion edits: `pool_smoke_test.cpp` gains six new `TEST_CASE`s (happy-path alloc, null-pool / null-block free no-ops, exhaustion + re-allocation, distinct + aligned pointer guarantee verifying the ADR-0009 §5 contract at runtime, Pool RAII wrapper allocate/deallocate LIFO round-trip) and drops the "still M1 stubs" TEST_CASE that has now served its purpose.
- [x] 2.5 Implement the C++ `it::d4np::memorypool::Pool` RAII wrapper. The wrapper's bodies (ctor → `memory_pool_create`, dtor → `memory_pool_destroy`, move-construct / move-assign, `allocate` / `deallocate` / `native_handle` forwarders) were co-introduced with the C-side implementation in M2.3 and M2.4 — see [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp). M2.5 is the formal acknowledgment that the minimal surface from [ADR-0010](docs/adr/0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) §2 is complete and exercised end-to-end by the smoke-test cases that landed across PRs #18 and #19 (construction, invalid construction → empty wrapper, move-construct handle transfer, move-assign handle release, allocate / deallocate LIFO round-trip). Companion edit: [`memory_pool.hpp`](src/main/cpp/it/d4np/memorypool/memory_pool.hpp) Doxygen drops the now-stale "Milestone 2.2 (ADR pending)" and "function bodies arrive together with the C implementation in Milestone 2" qualifiers, replacing them with concrete ADR-0010 references and a one-paragraph layout note (`sizeof(Pool) == sizeof(void*)`, single `memory_pool_t* handle_` member, copy deleted, move leaves source in valid empty state).
- [x] 2.6 ADR + impl: **Factory Method** / **Builder** for constructing configured pool instances (block size, block count, future growth/threading knobs). Implemented as [ADR-0011](docs/adr/0011-factory-method-and-builder-for-pool-construction.md) covering both patterns (co-introduced and interdependent — Builder's `build()` is the natural caller of Factory Method's `make`, per [AGENTS.md](AGENTS.md) §8 #5). `static std::optional<Pool> Pool::make(block_size, block_count)` is the Factory Method — engaged optional on success, `std::nullopt` on any ADR-0009 §2/§3 failure, orthogonal to the M3.1 `std::bad_alloc`-throwing decision. `class PoolBuilder` is the Builder — fluent `.with_block_size().with_block_count().build()` returning `std::optional<Pool>` via `Pool::make`; const `build()` so the same configured builder can produce independent pools. The Pool ctor stays as the lower-level path with its silent-empty-state semantic from ADR-0010 §2. Catalogue gains two new *Adopted / Planned* rows (status `Implemented`). Seven new `TEST_CASE`s in `pool_smoke_test.cpp` cover the happy path, three failure paths (misaligned, zero count, default-constructed builder, partially-configured builder), and the multi-build property of the const `build()`. Six rejected alternatives recorded in the ADR (Factory returning Pool directly, throwing factory, std::variant for error categories, skip-Factory, skip-Builder, nested Builder).
- [x] 2.7 Correctness tests covering the three scenarios named in spec §6.1: full exhaustion, null inputs, foreign-pointer / out-of-pool-range pointer policy. **Full exhaustion** and **null inputs** were already covered by the M2.3 / M2.4 smoke tests (`memory_pool_alloc exhausts the pool after block_count successful pops`, `memory_pool_destroy(NULL) is a defined no-op`, `memory_pool_alloc returns NULL on a null pool`, `memory_pool_free is a no-op on null pool or null block`). The new work in M2.7 is the **foreign-pointer / out-of-range** scenario, formalised as [ADR-0012](docs/adr/0012-foreign-pointer-and-out-of-range-pointer-policy.md) — `memory_pool_free` runs an `O(1)` range + alignment check against the pool's backing extents (ADR-0009 §6 fields) and silently no-ops on foreign / out-of-range / misaligned pointers. The comparison uses `std::uintptr_t` arithmetic to avoid `[expr.rel]/4` unspecified behaviour on cross-allocation pointer `<`. The C++ `Pool::deallocate` wrapper inherits the policy through its forward to `memory_pool_free`. Five new `TEST_CASE`s exercise out-of-range below, out-of-range above, in-range misaligned, foreign heap pointer (from a sibling pool), and stack pointer — each verifying the pool state is bit-identical before and after the offending call. Double-free detection is explicitly deferred to Milestone 6 (Decorator).
- [x] 2.8 Valgrind job in CI gated on `ERROR SUMMARY: 0 errors from 0 contexts` (spec §3.1, spec §6.2). Carry the exact `gcc -g -O0 ... && valgrind --leak-check=full --show-leak-kinds=all ./test_pool` invocation from spec §6.2 as a literal demonstrative test under `src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/` so the spec-named verification path is reproducible 1:1. Implemented in [`.github/workflows/ci.yml`](.github/workflows/ci.yml) as the `valgrind` job on Ubuntu 24.04: installs Valgrind via `apt`, compiles `test_pool.c` under `gcc -std=c89 -pedantic -g -O0` and `memory_pool.cpp` under `g++ -std=c++17 -g -O0` (the structural C → C++ substitution required because the implementation is C++17 per ADR-0009 §1; the side-by-side mapping with the literal spec command is in [`src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/README.md`](src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/README.md)), links with `g++`, then runs `valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=definite,indirect --error-exitcode=1` and additionally greps the output for the literal spec success criterion. The `--errors-for-leak-kinds=definite,indirect` flag is the only non-spec addition — without it, a leaked backing buffer would pass `--error-exitcode` while violating spec §3.1; `still reachable` and `possible` stay informational so global libstdc++ state does not trip the gate. The `test_pool.c` binary is also registered as a CTest target (`spec_6_2_valgrind`) so the same scenario is exercisable locally without Valgrind under the standard CMake / CTest invocation; it exercises every spec §6.1 scenario that intersects Valgrind's surface (happy path, null inputs, exhaustion, foreign-pointer probes from heap and stack) plus a two-cycle full alloc / free round-trip so the implicit free list is rebuilt under audit before `memory_pool_destroy` releases the backing.
- [ ] 2.9 Microbenchmark vs `malloc`/`free` over 1,000,000 iterations under `src/bench/cpp/it/d4np/memorypool/` (spec §6.3); numbers committed and summarised in the README.
- [ ] 2.10 Metadata-overhead measurement and budget: instrumented test reports bytes of pool-internal metadata as a function of `block_count`; result documented in an ADR and asserted as a CI lower bound (spec §3.2).
- [ ] 2.11 **Close Milestone 2 → `v0.2.0`**: bump `version.hpp`, roll `CHANGELOG.md`, draft `docs/releases/v0.2.0.md`, open release PR (ADR-0004 §2).

## Milestone 3 — C++ Wrapper & Type Safety

Goal: an idiomatic C++17 wrapper around the C core, with RAII and allocator-aware ergonomics.

- [ ] 3.1 ADR: exception policy at the C / C++ boundary — `NULL` on the C side, `std::bad_alloc` on the C++ side, behind a configurable knob (spec §2.2 — "restituire `NULL` o lanciare un'eccezione in C++").
- [ ] 3.2 `it::d4np::memorypool::TypedPool<T>` template with RAII lifetime and `try_allocate` / `allocate` variants.
- [ ] 3.3 ADR + impl: **Adapter** — STL-compatible allocator over the underlying pool; propagation traits specified in the ADR.
- [ ] 3.4 ADR + impl: **Iterator** (read-only) over the free list for diagnostics — disabled in release builds unless explicitly enabled.
- [ ] 3.5 Tests against `std::vector`, `std::list`, and a small custom container.
- [ ] 3.6 **Close Milestone 3 → `v0.3.0`**: bump `version.hpp`, roll `CHANGELOG.md`, draft `docs/releases/v0.3.0.md`, open release PR (ADR-0004 §2).

## Milestone 4 — Thread-Safe Variant

Goal: opt-in thread safety with a measurable single-threaded fast path preserved (spec §2.4).

- [ ] 4.1 ADR: thread-safety **Strategy** (lock-free CAS vs. mutex vs. per-thread caches) and configuration knob (compile-time macro per spec §2.4).
- [ ] 4.2 ADR + impl: **Template Method** allocation skeleton with hook points for the chosen Strategy.
- [ ] 4.3 Implementation behind a compile-time switch; default remains single-threaded so the fast path is preserved (spec §2.4 — "configurabile tramite macro di compilazione per massimizzare le prestazioni single-thread").
- [ ] 4.4 Concurrent stress tests; TSan job added to CI.
- [ ] 4.5 Comparative benchmark: single-thread fast path vs. concurrent path (re-runs spec §6.3 in both modes).
- [ ] 4.6 **Close Milestone 4 → `v0.4.0`**: bump `version.hpp`, roll `CHANGELOG.md`, draft `docs/releases/v0.4.0.md`, open release PR (ADR-0004 §2).

## Milestone 5 — Dynamic Growth Mode

Goal: optional behavior where the pool acquires additional contiguous chunks when exhausted (spec §2.2 — "richiedere un nuovo blocco contiguo se configurato in modalità dinamica").

- [ ] 5.1 ADR: growth policy (geometric vs. linear) and chunk-linking strategy.
- [ ] 5.2 ADR + impl: **Composite** chunk-list representation linking the original pool with overflow chunks.
- [ ] 5.3 Implementation behind a runtime / compile-time flag; default remains fixed-size (spec §2.2).
- [ ] 5.4 Tests and benchmarks covering exhaustion-and-grow scenarios.
- [ ] 5.5 **Close Milestone 5 → `v0.5.0`**: bump `version.hpp`, roll `CHANGELOG.md`, draft `docs/releases/v0.5.0.md`, open release PR (ADR-0004 §2).

## Milestone 6 — Observability & Decorators

Goal: optional logging / statistics / tracing without touching the hot path of release builds.

- [ ] 6.1 ADR + impl: **Decorator** for an instrumented pool variant (counters, allocation histogram, optional logging).
- [ ] 6.2 ADR + impl: **Observer** for pool-lifecycle events (exhaustion, growth, destruction).
- [ ] 6.3 Tests verifying zero-overhead in release builds when instrumentation is disabled.
- [ ] 6.4 **Close Milestone 6 → `v0.6.0`**: bump `version.hpp`, roll `CHANGELOG.md`, draft `docs/releases/v0.6.0.md`, open release PR (ADR-0004 §2).

## Milestone 7 — Release & Polish

Goal: ship a v1.0.0 reference implementation.

- [ ] 7.1 Doxygen-generated API documentation published as a static site.
- [ ] 7.2 README: full usage example, performance summary, compatibility matrix.
- [ ] 7.3 `CHANGELOG.md` audit for the v1.0.0 entry: consolidate every Unreleased line accumulated since `v0.6.0`, verify category placement, and write the v1.0.0 summary headline (the file itself was introduced in Milestone 1.12).
- [ ] 7.4 ADR: install / packaging layout (public-header export, pkg-config, CMake `find_package` config file) — phase 1 distribution per ADR-0004 §5.
- [ ] 7.5 Patterns catalogue audit — verify every adopted pattern has both an ADR and a code location; refresh statuses.
- [ ] 7.6 **Spec compliance acceptance** — walk every row of the Spec Coverage Map (below) and confirm each requirement is satisfied by a passing test, a documented ADR, or both. Record the audit outcome in an ADR.
- [ ] 7.7 **Close Milestone 7 → `v1.0.0`**: bump `version.hpp`, roll `CHANGELOG.md`, draft `docs/releases/v1.0.0.md`, open the release PR for the maintainer to tag and publish (ADR-0004 §2).
- [ ] 7.8 *(Stretch, post-v1.0)* vcpkg port: register `pbr-memory-pool` in microsoft/vcpkg, with portfile pinning to the v1.0.0 tag — phase 2 distribution per ADR-0004 §5.
- [ ] 7.9 *(Stretch, post-v1.0)* Conan recipe: publish a `conanfile.py` to ConanCenter or a self-hosted recipe index, with the same v1.0.0 pin — phase 2 distribution per ADR-0004 §5.

---

## Spec Coverage Map

Traceability from the contract in [`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md) to the roadmap items that fulfil it. Every spec requirement must terminate in at least one roadmap item; Milestone 7.6 (the acceptance audit before v1.0) walks this table row by row.

Legend: ⏳ pending · 🚧 in progress · ✅ done · ❎ not applicable (with reason).

| Spec section | Requirement                                                                       | Roadmap items   | Status |
|--------------|-----------------------------------------------------------------------------------|-----------------|--------|
| §2.1         | Pre-allocate contiguous pool given `block_size` and `block_count`                 | 1.6, 2.1, 2.3   | ✅     |
| §2.2         | O(1) allocation; return `NULL` (C) or `std::bad_alloc` (C++); dynamic growth opt. | 2.4, 3.1, 5.x   | ⏳     |
| §2.3         | O(1) deallocation; block marked free without returning to OS                      | 2.4             | ✅     |
| §2.4         | Optional, configurable thread safety; single-thread fast path preserved           | 4.1–4.5         | ⏳     |
| §3.1         | No memory leaks — destroy releases everything to the OS                           | 2.3, 2.8        | ✅     |
| §3.2         | Minimal metadata overhead per block                                               | 2.1, 2.10       | ⏳     |
| §3.3         | ANSI C / C++17 standard, no external dependencies                                 | 1.1, 1.10, 1.11 | ✅     |
| §4           | Free List implicit (free blocks store the next-free pointer)                      | 2.1, 2.3        | ✅     |
| §5 — create  | `memory_pool_t* memory_pool_create(size_t block_size, size_t block_count)`        | 1.6, 2.3        | ✅     |
| §5 — alloc   | `void* memory_pool_alloc(memory_pool_t* pool)`                                    | 1.6, 2.4        | ✅     |
| §5 — free    | `void memory_pool_free(memory_pool_t* pool, void* block)`                         | 1.6, 2.4        | ✅     |
| §5 — destroy | `void memory_pool_destroy(memory_pool_t* pool)`                                   | 1.6, 2.3        | ✅     |
| §6.1         | Correctness — exhaustion, null inputs, foreign / out-of-range pointers            | 2.7             | ✅     |
| §6.2         | Valgrind clean: `ERROR SUMMARY: 0 errors from 0 contexts`                         | 2.8             | ✅     |
| §6.3         | Benchmark `pool_alloc/free` vs `malloc/free` over 1,000,000 iterations            | 2.9, 4.5        | ⏳     |

When a roadmap item flips from ⏳ to ✅, update the corresponding cell(s) in this table in the same PR.

---

## Session Checkpoint

> Living, dated note describing where the project stands at the end of the most recent work session. Updated at the close of each session so the next session resumes from a known point without re-reading the full PR history. Latest entry first; older entries are kept for trail.

### 2026-06-10 — End of M1 release session (v0.1.0)

- **Done in this session** — Milestone 1 items 1.9 (cross-platform README quickstart, PR #11), 1.12 (initial `CHANGELOG.md` in Keep a Changelog 1.1.0 format, PR #12), 1.13 (`.github/workflows/release.yml` tag-triggered draft-release pipeline, PR #13), and 1.14 (this PR — the `v0.1.0` release PR rolling `CHANGELOG.md` Unreleased into `[0.1.0] — 2026-06-10`, adding `docs/releases/v0.1.0.md`, and updating the README Status block and badge).
- **Library state on `master`** — `pbr_memory_pool` static library + `pbr::memory_pool` alias, header skeletons for `memory_pool.h` / `memory_pool.hpp` / `version.hpp`, M1 stub implementations in `memory_pool.cpp` returning `NULL` / no-op. CTest registers one test (`pool_smoke`) covering the spec §5 surface and the `Pool` RAII wrapper against the M1 stub contract. CI workflows: `ci.yml` (build matrix + format + tidy + ANSI C / C99 + zero-external-deps), `docs.yml` (markdownlint + Lychee + ADR sanity), and `release.yml` (tag-triggered artifact + draft-release pipeline). `CHANGELOG.md` exists at the repo root with a sealed `[0.1.0]` block and a fresh empty `Unreleased`.
- **ADRs accepted to date** — 0001 (record ADRs), 0002 (cross-language layout), 0003 (design-patterns policy), 0004 (versioning & release policy), 0005 (toolchain matrix), 0006 (code style + clang-tidy baseline), 0007 (doctest as test framework).
- **Open issues / follow-ups carried into the next session** — none blocking. After this release PR merges, the maintainer cuts the annotated tag `v0.1.0` from `master`, pushes it, and clicks *Publish* on the draft GitHub Release that `release.yml` produces.
- **Resume the next session with**, in order:
  1. **M2.1** — ADR for the implicit free-list layout, the `block_size ≥ sizeof(void*)` constraint, and the alignment guarantee (spec §4, spec §2.1).
  2. **M2.2** — ADR introducing the **RAII** wrapper and the **Pimpl** idiom across the C / C++ boundary; companion update to `docs/patterns/README.md`.
  3. **M2.3** — implement `memory_pool_create` and `memory_pool_destroy` with contiguous backing allocation.
  4. **M2.4** — implement `memory_pool_alloc` and `memory_pool_free` against the implicit free list; both O(1); `alloc` returns `NULL` on exhaustion in fixed mode.
  5. From M2.5 onward as listed in the Milestone 2 section above.
- **State of the Spec Coverage Map** — §3.3 (ANSI C / C++17 / no external deps) is ✅ as of `v0.1.0`. All algorithm-bearing rows (§2.x, §3.1, §3.2, §4, §5 — create/alloc/free/destroy, §6.1–§6.3) remain at 🚧 or ⏳ and flip across Milestones 2–6 as the real implementations land.

---

**Status check:** As of 2026-06-10 — Milestones 0 and 1 complete; the `v0.1.0` tag follows once this release PR merges. Next: Milestone 2 — Core Memory Pool (single-threaded MVP), targeting `v0.2.0`.
