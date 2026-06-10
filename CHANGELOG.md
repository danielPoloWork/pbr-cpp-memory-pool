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
