# Changelog

All notable changes to `pbr-cpp-memory-pool` are documented in this file.

The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).
The full versioning and release policy is recorded in
[ADR-0004](docs/adr/0004-versioning-and-release-policy.md); the operational guide for
cutting a release lives in [`docs/workflow/release.md`](docs/workflow/release.md).

Pre-1.0 cadence: `MINOR` increments with each closed roadmap milestone (Milestone 1 →
`v0.1.0`, …, Milestone 6 → `v0.6.0`); `PATCH` covers hotfixes between milestones.
Breaking changes are allowed in a pre-1.0 `MINOR` bump but are always recorded below
under *Changed* or *Removed*.

## [Unreleased]

The `Unreleased` block accumulates entries during development and is rolled into a
dated version block (`## [X.Y.Z] — YYYY-MM-DD`) when a release PR closes a milestone.

### Added

- [ADR-0009](docs/adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md)
  freezes the Milestone 2 layout decisions before any code lands: the implicit
  free list (next-pointer in the first `sizeof(void*)` bytes of free slots,
  per spec §4) initialised in ascending address order; strict `block_size`
  preconditions (`≥ sizeof(void*)` AND `% alignof(std::max_align_t) == 0` —
  `memory_pool_create` returns `NULL` on violation, never silently rounds);
  mandatory `size_t` overflow guard on `block_size * block_count`; backing
  storage via C++17 `::operator new(size, std::align_val_t)` for a single
  zero-external-dependency code path across the Tier-1 platforms; `malloc`-
  parity alignment (`alignof(std::max_align_t)`) on every pointer returned by
  `memory_pool_alloc`; and the `struct memory_pool` field list
  (`backing` / `head` / `block_size` / `block_count` / `alignment`)
  required by the M2.7 foreign-pointer range check.
- [ADR-0010](docs/adr/0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md)
  formalises the two patterns that frame the C++ surface and updates the
  patterns catalogue. **RAII** for `it::d4np::memorypool::Pool` — move-only
  owner of `memory_pool_t*` (copy deleted, single `handle_` data member,
  `sizeof(Pool) == sizeof(void*)`); ctor calls `memory_pool_create`, dtor
  calls `memory_pool_destroy`, both safe on the `NULL` handle from
  ADR-0009's failure semantics. **Pimpl** across the C/C++ boundary —
  `struct memory_pool` forward-declared in `memory_pool.h`, defined
  exclusively in `memory_pool.cpp` (the C handle is the Impl; no separate
  `Pool::Impl` struct). [`docs/patterns/README.md`](docs/patterns/README.md)
  gains the two adopted rows (status flipped from `Planned` to
  `Implemented` once M2.3 lands the bodies). Five rejected alternatives
  are recorded.

### Changed (M2.3)

- `memory_pool_create` and `memory_pool_destroy` are real C++17 bodies
  replacing the Milestone 1 stubs. Construction validates the three
  `block_size` preconditions from [ADR-0009](docs/adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md)
  §2 (`> 0`, `>= sizeof(void*)`, multiple of `alignof(std::max_align_t)`),
  enforces `block_count > 0` and the `size_t`-overflow guard from §3,
  obtains the over-aligned contiguous backing via
  `::operator new(total, std::align_val_t{alignof(std::max_align_t)})`
  with `std::bad_alloc` caught at the C ABI boundary, and initialises
  the implicit free list in ascending address order using `std::memcpy`
  (strict-aliasing-clean). Every failure path returns `NULL`;
  `memory_pool_destroy(nullptr)` is a defined no-op; non-null destroy
  releases the backing via the matching aligned `::operator delete` and
  then frees the metadata struct. `memory_pool_alloc` / `memory_pool_free`
  remain M1 stubs until M2.4.
- `struct memory_pool` is defined exclusively in
  [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp)
  (Pimpl per ADR-0010 — the C handle is the Impl). Field list is the
  five-tuple from ADR-0009 §6: `backing`, `head`, `block_size`,
  `block_count`, `alignment`.
- `memory_pool.h` and `memory_pool.hpp` Doxygen now spell out the three
  `block_size` preconditions, the `block_count` / overflow contract, and
  the `alignof(std::max_align_t)` alignment guarantee on every pointer
  returned by `memory_pool_alloc`. The remaining "ADR pending" qualifiers
  are replaced with concrete `ADR-0009` references.
- Patterns catalogue: **RAII** and **Pimpl** rows flip from `Planned` to
  `Implemented`.
- Spec Coverage Map: §2.1 (pre-allocate contiguous), §4 (implicit free
  list), §5—create, and §5—destroy flip from 🚧 to ✅. §3.1 (no memory
  leaks at destroy) moves from ⏳ to 🚧 — the implementation is in place,
  full validation lands with the Valgrind CI cell in M2.8.
- `pool_smoke_test.cpp` rewritten: the M1-stub-contract test is replaced
  with the M2.3 round-trip; six new `TEST_CASE`s cover each precondition
  violation independently (block_size = 0, < `sizeof(void*)`, misaligned,
  block_count = 0, `size_t` overflow, plus `destroy(nullptr)` safety);
  the RAII move-construct and move-assign tests now assert
  handle-transfer rather than the old M1-stub double-null pattern.

### Changed

- Agent-vs-human boundary: tag creation (`git tag -a v<X.Y.Z>`) and tag push
  (`git push origin v<X.Y.Z>`) are now agent-driven steps, executed immediately
  after the release (or hotfix) PR merges to `master`. The maintainer keeps
  control of *whether* a release happens (by reviewing and merging the release
  PR) and of *when it becomes world-visible* (by clicking *Publish* on the draft
  GitHub Release that `release.yml` produces). Amends [ADR-0004](docs/adr/0004-versioning-and-release-policy.md)
  §6 via [ADR-0008](docs/adr/0008-delegate-tag-creation-and-push-to-the-agent.md);
  [`AGENTS.md`](AGENTS.md) §11 and [`docs/workflow/release.md`](docs/workflow/release.md)
  §2 + §7 updated in lockstep.

## [0.1.0] — 2026-06-10

**Milestone 1 — Build System & Project Skeleton.** First tagged release: a clean,
reproducible C++17 build that links a public C API skeleton (`memory_pool_create`,
`memory_pool_alloc`, `memory_pool_free`, `memory_pool_destroy`) with Milestone 1
stub implementations and passes the full enterprise CI gate on Linux × {GCC, Clang},
Windows × MSVC, and macOS arm64. The library is *linkable* but the public
functions still return `NULL` / no-op; the real free-list algorithm arrives in
Milestone 2 → `v0.2.0`. Full release notes in
[`docs/releases/v0.1.0.md`](docs/releases/v0.1.0.md).

### Added

- Cross-tool agent contract in `AGENTS.md` (persona, language, source layout, git
  workflow, documentation rules, design-patterns policy, enterprise quality bar,
  versioning policy); tool adapters `CLAUDE.md` and `GEMINI.md` defer to it.
- `README.md` landing page with project overview, public C API, architecture diagram,
  status table, repository layout, and a cross-platform Build-and-Test quickstart
  (POSIX `&&`-chain plus a PowerShell variant for Windows 5.1).
- `ROADMAP.md` with numbered, checkbox-driven milestones and a Spec Coverage Map
  tracing every spec requirement to its roadmap item(s).
- Frozen specification at `docs/specs/01_spec_cpp_memory_pool.md`.
- Architecture Decision Records 0001–0007: record ADRs, cross-language source layout,
  design-patterns policy, versioning & release policy, toolchain matrix and supported
  platforms, code style + static-analysis baseline, and doctest as the test framework.
- Design-patterns catalogue under `docs/patterns/` with the canonical enterprise
  taxonomy (`design-patterns.md`) and the project-scoped candidate list (`README.md`).
- Git and documentation conventions under `docs/workflow/` (`git-workflow.md`,
  `documentation.md`, `release.md`).
- Local Build Guide (`docs/development/local-build.md`) covering toolchain
  installation per platform, fresh-clone workflow, and quality-bar verification.
- Pull-request template (`.github/PULL_REQUEST_TEMPLATE.md`) enforcing the
  AGENTS.md §6.4 PR body shape.
- Maven-style cross-language source tree at `src/{main,test,bench}/cpp/it/d4np/memorypool/`
  per ADR-0002; placeholders and `src/README.md` describing the layout.
- Top-level `CMakeLists.txt` declaring the `pbr_memory_pool` static library target
  (with alias `pbr::memory_pool`), reading version constants from
  `src/main/cpp/it/d4np/memorypool/version.hpp` as CMake's single source of truth
  for `project(... VERSION ...)`.
- `CMakePresets.json` with `debug`, `release`, `asan`, `ubsan`, and `tsan` presets;
  sanitizer presets are POSIX-only per ADR-0005 §3.
- Public C API skeleton in `<it/d4np/memorypool/memory_pool.h>` —
  `memory_pool_create`, `memory_pool_alloc`, `memory_pool_free`,
  `memory_pool_destroy` — fully Doxygen-documented to the spec §5 contract.
- C++17 wrapper skeleton in `<it/d4np/memorypool/memory_pool.hpp>` exposing the
  `it::d4np::memorypool::Pool` RAII type.
- Milestone 1 stub implementations in `memory_pool.cpp` (`NULL` / no-op) so the
  library is linkable from day 1; Milestone 2 replaces the stubs with the real
  free-list algorithms.
- `.clang-format` — LLVM-derived style, 4-space indent, 120-col soft limit,
  pointer-aligned-left (ADR-0006 §1).
- `.clang-tidy` baseline (`bugprone-*`, `cert-*`, `cppcoreguidelines-*`,
  `modernize-*`, `performance-*`, `portability-*`, `readability-*`) with the
  deviations recorded in ADR-0006 §2.
- doctest v2.4.11 pulled via `FetchContent` (shallow clone), gated by the
  `PBR_MEMORY_POOL_BUILD_TESTS` option (on by default in every preset) —
  ADR-0007.
- First CTest smoke test (`pool_smoke`, labels `smoke;milestone-1`) exercising
  the version constants, the four spec §5 C symbols, and the `Pool` RAII wrapper
  against the Milestone 1 stub contract.
- Enterprise CI workflow `.github/workflows/ci.yml`: build matrix across
  Linux × {GCC 13, Clang 18} × {debug, release, asan, ubsan} + Windows × MSVC ×
  {debug, release} + macOS arm64 × Apple Clang × {debug, release, asan, ubsan}
  (14 cells), `clang-format` repo-wide dry-run with `-Werror`, `clang-tidy` diff
  gate with `--warnings-as-errors='*'`, ANSI C (`-std=c89`) and C99 (`-std=c99`)
  compatibility verification of the public header, and a zero-external-dependency
  audit that builds the library with tests/benchmarks OFF and inspects the static
  archive for stray third-party objects.
- Docs-only CI workflow `.github/workflows/docs.yml` running markdownlint,
  internal-link integrity via Lychee (offline mode), and ADR-numbering &
  index-coverage sanity.
- Release CI workflow `.github/workflows/release.yml` triggered on `v*` tag
  push (and `workflow_dispatch` for re-runs). Re-runs the full PR-gating
  matrix via `workflow_call` into `ci.yml`, builds per-platform binary
  artifacts (`pbr-memory-pool-<version>-<platform>.tar.gz` for Linux x86_64,
  Windows x86_64, and macOS arm64 — static library + public headers +
  LICENSE + README + CHANGELOG), emits a single `SHA256SUMS`, and creates a
  *draft* GitHub Release whose body is `docs/releases/<tag>.md`. Pre-release
  suffixes (`-alpha.N` / `-beta.N` / `-rc.N`) are auto-detected and
  propagated. The workflow never auto-publishes — the maintainer reviews
  the draft and clicks *Publish* (ADR-0004 §6). `ci.yml` gains a
  `workflow_call:` trigger so the release workflow can invoke it as a
  reusable workflow.

### Changed

- README landing-page title shortened to `High-Performance Memory Pool Manager
  (C++)`. The previous `Purpose-built reference` qualifier was redundant with
  the PBR-series tagline already present in the first paragraph and crowded
  search-engine titles without adding meaning.

### Fixed

- CMake `project(... VERSION ...)` parsing treats `"0"` as a successful regex
  match. CMake's truthiness rule classifies the literal string `"0"` as falsy,
  which previously misfired on legitimate zero version components — for example
  `v0.1.0`'s `MAJOR` and `PATCH` would short-circuit the parse and fail the
  configure on a fresh clone.

---

[Unreleased]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.1.0
