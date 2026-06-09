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

- [ ] 1.1 ADR: C++17 toolchain matrix (MSVC, GCC, Clang — Debug & Release) and supported platforms (spec §3.3).
- [ ] 1.2 Add `CMakeLists.txt` exposing `src/main/cpp` as the public include root; declare `pbr_memory_pool` library target with sources globbed under `src/main/cpp/it/d4np/memorypool/`.
- [ ] 1.3 Add `CMakePresets.json` with `debug`, `release`, `asan`, `ubsan`, `tsan` presets.
- [ ] 1.4 Add `.clang-format` (LLVM derivative, 4-space indent, 120-col soft limit) — ADR for style baseline.
- [ ] 1.5 Add `.clang-tidy` with the baseline check set declared in `AGENTS.md` §9 — ADR if checks deviate from that baseline.
- [ ] 1.6 Add `src/main/cpp/it/d4np/memorypool/memory_pool.h` (public C API skeleton, signatures from spec §5), `memory_pool.hpp` (C++ wrapper skeleton), and `version.hpp` (single source of truth for the project version constants consumed by CMake's `project(... VERSION ...)`) — no-op definitions, fully documented (spec §5; version constants per ADR-0004).
- [ ] 1.7 Add CTest wiring; create a no-op smoke test under `src/test/cpp/it/d4np/memorypool/`.
- [ ] 1.8 Set up CI workflow: build matrix, `clang-tidy`, ASan + UBSan, CTest — gate `master` on all green.
- [ ] 1.9 README quickstart: build / test commands verified on Windows and Linux.
- [ ] 1.10 ANSI C compatibility verification: dedicated CI job compiling `memory_pool.h` and a minimal C TU under `-std=c89 -pedantic -Werror` and `-std=c99 -pedantic -Werror` to enforce the C interop contract (spec §3.3).
- [ ] 1.11 Zero-external-dependency verification: CI job that builds with `-nostdinc++` audit / `find_package(...)` introspection failing if any external package leaks into the build graph (spec §3.3).
- [ ] 1.12 Add the initial `CHANGELOG.md` at the repo root in Keep a Changelog 1.1.0 format, with an empty `Unreleased` section ready to accept entries from this and subsequent PRs (ADR-0004 §3).
- [ ] 1.13 Add `.github/workflows/release.yml` triggered on `v*` tag push: re-run the full test matrix, build per-platform binaries (Linux x86_64, Windows x86_64, macOS arm64 — degrade gracefully where unavailable), emit `SHA256SUMS`, and create a **draft** GitHub Release with the corresponding `docs/releases/v<X.Y.Z>.md` as the body (ADR-0004 §4).
- [ ] 1.14 **Close Milestone 1 → `v0.1.0`**: bump `version.hpp` to `0.1.0`, roll `CHANGELOG.md` Unreleased into a `[0.1.0]` block with ISO date, add `docs/releases/v0.1.0.md` release notes, open the release PR for the maintainer to tag and publish (see [`docs/workflow/release.md`](docs/workflow/release.md)).

## Milestone 2 — Core Memory Pool (Single-Threaded MVP)

Goal: a correct, leak-free, O(1) fixed-block pool matching the spec — single-threaded, no dynamic growth yet, with measured demonstrative patterns.

- [ ] 2.1 ADR: implicit free-list layout, `block_size` minimum (≥ `sizeof(void*)`), and alignment guarantee (spec §4, spec §2.1).
- [ ] 2.2 ADR: introduce the **RAII** wrapper (`Pool`) and the **Pimpl** idiom across the C++/C boundary; update patterns catalogue.
- [ ] 2.3 Implement `memory_pool_create` and `memory_pool_destroy` with contiguous backing allocation (spec §2.1, spec §5, spec §3.1 — destroy releases all pre-allocated memory).
- [ ] 2.4 Implement `memory_pool_alloc` and `memory_pool_free` against the implicit free list — both O(1); `alloc` returns `NULL` on exhaustion in fixed mode (spec §2.2, spec §2.3, spec §5).
- [ ] 2.5 Implement the C++ `it::d4np::memorypool::Pool` RAII wrapper.
- [ ] 2.6 ADR + impl: **Factory Method** / **Builder** for constructing configured pool instances (block size, block count, future growth/threading knobs).
- [ ] 2.7 Correctness tests covering the three scenarios named in spec §6.1: full exhaustion, null inputs, foreign-pointer / out-of-pool-range pointer policy.
- [ ] 2.8 Valgrind job in CI gated on `ERROR SUMMARY: 0 errors from 0 contexts` (spec §3.1, spec §6.2). Carry the exact `gcc -g -O0 ... && valgrind --leak-check=full --show-leak-kinds=all ./test_pool` invocation from spec §6.2 as a literal demonstrative test under `src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/` so the spec-named verification path is reproducible 1:1.
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
| §2.1         | Pre-allocate contiguous pool given `block_size` and `block_count`                 | 1.6, 2.1, 2.3   | ⏳     |
| §2.2         | O(1) allocation; return `NULL` (C) or `std::bad_alloc` (C++); dynamic growth opt. | 2.4, 3.1, 5.x   | ⏳     |
| §2.3         | O(1) deallocation; block marked free without returning to OS                      | 2.4             | ⏳     |
| §2.4         | Optional, configurable thread safety; single-thread fast path preserved           | 4.1–4.5         | ⏳     |
| §3.1         | No memory leaks — destroy releases everything to the OS                           | 2.3, 2.8        | ⏳     |
| §3.2         | Minimal metadata overhead per block                                               | 2.1, 2.10       | ⏳     |
| §3.3         | ANSI C / C++17 standard, no external dependencies                                 | 1.1, 1.10, 1.11 | ⏳     |
| §4           | Free List implicit (free blocks store the next-free pointer)                      | 2.1             | ⏳     |
| §5 — create  | `memory_pool_t* memory_pool_create(size_t block_size, size_t block_count)`        | 1.6, 2.3        | ⏳     |
| §5 — alloc   | `void* memory_pool_alloc(memory_pool_t* pool)`                                    | 1.6, 2.4        | ⏳     |
| §5 — free    | `void memory_pool_free(memory_pool_t* pool, void* block)`                         | 1.6, 2.4        | ⏳     |
| §5 — destroy | `void memory_pool_destroy(memory_pool_t* pool)`                                   | 1.6, 2.3        | ⏳     |
| §6.1         | Correctness — exhaustion, null inputs, foreign / out-of-range pointers            | 2.7             | ⏳     |
| §6.2         | Valgrind clean: `ERROR SUMMARY: 0 errors from 0 contexts`                         | 2.8             | ⏳     |
| §6.3         | Benchmark `pool_alloc/free` vs `malloc/free` over 1,000,000 iterations            | 2.9, 4.5        | ⏳     |

When a roadmap item flips from ⏳ to ✅, update the corresponding cell(s) in this table in the same PR.

---

**Status check:** As of 2026-06-09 — Milestone 0 complete; Milestone 1 ready to start.
