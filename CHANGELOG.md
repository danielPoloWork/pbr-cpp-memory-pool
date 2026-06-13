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

### Added (M3.1)

- [ADR-0016](docs/adr/0016-exception-policy-at-the-c-cpp-boundary.md) — exception
  policy at the C/C++ boundary: the C ABI is exception-free forever (every C failure
  is `NULL` / no-op), and the C++ surface adopts a dual-verb convention where the
  spec §2.2 "configurable knob" is resolved per call site, not per build.
- `Pool::try_allocate()` — new `noexcept` allocation verb returning `nullptr` on
  exhaustion or on an empty (moved-from) wrapper; the exact `Pool::allocate()`
  semantics of v0.2.0.

### Added (M3.2)

- [ADR-0017](docs/adr/0017-typed-pool-design.md) and
  [`typed_pool.hpp`](src/main/cpp/it/d4np/memorypool/typed_pool.hpp) —
  `it::d4np::memorypool::TypedPool<T>`, the header-only type-safe pool: the
  spec-conformant `block_size` is derived from `T` at compile time (ADR-0009 §2
  satisfied by construction, over-aligned `T` rejected with a `static_assert`),
  the typed storage verbs follow the ADR-0016 dual-verb policy, and the
  `construct` / `destroy` object-lifetime pair offers the strong exception
  guarantee on throwing `T` constructors. Dedicated `typed_pool` CTest binary
  with eight `TEST_CASE`s.

### Added (M3.3)

- [ADR-0018](docs/adr/0018-stl-allocator-adapter.md) and
  [`pool_allocator.hpp`](src/main/cpp/it/d4np/memorypool/pool_allocator.hpp) —
  `it::d4np::memorypool::PoolAllocator<T>`, the header-only STL-compatible
  allocator **Adapter**. It satisfies the *Cpp17Allocator* requirements and is a
  non-owning back-reference to a `Pool` (single `Pool*` member,
  `sizeof == sizeof(void*)`; the pool must out-live every container and adapter
  copy). Single-block requests (`n == 1`, fitting, not over-aligned) route to the
  pool in O(1) — `std::bad_alloc` on exhaustion per ADR-0016 §2 — while
  everything else (`n > 1`, oversized / over-aligned `T`, rebound nodes larger
  than the block) falls back to over-aligned `::operator new` / `::operator delete`.
  The routing predicate is a pure function of `(n, sizeof(T), alignof(T),
  block_size)`, so `deallocate` returns each pointer through the path that
  allocated it with no per-pointer bookkeeping. `std::list` / `std::map` /
  `std::set` run on the pool fast path; `std::vector` runs on the fallback.
- Propagation traits specified per ADR-0018 §4:
  `propagate_on_container_copy_assignment`,
  `propagate_on_container_move_assignment`, and `propagate_on_container_swap`
  are all `std::false_type`; `is_always_equal` is `std::false_type` (stateful);
  `operator==` compares the underlying `Pool` identity.
- Public C function
  [`memory_pool_block_size(const memory_pool_t*)`](src/main/cpp/it/d4np/memorypool/memory_pool.h)
  — reports the configured per-block size, O(1), `NULL`-tolerant (returns 0),
  ANSI C C89-compatible; the introspection companion to
  `memory_pool_metadata_bytes` that backs the adapter's size-fit decision
  (ADR-0018 §3). C++ forwarder
  `[[nodiscard]] std::size_t Pool::block_size() const noexcept` added in
  lock-step; the accessor is exercised by `c_consumer_min.c` under the C89/C99
  CI jobs.
- Dedicated `pool_allocator` CTest binary
  ([`pool_allocator_test.cpp`](src/test/cpp/it/d4np/memorypool/pool_allocator_test.cpp))
  with seven `TEST_CASE`s: pool-fast-path exhaustion, multi-block + oversized-`T`
  fallback leaving the pool untouched, equality / statefulness / rebinding, the
  propagation-trait `static_assert`s, and end-to-end `std::list` (pool path) +
  `std::vector` (fallback) round-trips.
- **Adapter** added to [`docs/patterns/README.md`](docs/patterns/README.md)
  *Adopted / Planned* table as row #5, status `Implemented`.

### Changed (M3.1)

- **Breaking (pre-1.0):** `Pool::allocate()` now throws `std::bad_alloc` on
  exhaustion (and on a moved-from wrapper) instead of returning `nullptr` —
  migration: `allocate()` → `try_allocate()` for the in-band-failure behaviour.
- **Breaking (pre-1.0):** the `Pool(block_size, block_count)` constructor now throws
  `std::bad_alloc` when the underlying `memory_pool_create` fails, retiring the
  ADR-0010 §2 silent-empty-state semantics — migration: use `Pool::make` or
  `PoolBuilder::build` for failure-as-a-value construction. `Pool::make` is
  restructured around a private adopt-handle constructor so the non-throwing path
  contains no try/catch.
- The microbenchmark's timed loops call `try_allocate()` instead of `allocate()` —
  the apples-to-apples comparison against `malloc`'s in-band `NULL`, byte-identical
  to the code path that produced the committed v0.2.0 numbers (ADR-0016 §4).

## [0.2.0] — 2026-06-11

**Milestone 2 — Core Memory Pool (single-threaded MVP).** The Milestone 1 stubs are
replaced with the real O(1) free-list algorithm: `memory_pool_create` allocates the
contiguous over-aligned backing, validates every ADR-0009 §2/§3 precondition, and
initialises the implicit free list; `memory_pool_alloc` / `memory_pool_free` are
constant-time head-pop / head-push against that list; `memory_pool_destroy` releases
every byte to the OS and is gated Valgrind-clean. The C++ `Pool` wrapper is a
move-only RAII owner with a static `Pool::make` Factory Method and a fluent
`PoolBuilder`. A microbenchmark binary measures the pool at **11.02 × / 5.35 × / 4.45 ×**
faster than `malloc` (bulk-alloc / bulk-free / interleaved) on the maintainer's
Skylake reference host. Eight new Architecture Decision Records freeze the design
contracts (free-list layout, C/C++ boundary, Factory + Builder, foreign-pointer
policy, documentation format, microbenchmark methodology, metadata-overhead budget,
agent-driven tag push). Full release notes in
[`docs/releases/v0.2.0.md`](docs/releases/v0.2.0.md).

### Added

- Architecture Decision Records 0008–0015:
  [0008](docs/adr/0008-delegate-tag-creation-and-push-to-the-agent.md) (delegate
  annotated-tag creation and push to the agent),
  [0009](docs/adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md)
  (free-list layout + `block_size` constraints + alignment guarantee — five-field
  `struct memory_pool`, three `block_size` preconditions, `size_t`-overflow guard,
  C++17 over-aligned `::operator new` for the backing, `alignof(std::max_align_t)`
  return-pointer parity with `malloc`),
  [0010](docs/adr/0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md)
  (move-only RAII `Pool` wrapper + C-style Pimpl across the C/C++ boundary),
  [0011](docs/adr/0011-factory-method-and-builder-for-pool-construction.md)
  (Factory Method `Pool::make` + Builder `PoolBuilder` for configured construction),
  [0012](docs/adr/0012-foreign-pointer-and-out-of-range-pointer-policy.md)
  (foreign-pointer / out-of-range pointer policy — silent no-op via O(1) range +
  alignment check),
  [0013](docs/adr/0013-doxygen-for-api-markdown-for-narrative.md) (retroactive —
  Doxygen for the per-symbol API contract, Markdown for the narrative corpus),
  [0014](docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md) (microbenchmark
  methodology — hand-rolled `std::chrono` with anti-optimization barriers, two
  scenarios, statistical summary, CI smoke-only gate),
  [0015](docs/adr/0015-metadata-overhead-budget-and-introspection.md)
  (metadata-overhead budget — 0 bytes per block, ≤ 128 bytes per pool, with both
  a compile-time and a runtime gate).
- Public C function
  [`memory_pool_metadata_bytes(const memory_pool_t*)`](src/main/cpp/it/d4np/memorypool/memory_pool.h)
  — reports per-pool metadata cost in bytes, NULL-tolerant, ANSI C C89-compatible.
- C++ wrapper methods: `[[nodiscard]] static std::optional<Pool> Pool::make(size_t,
  size_t)` (Factory Method), `[[nodiscard]] std::size_t Pool::metadata_bytes() const
  noexcept`, and the `PoolBuilder` class with fluent `.with_block_size(...)` /
  `.with_block_count(...)` / `.build()`.
- Foreign-pointer detection in `memory_pool_free`: O(1) range + alignment check
  against the pool's backing extents (`std::uintptr_t` arithmetic to avoid
  `[expr.rel]/4` unspecified behaviour) — silent no-op on out-of-range, foreign-heap,
  stack, and in-range-but-misaligned pointers.
- Compile-time metadata-budget gate (`static_assert(sizeof(memory_pool) <= 128U,
  ...)` in
  [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp)) — fires on
  every cell of the 14-cell CI build matrix on every PR.
- `valgrind` CI job in [`ci.yml`](.github/workflows/ci.yml) on Ubuntu 24.04 gated on
  the literal spec §6.2 success criterion `ERROR SUMMARY: 0 errors from 0 contexts`,
  with the C → C++ structural substitution required by the C++17 implementation
  (ADR-0009 §1) documented in the directory README and the side-by-side mapping
  table.
- Spec §6.2 literal demonstrative test at
  [`src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/test_pool.c`](src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/test_pool.c)
  (strict ANSI C89), companion CMake target and `spec_6_2_valgrind` CTest cell.
- `bench-smoke` CI job in `ci.yml` — builds the bench binary with the new `bench`
  preset (Release + benchmarks ON + tests OFF) and runs it briefly; exit-code gate
  only (ADR-0014 §8 — shared GHA runner noise makes numeric thresholds meaningless).
- Microbenchmark binary
  [`pool_vs_malloc_bench`](src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench.cpp)
  implementing the spec §6.3 contract (1,000,000 iterations × 10 repeats per
  scenario, first dropped as warm-up, statistical summary, headline ratio). CLI:
  `--iterations N`, `--repeats N`, `--block-size N`,
  `--scenario {bulk|interleaved|both}`. New `bench` preset opts in.
- Canonical bench report for v0.2.0:
  [`docs/bench/v0.2.0-windows-msvc-x64.md`](docs/bench/v0.2.0-windows-msvc-x64.md)
  (Intel Core i5-6600K @ 3.5 GHz Skylake, 32 GB RAM, MSVC 19.51 Release). Headline
  ratios: **11.02 ×** faster than `malloc` on bulk-alloc, **5.35 ×** on bulk-free,
  **4.45 ×** on interleaved.
- [`docs/bench/`](docs/bench/) directory with index `README.md` documenting the
  contributor recipe for adding reports from other hosts; new `bench` build preset.
- README *Performance* section between *Architecture* and *Status* — three-row
  headline table linking to the full bench report and to ADR-0014.
- README *At a glance* gains a Metadata overhead bullet documenting the
  zero-per-block and ≤128-bytes-per-pool guarantees with link to ADR-0015.
- Two new design patterns added to
  [`docs/patterns/README.md`](docs/patterns/README.md) *Adopted / Planned* table:
  **Factory Method** (`Pool::make`) and **Builder** (`PoolBuilder`), both status
  `Implemented`. The existing **RAII** and **Pimpl** rows flip from `Planned` to
  `Implemented` as the M2.3 + M2.4 bodies land.
- Twenty new doctest `TEST_CASE`s in
  [`pool_smoke_test.cpp`](src/test/cpp/it/d4np/memorypool/pool_smoke_test.cpp)
  covering: every ADR-0009 §2/§3 precondition violation, the create/destroy
  round-trip, alloc / free with exhaustion + re-allocation, distinct-and-aligned
  pointer guarantee, the five foreign-pointer scenarios from ADR-0012, the
  Factory Method + Builder happy + failure paths, the metadata-bytes accessor
  (null / sanity / budget / O(1)-in-`block_count` invariants), and the C++ wrapper
  forwarder.
- *Format* section in
  [`docs/workflow/documentation.md`](docs/workflow/documentation.md) carrying the
  four-row taxonomy table (API contract / narrative / implementation comments /
  test names) and the mutual-exclusion rule between Doxygen and Markdown.
- Session Checkpoint refresh in [`ROADMAP.md`](ROADMAP.md) capturing the state at
  the close of each session through the milestone.

### Changed

- `memory_pool_create` and `memory_pool_destroy` (M2.3): real C++17 bodies replace
  the Milestone 1 stubs. Construction validates the three `block_size` preconditions
  together with `block_count > 0` and the `size_t`-overflow guard from ADR-0009 §2
  and §3, obtains the
  over-aligned contiguous backing via `::operator new(total, std::align_val_t{...})`
  with `std::bad_alloc` caught at the C ABI boundary, and initialises the implicit
  free list in ascending address order. Every failure path returns `NULL`;
  `memory_pool_destroy(NULL)` is a defined no-op; non-null destroy releases the
  backing via the matching aligned `::operator delete` then frees the metadata
  struct.
- `memory_pool_alloc` and `memory_pool_free` (M2.4): real O(1) bodies replace the
  Milestone 1 stubs. Alloc is a constant-time pop of the implicit free-list head;
  free is the symmetric push. Both use the canonical
  `*static_cast<void**>(slot) = ptr` write idiom (no `std::memcpy`, no multi-level
  pointer conversion). Alloc returns `NULL` on exhaustion (fixed-mode per
  ADR-0009 §7); free is a no-op on null pool or null block.
- `Pool` RAII wrapper (M2.5): formal acknowledgment that the minimal surface from
  ADR-0010 §2 is complete — ctor → `memory_pool_create`, dtor → `memory_pool_destroy`,
  move-construct / move-assign, `allocate` / `deallocate` / `native_handle` /
  `metadata_bytes` forwarders, copy deleted. `sizeof(Pool) == sizeof(void*)`.
- `memory_pool.h` Doxygen on `memory_pool_free` documents the new
  silent-no-op-with-detection contract per ADR-0012; the double-free case (in-range,
  aligned, already-on-free-list pointer) is explicitly noted as still UB and
  deferred to Milestone 6's Decorator instrumentation.
- Agent-vs-human boundary (ADR-0008): tag creation (`git tag -a v<X.Y.Z>`) and tag
  push (`git push origin v<X.Y.Z>`) are now agent-driven, executed immediately after
  the release PR merges to `master`. The maintainer retains control of *whether* a
  release happens (by reviewing and merging the release PR) and of *when it becomes
  world-visible* (by clicking *Publish* on the draft GitHub Release). Amends
  ADR-0004 §6; AGENTS.md §11 and `docs/workflow/release.md` updated in lockstep.
- `AGENTS.md` §9 gains a one-sentence cross-link to ADR-0013 (Doxygen for the API
  contract, Markdown for the narrative). The wording does not change; only the
  cross-reference is added.
- `memory_pool.hpp` Doxygen polish (M2.5): the file-level brief drops the
  *"Milestone 2.2 (ADR pending)"* qualifier and the *"function bodies arrive
  together with the C implementation in Milestone 2"* paragraph, both now stale.
  The class-level Doxygen on `Pool` gains a one-paragraph layout note documenting
  the `sizeof(Pool) == sizeof(void*)` property, the single `memory_pool_t* handle_`
  member, the copy-deleted / move-only contract, and the `handle_ == nullptr`
  valid-empty-state invariant.

### Spec Coverage Map flips

This release moves nine rows of the Spec Coverage Map. Coverage at the close of
Milestone 2 — eight ✅, two 🚧, three ⏳:

- **§2.1** (Pre-allocate contiguous pool given `block_size` and `block_count`):
  🚧 → ✅
- **§2.3** (O(1) deallocation; block marked free without returning to OS): ⏳ → ✅
- **§3.1** (No memory leaks — destroy releases everything to the OS): 🚧 → ✅
- **§3.2** (Minimal metadata overhead per block): ⏳ → ✅
- **§4** (Free List implicit — free blocks store the next-free pointer): 🚧 → ✅
- **§5 — create** (`memory_pool_t* memory_pool_create(...)`): 🚧 → ✅
- **§5 — alloc** (`void* memory_pool_alloc(...)`): 🚧 → ✅
- **§5 — free** (`void memory_pool_free(...)`): 🚧 → ✅
- **§5 — destroy** (`void memory_pool_destroy(...)`): 🚧 → ✅
- **§6.1** (Correctness — exhaustion, null inputs, foreign / out-of-range pointers):
  ⏳ → ✅
- **§6.2** (Valgrind clean: `ERROR SUMMARY: 0 errors from 0 contexts`): ⏳ → ✅
- **§6.3** (Benchmark `pool_alloc/free` vs `malloc/free` over 1,000,000 iterations):
  ⏳ → 🚧 (single-threaded coverage complete; full ✅ at M4.5 with the concurrent
  comparative rerun).

§2.2 (return policy on the C++ side + dynamic growth), §2.4 (thread safety), §6.3
(concurrent re-run) remain in flight for Milestones 3–5.

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

[Unreleased]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.2.0
[0.1.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.1.0
