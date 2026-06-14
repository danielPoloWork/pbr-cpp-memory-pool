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

### Changed (M8.7)

- **Consistency lint wired into the agent contract** (ROADMAP §8.7). [`AGENTS.md`](AGENTS.md)
  §6.4 now mandates running `python tools/consistency_lint.py` (and passing it)
  before drafting any post-`v1.0.0` PR; the [PR template](.github/PULL_REQUEST_TEMPLATE.md)
  gains the corresponding checkbox; and [`docs/workflow/maintenance.md`](docs/workflow/maintenance.md)
  gains a **failure → remediation table** mapping each of the six lint checks to
  exactly how to fix it.

### Added (M8.6)

- **Agent-runnable consistency lint** ([ADR-0035](docs/adr/0035-agent-runnable-consistency-lint.md),
  ROADMAP §8.6) — [`tools/consistency_lint.py`](tools/consistency_lint.py), a
  dependency-free (Python 3 stdlib only) checker that asserts cross-artifact
  congruence and exits non-zero with an actionable report. Six checks: version
  lockstep (`version.hpp` ⇆ CHANGELOG ⇆ README badge ⇆ newest release notes), ADR
  index↔file bijection + numbering, every Adopted pattern cites an existing ADR +
  `src/main/cpp/` location, Spec Coverage Map has no dangling row, no `translated`
  i18n entry staler than its English source, and README↔ROADMAP milestone-state
  consistency. Wired into a new `consistency` job in
  [`docs.yml`](.github/workflows/docs.yml) (full-history checkout); also runnable
  locally as `python tools/consistency_lint.py`. The pre-PR checklist that invokes
  it is §8.7.

### Added (M8.5)

- **Post-release maintenance protocol** ([ADR-0034](docs/adr/0034-post-release-maintenance-protocol.md),
  ROADMAP §8.5) — [`docs/workflow/maintenance.md`](docs/workflow/maintenance.md)
  governs the maintained-product phase: it names the version-protected surface
  (C ABI + C++ types + compile-time knobs + the CMake imported target), a
  three-question patch/minor/major **decision tree** (ambiguous rounds up), the
  per-level `version.hpp` / `CHANGELOG` / release-notes mechanics (reusing the
  milestone-close flow), the **hotfix-by-releasability** path (fix on `master` →
  next PATCH, else branch from the tag → PATCH → forward-port), a private-first
  `Security`-categorized fix path, and a deprecate-in-MINOR → window →
  remove-in-MAJOR deprecation policy. Cross-links ADR-0004 / ADR-0008 / AGENTS §11;
  `release.md` gains a reciprocal pointer. The agent-vs-human release boundary is
  unchanged.

### Added (M8.4)

- **Japanese (`ja`) translation — the specification** (ROADMAP §8.4, page 1/3,
  page-by-page). [`docs/i18n/ja/docs/specs/01_spec_cpp_memory_pool.md`](docs/i18n/ja/docs/specs/01_spec_cpp_memory_pool.md)
  is a faithful `ja` translation of the (English) spec at the 1:1 mirrored path,
  pinned to source commit `612f9d2` with the "English is normative" banner; code,
  identifiers, `O(1)`, `Free List`, and the Valgrind command kept verbatim per the
  glossary. The `ja` spec manifest row flips to `translated` and the `ja` index
  links the page. The patterns overview and README follow in subsequent PRs.
- **`ja` translation — the patterns-catalogue overview** (page 2/3). [`docs/i18n/ja/docs/patterns/README.md`](docs/i18n/ja/docs/patterns/README.md)
  translates the catalogue's didactic overview (mirroring the `zh-Hans` page),
  pinned to source commit `6c6aeb7`. Per ADR-0032 §2 the per-row ADR-link table is
  **not** reproduced — it points to the English catalogue; pattern names,
  identifiers, and status keywords stay verbatim. Manifest + `ja` index updated.
- **`ja` translation — the README** (page 3/3, completing §8.4). [`docs/i18n/ja/README.md`](docs/i18n/ja/README.md)
  is the full `ja` translation of the project README (and the `ja` landing page,
  with nav to the spec + patterns translations), pinned to source commit
  `be70cf8`. Code blocks, badge URLs, identifiers, `O(1)`, GoF pattern names, tool
  names, and the numeric benchmark tables kept verbatim; relative links recomputed
  for the deeper path. All three `ja` manifest rows are now `translated` and
  **ROADMAP §8.4 is complete** — both `zh-Hans` and `ja` now cover the full §8.1
  translatable surface.

### Added (M8.3)

- **Simplified Chinese (`zh-Hans`) translation — the specification** (ROADMAP §8.3,
  done page-by-page). [`docs/i18n/zh-Hans/docs/specs/01_spec_cpp_memory_pool.md`](docs/i18n/zh-Hans/docs/specs/01_spec_cpp_memory_pool.md)
  is a faithful `zh-Hans` translation of the (English) spec, pinned to source
  commit `2e55dfa` with the standard "English is normative" banner; code,
  identifiers, `O(1)`, `Free List`, and the Valgrind command are kept verbatim per
  the glossary. The manifest's `zh-Hans` spec row flips to `translated` and the
  `zh-Hans` index links the page. First of the three `zh-Hans` pages (README and
  the patterns overview follow in subsequent PRs).
- **`zh-Hans` translation — the patterns-catalogue overview** (page 2/3). [`docs/i18n/zh-Hans/docs/patterns/README.md`](docs/i18n/zh-Hans/docs/patterns/README.md)
  translates the catalogue's didactic overview (intro, how-to-use, status
  vocabulary, an Adopted *overview* table, the candidate-pattern discussion, and
  out-of-scope categories), pinned to source commit `524f0cc`. Per ADR-0032 §2 the
  per-row ADR-link table is **not** reproduced — it points to the English
  catalogue for the authoritative per-row links + code locations; pattern names,
  identifiers, and status keywords stay verbatim. Manifest + `zh-Hans` index
  updated.
- **`zh-Hans` translation — the README** (page 3/3, completing §8.3). [`docs/i18n/zh-Hans/README.md`](docs/i18n/zh-Hans/README.md)
  is the full `zh-Hans` translation of the project README (and doubles as the
  `zh-Hans` landing page, with nav to the spec + patterns translations), pinned to
  source commit `a01d4f4`. Code blocks, badge URLs, identifiers, `O(1)`, GoF
  pattern names, tool names, and the numeric benchmark tables are kept verbatim;
  relative links are recomputed for the deeper path. With this, all three
  `zh-Hans` manifest rows are `translated` and **ROADMAP §8.3 is complete**.
  Japanese (`ja`) is §8.4.

### Added (M8.1)

- **Documentation i18n architecture** ([ADR-0032](docs/adr/0032-documentation-i18n-architecture.md),
  ROADMAP §8.1). Fixes the shape of the upcoming translation system: a file-based,
  zero-external-dependency per-language Markdown tree under `docs/i18n/<lang>/`
  (`zh-Hans`, `ja`) mirroring the English source path 1:1; English stays normative
  and is the explicit fallback (no empty stubs); the translatable surface is
  README + spec + getting-started/usage + patterns-catalogue overview, while ADRs /
  `CHANGELOG.md` / `ROADMAP.md` / `AGENTS.md` / the Doxygen API reference are
  English-only; a commit-pinned `translation-status.md` manifest makes staleness
  CI-detectable (for §8.6) and a `glossary.md` carries the canonical ↔ `zh-Hans` ↔
  `ja` terms. Decision only — the scaffold is §8.2.

### Added (M8.2)

- **i18n scaffold** (ROADMAP §8.2, [ADR-0032](docs/adr/0032-documentation-i18n-architecture.md)).
  [`docs/i18n/README.md`](docs/i18n/README.md) (contributor guide + translation
  workflow), [`docs/i18n/translation-status.md`](docs/i18n/translation-status.md)
  (the manifest, seeded with all six translatable-page × language rows at
  `missing`), [`docs/i18n/glossary.md`](docs/i18n/glossary.md) (a *Keep in English*
  section + ~24 translatable terms with `zh-Hans` / `ja` renderings), and the
  per-language index pages [`zh-Hans/README.md`](docs/i18n/zh-Hans/README.md) /
  [`ja/README.md`](docs/i18n/ja/README.md) (localised, linking each untranslated
  page to its English source — the explicit fallback; no empty stubs). README's
  repository-layout table gains a `docs/i18n/` row. Translations are §8.3 / §8.4.

### Changed (M8.9)

- **The specification is now maintained in English** ([ADR-0033](docs/adr/0033-english-as-the-spec-normative-language.md),
  ROADMAP §8.9). [`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md)
  was authored in Italian (the original contract); it is translated to English
  **in place** as the single normative source, so the whole repository is uniformly
  English-normative (AGENTS.md §2) and the spec can be localized coherently under
  ADR-0032. The translation is **faithful** — every requirement, the API, the Free
  List description, the diagram, and the verification strategy are preserved with
  identical meaning; only the prose language changes, so the frozen contract is not
  semantically altered. The Italian original remains in git history (commit
  `3ccff68`). Prerequisite for the §8.3 / §8.4 spec translations.
- **English-quote propagation.** Following the spec's translation, the Italian
  quotations embedded elsewhere were updated to the English wording so the
  repository is uniformly English: four spec quotes in `ROADMAP.md`, the spec
  citations in six ADRs (0005, 0009, 0015, 0016, 0020, 0022), and a verbatim
  Italian session-question quote in ADR-0013. Mechanical citation-sync — no ADR
  decision or reasoning changed; `src/**` was already English (audited).

## [1.0.1] — 2026-06-14

**Packaging patch over the frozen `v1.0.0` API.** Ships the two Phase-2
package-manager integrations completed as Milestone 7's stretch items — the
**vcpkg** overlay port (M7.8, [ADR-0030](docs/adr/0030-vcpkg-port.md)) and the
**Conan 2.x** recipe (M7.9, [ADR-0031](docs/adr/0031-conan-recipe.md)), both
pinned to the `v1.0.0` source tag and building through the project's own ADR-0028
install rules. This is a **`PATCH`** because the shipped library is **byte-identical
to `v1.0.0`** — no source, API, ABI, or behaviour change; only repository-side
packaging metadata was added (which is also why the closing of Milestone 8 stays
targeted at `v1.1.0`). Full release notes in [`docs/releases/v1.0.1.md`](docs/releases/v1.0.1.md).

### Added (M7.8)

- **vcpkg port — Phase 2 distribution** ([ADR-0030](docs/adr/0030-vcpkg-port.md),
  ROADMAP §7.8). An in-repo overlay port under [`ports/pbr-memory-pool/`](ports/pbr-memory-pool/)
  ([`vcpkg.json`](ports/pbr-memory-pool/vcpkg.json) + [`portfile.cmake`](ports/pbr-memory-pool/portfile.cmake)),
  pinned to the `v1.0.0` source tag by SHA512. It builds from source through the
  project's own ADR-0028 install rules and relocates the `find_package` config and
  pkg-config `.pc` into vcpkg's layout, so a vcpkg consumer links the same
  `pbr::memory_pool` target. Consumable today via
  `vcpkg install pbr-memory-pool --overlay-ports=ports`; upstream registration in
  microsoft/vcpkg is deferred (the port is written to upstream conventions —
  [`ports/README.md`](ports/README.md)). First post-`v1.0.0` change.

### Added (M7.9)

- **Conan recipe — Phase 2 distribution** ([ADR-0031](docs/adr/0031-conan-recipe.md),
  ROADMAP §7.9). A Conan 2.x recipe under [`conan/`](conan/) — [`conanfile.py`](conan/conanfile.py)
  plus a ConanCenter-style [`test_package/`](conan/test_package/) — pinned to the
  `v1.0.0` source tag by SHA256. It builds from source through the project's
  ADR-0028 CMake rules, drops the upstream-bundled CMake config + `.pc` (Conan's
  `CMakeDeps` supplies the consumer config), and re-exposes the target via
  `package_info` so a Conan consumer links the same `pbr::memory_pool`. Creatable
  today via `conan create conan/`; ConanCenter / self-hosted publication is
  deferred (the recipe follows ConanCenter conventions — [`conan/README.md`](conan/README.md)).
  Mirrors the vcpkg port (§7.8). With this, Milestone 7's stretch items are complete.

## [1.0.0] — 2026-06-14

**Milestone 7 — Release & Polish (`v1.0.0`).** The first **stable** release: the
public C ABI (`memory_pool_create` / `_alloc` / `_free` / `_destroy` plus the O(1)
introspection accessors) and the C++ surface (`Pool`, `TypedPool<T>`,
`PoolAllocator<T>`, `InstrumentedPool`) are frozen under the SemVer 1.0 stability
promise — no breaking change without a `2.0.0`. `v1.0.0` seals the complete feature
set delivered across Milestones 0–6 — the O(1) implicit-free-list fixed-block pool
(spec §2.1–§2.4, §4) with zero per-block metadata, the RAII / typed / STL-allocator
C++ wrapper, compile-time-configurable thread safety, optional geometric dynamic
growth, and opt-in observability — and adds the Milestone 7 release polish: the
published Doxygen API-reference site (M7.1), the expanded usage / performance /
compatibility README (M7.2), the install / packaging layout (M7.4), and the
patterns-catalogue (M7.5) and spec-compliance (M7.6) acceptance audits. The
row-by-row Spec Coverage Map acceptance is recorded in M7.6; no spec row regresses.
Full release notes in [`docs/releases/v1.0.0.md`](docs/releases/v1.0.0.md).

### Added (M7.1)

- **Doxygen-generated API reference published as a static site** ([ADR-0027](docs/adr/0027-doxygen-html-site-and-publication-pipeline.md), ROADMAP §7.1). A
  checked-in partial Doxyfile ([`docs/doxygen/Doxyfile`](docs/doxygen/Doxyfile)) and a
  hand-written landing page ([`docs/doxygen/mainpage.md`](docs/doxygen/mainpage.md)) drive
  a dependency-free Doxygen HTML build (built-in theme + treeview, no Graphviz, no Python
  doc stack) over the public-header contract surface only. `PROJECT_NUMBER` is injected at
  build time from `version.hpp` so the version string stays single-sourced.
- **`docs-site` CI workflow** ([`.github/workflows/docs-site.yml`](.github/workflows/docs-site.yml)) — builds the
  reference as a **warn-as-error gate on every PR** (`WARN_AS_ERROR = FAIL_ON_WARNINGS`,
  refining the ADR-0013 §5 expectation to gate documentation *correctness* — doc-rot,
  stale `@param`, broken refs — not exhaustiveness) and **publishes to GitHub Pages on
  push to `master`** via the official `upload-pages-artifact` / `deploy-pages` Actions.
- README gains a `docs-site` build badge and a pointer to the published API reference.

### Changed (M7.1)

- Three public-header Doxygen comments use the `%` auto-link escape
  (`::%operator new` / `::%operator delete`) to silence spurious unresolved-link warnings
  on the global operators under the new warn-as-error gate. No API or behavior change.

### Changed (M7.2)

- **README expanded for the v1.0 audience** (ROADMAP §7.2): a new **Usage** section with
  six compilable examples (C core, RAII `Pool` + `PoolBuilder`, `TypedPool<T>`,
  `PoolAllocator<T>` with `std::list`, dynamic growth, `InstrumentedPool` + `PoolObserver`),
  each verified by compiling and running a single program against the real headers under
  MSVC 19.51 `/W4`; a consolidated **Performance** summary covering the fixed (4–11×),
  dynamic-growth (~2×), and threaded regimes with links to every `docs/bench/` report; and a
  new **Compatibility** section (Tier-1/Tier-2 platforms, compiler floor versions, C++17 /
  C89+C99 standards, thread-safety modes, zero external dependencies — [ADR-0005](docs/adr/0005-toolchain-matrix-and-supported-platforms.md)).

### Added (M7.4)

- **CMake install / export + pkg-config — Phase 1 distribution** ([ADR-0028](docs/adr/0028-install-and-packaging-layout.md),
  ROADMAP §7.4, ADR-0004 §5). A `PBR_MEMORY_POOL_INSTALL` option (default
  `PROJECT_IS_TOP_LEVEL`) gates `install(TARGETS … EXPORT)` + `install(EXPORT …
  NAMESPACE pbr::)`, the full `it/d4np/memorypool/` public-header tree, a
  relocatable package config (`configure_package_config_file` +
  `SameMajorVersion` version file), a pkg-config
  [`pbr-memory-pool.pc`](cmake/pbr-memory-pool.pc.in), and `LICENSE` under
  `share/doc/`. Consumers use
  `find_package(pbr_memory_pool CONFIG REQUIRED)` +
  `target_link_libraries(app pbr::memory_pool)`.
- The internal target carries `EXPORT_NAME memory_pool` so the **installed**
  imported target is `pbr::memory_pool` — identical to the in-build alias, so a
  single link line serves `add_subdirectory` / `FetchContent` and installed
  packages alike. Verified end-to-end locally (MSVC 19.51 + Ninja): install to a
  prefix, then a separate `find_package` consumer built, linked, and ran.
- README gains an **Install and consume** section.

### Changed (M7.4)

- **`release.yml` packages via `cmake --install`** instead of hand-copying.
  This fixes a latent bug: the previous tarball shipped only three of the seven
  public headers (`typed_pool.hpp`, `pool_allocator.hpp`, `instrumented_pool.hpp`,
  `free_list_iterator.hpp` were missing) and no CMake config / `.pc`, so the
  ADR-0004 §5 "install the artifacts from a Release" path was not actually
  deliverable. Release tarballs are now complete `find_package`-ready install
  trees.

### Changed (M7.5)

- **Patterns-catalogue audit** ([`docs/patterns/README.md`](docs/patterns/README.md), ROADMAP §7.5). All
  eleven adopted patterns (RAII, Pimpl, Factory Method, Builder, Adapter,
  Iterator, Strategy, Template Method, Composite, Decorator, Observer) were
  verified to have an `Accepted` ADR and a live code-location symbol; statuses
  are unchanged (`Implemented`). Forward-looking phrasing that diverged from the
  realized design was refreshed — the Factory Method row (M4 thread safety became
  a compile-time **Strategy**, not Factory dispatch), the Builder row (growth via
  `.with_growth_factor()`; thread safety is a compile-time knob), the Composite
  row (no longer "dormant" — dynamic growth populates `overflow_`), and the RAII
  row (M2 scaffolding phrasing dropped). A dated audit-provenance note was added.

### Added (M7.6)

- **Specification-compliance acceptance audit** ([ADR-0029](docs/adr/0029-spec-compliance-acceptance-audit.md),
  ROADMAP §7.6) — the `v1.0.0` acceptance gate. All fifteen Spec Coverage Map
  rows were re-verified end-to-end against live evidence (ten CTest targets / 95
  doctest cases + the `spec_6_2_valgrind` C program + `c_consumer_min.c`, and the
  `ci.yml` build / `ansi-c-compat` / `zero-external-deps` / `valgrind` /
  `thread-safety` / `tsan` / bench jobs), each cross-referenced to its satisfying
  ADR. **Verdict: every normative spec clause is satisfied; no gap, regression,
  or unsupported mark — the project is acceptance-ready for `v1.0.0`.** No
  coverage cell changes state (all fifteen remain ✅).

## [0.6.0] — 2026-06-14

**Milestone 6 — Observability & Decorators.** Optional logging / statistics / tracing
"without touching the hot path of release builds" (ROADMAP §6 goal). A new header-only
`it::d4np::memorypool::InstrumentedPool` **Decorator** composes a `Pool` and counts
allocation activity (allocations, deallocations, failures, live blocks, and the
`peak_live_` high-water mark), exposed as a `PoolStats` snapshot + `write_summary`. A
runtime **Observer** (`PoolObserver` registered via `add_observer`) delivers
pool-lifecycle events — `exhausted`, `grew`, `destroyed` — reusing the Decorator's
interception points. Observability is **opt-in by type**: a program that uses `Pool`
directly pays nothing — no counter, no branch, no atomic — verified structurally by the
new `zero_overhead` test. The single library-side addition is one `std::atomic<size_t>
grow_count_` on `struct memory_pool` (incremented only on the growth slow path) exposed
by the O(1) `memory_pool_growths` accessor, within the ADR-0015 192-byte budget. Two new
ADRs (0025–0026) take the running total to 26; the patterns catalogue gains **Decorator**
and **Observer** as Implemented. No spec row changes (observability is additive). Full
release notes in [`docs/releases/v0.6.0.md`](docs/releases/v0.6.0.md).

### Added (M6.3)

- Dedicated `zero_overhead` CTest binary
  ([`zero_overhead_test.cpp`](src/test/cpp/it/d4np/memorypool/zero_overhead_test.cpp))
  discharging the [ADR-0025](docs/adr/0025-decorator-for-instrumented-pool.md) §5
  zero-overhead-when-disabled contract ("disabled" = **opt-in by type**: a program
  using `Pool` directly pays nothing). Both §5 obligations are *structural*, so
  they are verified by `static_assert` (config-independent — they hold in Release
  exactly as in Debug) plus a runtime behavioural-equivalence check, **not** a
  timing gate (shared CI runners are too noisy to gate on — [ADR-0014](docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md)
  §8): **(1)** a `std::void_t` detection idiom proves the `stats()` / `add_observer()`
  surface is absent from `Pool` and present on `InstrumentedPool`; **(2)**
  `sizeof(Pool) == sizeof(memory_pool_t*)` and `Pool` stays standard-layout (no
  member / vtable / padding added by the decorator); **(3)** the decorator is at
  least `5 × sizeof(atomic<size_t>) + sizeof(size_t)` larger than `Pool` — the
  counter cost is wholly inside it. The runtime case proves a bare `Pool` and an
  `InstrumentedPool` over the same configuration are indistinguishable: identical
  `metadata_bytes()` (the C `struct memory_pool` does not grow), identical
  `block_size()`, identical capacity / exhaustion, and the identical LIFO
  re-allocation signature. Four `TEST_CASE`s / 77 assertions; as an ordinary CTest
  target it runs in every CI matrix cell including the Release cells — the "in
  release builds" coverage the roadmap item asks for. No new ADR (methodology fixed
  by ADR-0025 §5).

### Added (M6.2)

- [ADR-0026](docs/adr/0026-observer-for-pool-lifecycle-events.md) — the GoF
  runtime **Observer** for pool-lifecycle events, wired into `InstrumentedPool`:
  a `PoolObserver` interface (`virtual on_pool_event(PoolEvent, const PoolStats&)
  noexcept`) registered via `add_observer`, notified on `exhausted` (alloc found
  the pool empty), `grew` (a dynamic pool acquired a chunk), and `destroyed`
  (dtor; a moved-from instance notifies nobody).
- Public C accessor `size_t memory_pool_growths(const memory_pool_t*)` — O(1),
  NULL-tolerant, ANSI C C89-compatible, always present. Backed by a new
  `std::atomic<std::size_t> grow_count_` in `struct memory_pool`, incremented
  only on the growth slow path (`grow_pool`), so the hot path is untouched; the
  Observer reads it to detect growth in O(1). The field adds 8 bytes (NONE
  struct → 64, MUTEX → 144, within the ADR-0015 192 budget).
- Three new `instrumented_pool` `TEST_CASE`s (exhaustion / destruction / growth
  notification) passing under NONE / MUTEX / LOCKFREE; `c_consumer_min.c`
  exercises `memory_pool_growths` under the C89/C99 jobs.
- **Observer** moves to `Implemented` (row #11) in
  [`docs/patterns/README.md`](docs/patterns/README.md).

### Added (M6.1)

- [ADR-0025](docs/adr/0025-decorator-for-instrumented-pool.md) and
  [`instrumented_pool.hpp`](src/main/cpp/it/d4np/memorypool/instrumented_pool.hpp) —
  `it::d4np::memorypool::InstrumentedPool`, the **Decorator** that composes a
  `Pool` and counts allocation activity: `allocations_`, `deallocations_`,
  `allocation_failures_`, `live_`, and the high-water mark `peak_live_`, exposed
  as a `PoolStats` snapshot via `stats()` plus a `write_summary(std::ostream&)`.
  Counters are relaxed atomics (safe to wrap a thread-safe pool), with a
  hand-written move keeping the type factory-returnable (`make` / `make_dynamic`
  mirror `Pool`). Instrumentation is opt-in by type — undecorated `Pool` pays
  nothing (the §6 zero-overhead goal; verified in M6.3). For a fixed-block pool
  a size histogram is degenerate, so `peak_live_` is the occupancy signal;
  event-stream logging is the M6.2 Observer's job.
- Dedicated `instrumented_pool` CTest binary
  ([`instrumented_pool_test.cpp`](src/test/cpp/it/d4np/memorypool/instrumented_pool_test.cpp))
  — five cases (counters/live/peak, exhaustion-failure counting + LIFO
  forwarding, `write_summary`, move semantics + pass-throughs, over-a-dynamic-pool)
  passing under NONE / MUTEX / LOCKFREE.
- **Decorator** moves to `Implemented` (row #10) in
  [`docs/patterns/README.md`](docs/patterns/README.md).

## [0.5.0] — 2026-06-13

**Milestone 5 — Dynamic Growth Mode.** Optional, runtime, per-pool dynamic growth
(spec §2.2): a pool created with `memory_pool_create_dynamic` (or `Pool::make_dynamic`
/ `PoolBuilder::with_growth_factor`) acquires a new geometric contiguous chunk on
exhaustion instead of failing, while the default stays fixed-size — the v0.4.0
behaviour, bit-for-bit. The pool is now a **Composite**: an inline first chunk plus an
append-only list of overflow chunks threaded by one shared implicit free list, so
`alloc` and the `free` push stay O(1); only the `free` safety check and `destroy`
become O(log N) in dynamic mode, and per-block overhead stays zero. Growth is
geometric (chunk count O(log N)) and runs inside the allocation `pop_head` under the
policy's own synchronization (NONE / MUTEX); **lock-free + dynamic is rejected at
creation** (deferred — ADR-0024 §2). Three new ADRs (0022–0024) take the running total
to 24; the patterns catalogue gains **Composite** as Implemented. The ADR-0015 per-pool
metadata budget is renegotiated 128 → 192. Full release notes in
[`docs/releases/v0.5.0.md`](docs/releases/v0.5.0.md).

### Added (M5.4)

- `dynamic_growth` CTest binary
  ([`dynamic_growth_test.cpp`](src/test/cpp/it/d4np/memorypool/dynamic_growth_test.cpp))
  — exhaustion-and-grow tests. The target mirrors the library's thread-safety
  mode: under NONE / MUTEX it covers repeated geometric growth, multiple
  factors, cross-chunk distinctness, full recovery / no leak (ASan- and
  Valgrind-checked), the ADR-0012 range check across grown chunks, and the C++
  `make_dynamic` / `PoolBuilder` surface; under LOCKFREE it asserts the
  rejection contract (ADR-0024 §2) while fixed-mode pools still work.
- `growth` scenario in `pool_vs_malloc_bench` (`--scenario growth|all`): a
  dynamic pool that starts at 256 blocks and grows to `iterations` during a
  bulk alloc, measuring amortized cost including growth (skipped under the
  lock-free build). Committed numbers at
  [`docs/bench/v0.5.0-windows-msvc-x64-growth.md`](docs/bench/v0.5.0-windows-msvc-x64-growth.md)
  — a growing pool is **1.96 ×** faster than `malloc` (55 vs 108 ns/op),
  versus ~11 × for a pre-sized fixed pool.
- CI: the `bench-concurrent-smoke` job is broadened to `bench-policy-smoke`,
  running `--scenario all` (incl. growth) under MUTEX + LOCKFREE.

### Added (M5.3)

- [ADR-0024](docs/adr/0024-dynamic-growth-synchronization-and-creation-surface.md)
  and the dynamic-growth implementation: pools created with
  `memory_pool_create_dynamic(block_size, block_count, growth_factor)` acquire a
  geometric overflow chunk on exhaustion instead of failing (spec §2.2). Growth
  runs inside `pop_head` under the policy's synchronization (plain for NONE,
  under the held mutex for MUTEX); `grow_pool` is `noexcept` and falls back to
  fixed-mode exhaustion on OOM. The frozen `memory_pool_create` stays fixed-mode.
- C++ surface: `Pool::make_dynamic(block_size, block_count, growth_factor)` and
  `PoolBuilder::with_growth_factor(...)` (routing `build()` to the dynamic
  factory when the factor is ≥ 2). Dynamic mode is a runtime, per-pool flag —
  `struct memory_pool` carries a `grow_factor_` (0 = fixed).
- **Lock-free + dynamic is rejected at creation** (`memory_pool_create_dynamic`
  → `NULL`, `make_dynamic` → `std::nullopt`): safe concurrent chunk-list growth
  needs atomic chunk links + a grow-lock and is not TSan-verifiable, so it is
  deferred (ADR-0024 §2). Fixed-mode lock-free pools are fully supported.
- **Breaking (internal only):** the ADR-0015 per-pool metadata budget is
  renegotiated 128 → 192 (per ADR-0015 §4) — `grow_factor_` took the MUTEX
  `struct memory_pool` to 136 bytes. The compile-time `static_assert` and the
  `pool_smoke` runtime budget check move in lockstep; per-block overhead stays
  zero, and `metadata_bytes` now sums the per-chunk overflow descriptors.
- `c_consumer_min.c` exercises `memory_pool_create_dynamic` under the C89/C99
  jobs; two smoke `TEST_CASE`s cover growth past the initial capacity (ASan-leak-
  checked) and the `growth_factor < 2` rejection.

### Added (M5.2)

- [ADR-0023](docs/adr/0023-composite-chunk-list-representation.md) and the
  **Composite** chunk-list representation in
  [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp): a new
  `struct Chunk { backing_, block_count_, next_ }` and a `Chunk* overflow_`
  head on `struct memory_pool`. The pool composes its inline first chunk
  (the existing `backing_`/`block_count_`, ADR-0009 §6 preserved) with a
  forward-linked list of overflow `Chunk` leaves, under one shared implicit
  free list — so `alloc`/`free`-push stay O(1) across any chunk count; only
  `is_block_in_range` and `destroy` walk the list (O(log N) in dynamic mode,
  O(1) in fixed mode). `memory_pool_metadata_bytes` now sums the per-chunk
  descriptors (honest, O(chunks)); per-block overhead stays zero.
- The inline-first-chunk layout keeps the MUTEX `struct memory_pool` at exactly
  the 128-byte ADR-0015 budget (no renegotiation needed this milestone) and
  `metadata_bytes` honest. The representation is dormant (`overflow_` null)
  until dynamic growth populates it in M5.3 — behavior is byte-identical to
  v0.4.0, verified by the full CTest suite passing in NONE / MUTEX / LOCKFREE.
- **Composite** moves to `Implemented` (row #9) in
  [`docs/patterns/README.md`](docs/patterns/README.md).

### Added (M5.1)

- [ADR-0022](docs/adr/0022-dynamic-growth-policy-and-chunk-linking.md) — the
  dynamic-growth policy decision (spec §2.2). Growth is opt-in, **runtime**,
  per-pool (default fixed — v0.4.0 behaviour unchanged), because its decision
  point is the exhaustion slow path, not the hot path. Growth is **geometric**
  (default ×2); **linear is rejected** because geometric keeps the chunk count
  at O(log N) — linear makes it O(N) and degrades `free`'s ADR-0012 safety check
  and `destroy`. Chunks are an append-only singly-linked list threaded by one
  shared implicit free list, so `alloc`/`free`-push stay O(1) (only the `free`
  validation and `destroy` become O(log N) in dynamic mode); per-block overhead
  stays zero (ADR-0015). Chunks are never moved (address stability). The
  Composite chunk-list representation is M5.2, the implementation M5.3. Decision
  only — no source changes.

### Spec Coverage Map flips

- **§2.2** (O(1) allocation; return `NULL` (C) / `std::bad_alloc` (C++); dynamic
  growth optional): 🚧 → ✅ — both clauses are now satisfied (the C++ exception
  half by ADR-0016 in v0.3.0, the dynamic-growth half by M5.1–M5.4 here).

Coverage at the close of Milestone 5: eleven rows ✅. The remaining work is the
Milestone 6 observability/instrumentation items (Decorator, Observer,
double-free detection), which do not correspond to a distinct spec row.

## [0.4.0] — 2026-06-13

**Milestone 4 — Thread-Safe Variant.** Opt-in, compile-time-configurable thread
safety, with the single-threaded fast path preserved at zero cost (spec §2.4). The
allocation algorithm is refactored into a **Template Method** skeleton (ADR-0021)
whose synchronization is a compile-time **Strategy** (ADR-0020) selected by the new
`PBR_MEMORY_POOL_THREAD_SAFETY` macro: `NONE` (default — the v0.3.0 path verbatim),
`MUTEX` (a `std::mutex`), or `LOCKFREE` (an ABA-tagged Treiber-stack CAS). The C ABI,
the C++ wrapper, and the public headers are unchanged — the mode is a library-build
property. Concurrent stress tests + a ThreadSanitizer CI job validate the thread-safe
path, and a concurrent benchmark scenario measures the fast path vs the contended
path across policies. Two new ADRs (0020–0021) take the running total to 21; the
patterns catalogue gains **Strategy** and **Template Method** as Implemented. Full
release notes in [`docs/releases/v0.4.0.md`](docs/releases/v0.4.0.md).

### Added (M4.5)

- Concurrent scenario in `pool_vs_malloc_bench` (ADR-0014): `T` threads run the
  interleaved alloc/free loop against a shared pool, reporting aggregate `ns/op`
  vs `malloc`. New CLI `--threads N` and `--scenario {bulk|interleaved|concurrent|both|all}`;
  the binary prints the `thread_safety_policy` it was built against and clamps
  the concurrent scenario to one thread under the racy `NONE` build (spec §2.4).
- Comparative benchmark report
  [`docs/bench/v0.4.0-windows-msvc-x64-threading.md`](docs/bench/v0.4.0-windows-msvc-x64-threading.md):
  the single-thread fast path is preserved (`NONE` interleaved ≈ 9 ns/op,
  matching M2.9); uncontended synchronization cost is `MUTEX` 47 / `LOCKFREE`
  32 ns/op; under 4-thread contention `LOCKFREE` (41.8) beats `MUTEX` (69.5),
  while a single-shared-head pool cannot out-scale `malloc`'s per-thread arenas
  — motivating the deferred per-thread caches (ADR-0020 §4).
- `bench-concurrent-smoke` CI job — builds + briefly runs the concurrent
  scenario under MUTEX and LOCKFREE (exit-code gate, ADR-0014 §8).
- **Spec Coverage Map §6.3 flips 🚧 → ✅** — the concurrent comparative re-run
  completes the benchmark contract.

### Added (M4.4)

- `concurrency_stress` CTest binary
  ([`concurrency_stress_test.cpp`](src/test/cpp/it/d4np/memorypool/concurrency_stress_test.cpp))
  — drives `Pool` from 8 threads to validate the MUTEX / LOCKFREE policies on
  three invariants: no over-vend / distinctness (concurrent drain hands out
  exactly `block_count` distinct blocks), full recovery / no leak (exact
  `block_count` recovered after heavy churn), and exclusive ownership (a
  per-thread byte marker proves no double-vend). Gated behind
  `PBR_MEMORY_POOL_THREAD_SAFETY != NONE` (a placeholder runs under the default
  single-threaded build); the test target mirrors the library's thread-safety
  mode and links `Threads::Threads`. Runs under the existing `thread-safety` CI
  job (MUTEX + LOCKFREE × GCC + Clang).
- `tsan` CI job (Clang + ThreadSanitizer) running the suite under MUTEX —
  verifies the mutex-guarded path is data-race free. LOCKFREE is intentionally
  excluded from TSan (its Treiber-stack next-link reads are a benign,
  not-cleanly-expressible-in-C++17 race; correctness is covered by the logical
  stress invariants + ADR-0020 §3). Documented in the test header and CI-job
  comment.

### Added (M4.3)

- Thread-safety policies behind the compile-time `PBR_MEMORY_POOL_THREAD_SAFETY`
  switch (ADR-0020, on the M4.2 ADR-0021 skeleton):
  [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp) compiles
  exactly one of `SingleThreadedPolicy` (`NONE`, default — the v0.3.0 path
  verbatim), `MutexPolicy` (`MUTEX` — a `std::mutex` across the O(1) head
  pop/push), or `LockFreePolicy` (`LOCKFREE` — a Treiber-stack
  `compare_exchange_weak` loop on an ABA-tagged `std::atomic<TaggedHead>` head;
  in-slot links stay plain, the tag defeats ABA). The skeleton is unchanged.
- `struct memory_pool` gains policy state conditionally (a 16-byte atomic tagged
  head under LOCKFREE, a `std::mutex` under MUTEX); the ADR-0015
  `static_assert(sizeof(memory_pool) <= 128)` stays green in all three modes.
- Macro constants `PBR_MEMORY_POOL_THREAD_SAFETY_{NONE,MUTEX,LOCKFREE}` + the
  NONE default in [`memory_pool.h`](src/main/cpp/it/d4np/memorypool/memory_pool.h)
  (C89-clean), and the `PBR_MEMORY_POOL_THREAD_SAFETY` CMake option
  (`NONE`|`MUTEX`|`LOCKFREE`) mapping to a PRIVATE compile definition — the mode
  is a library-internal property, the public ABI is unchanged.
- `thread-safety` CI job: builds + runs the full single-threaded CTest suite
  under MUTEX and LOCKFREE on Linux GCC + Clang (the build-correctness gate for
  the switch; concurrent stress + TSan land in M4.4).
- **Strategy** moves to `Implemented` (row #7) in
  [`docs/patterns/README.md`](docs/patterns/README.md).

### Added (M4.2)

- [ADR-0021](docs/adr/0021-template-method-allocation-skeleton.md) — the
  **Template Method** allocation skeleton hosting the ADR-0020 thread-safety
  Strategy. `memory_pool_alloc` / `memory_pool_free` are refactored into the
  `alloc_skeleton` / `free_skeleton` templates: the skeleton owns the invariant
  race-free guards (null pool / null block / foreign-pointer range check), and
  delegates the synchronized free-list head mutation to two compile-time policy
  hooks — `Policy::pop_head` / `Policy::push_head`. The exhaustion test lives
  inside `pop_head` so a future lock-free policy can re-test inside its CAS loop.
- `SingleThreadedPolicy` (the v0.3.0 head pop/push verbatim, no synchronization)
  is the only policy in this milestone and is hard-wired via
  `using ActivePolicy = SingleThreadedPolicy;`. The policy and skeletons are
  internal to [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp)
  (anonymous namespace), so the public C ABI, the C++ wrapper, `struct memory_pool`,
  and the ADR-0015 metadata budget are unchanged; behavior is byte-identical to
  v0.3.0. The `MutexPolicy` / `LockFreePolicy` classes and the
  `PBR_MEMORY_POOL_THREAD_SAFETY` macro selector arrive in M4.3 without touching
  the skeleton. **Template Method** moves to `Implemented` (row #8) in
  [`docs/patterns/README.md`](docs/patterns/README.md).

### Added (M4.1)

- [ADR-0020](docs/adr/0020-thread-safety-strategy-and-compile-time-knob.md) — the
  thread-safety **Strategy** decision (spec §2.4). Thread safety is modelled as the
  GoF Strategy pattern bound **at compile time** (policy-based, not runtime-virtual)
  so the single-threaded build pays nothing. Three policies selected by a new
  `PBR_MEMORY_POOL_THREAD_SAFETY` macro — `…_NONE` (default, the v0.3.0 fast path),
  `…_MUTEX` (`std::mutex`), `…_LOCKFREE` (ABA-tagged Treiber-stack CAS) — fixed for
  the whole library at build time. Per-thread caches are deferred; the Strategy seam
  keeps them a non-breaking future addition. Decision only — the policy classes, the
  macro, and the CMake option are implemented in M4.2 (Template Method skeleton) /
  M4.3. **Strategy** moves to `Planned` in
  [`docs/patterns/README.md`](docs/patterns/README.md).

### Spec Coverage Map flips

- **§6.3** (Benchmark `pool_alloc/free` vs `malloc/free` over 1,000,000 iterations):
  🚧 → ✅ — M4.5's concurrent comparative re-run completes the benchmark contract
  (single-thread fast path + concurrent path across all three policies).
- **§2.4** (Optional, configurable thread safety; single-thread fast path preserved):
  ⏳ → ✅ — delivered by the M4.1–M4.5 compile-time Strategy + Template Method, with
  the fast path measured unchanged.

Coverage at the close of Milestone 4: ten rows ✅; the remaining ⏳ rows are §2.2's
dynamic-growth half (Milestone 5) and the per-block-overhead / instrumentation items
that land in Milestones 5–6.

## [0.3.0] — 2026-06-13

**Milestone 3 — C++ Wrapper & Type Safety.** A C++-ergonomics milestone layered on
the v0.2.0 single-threaded core: the C ABI and its O(1) algorithm are unchanged. The
C++ surface gains a resolved exception policy at the C/C++ boundary (the dual-verb
`allocate` / `try_allocate` convention, with the `Pool` constructor now throwing
`std::bad_alloc` on failure), the type-safe `TypedPool<T>`, an STL-compatible
`PoolAllocator<T>` **Adapter** that lets standard and custom containers draw storage
from a pool, and a read-only free-list diagnostic **Iterator** gated out of release
builds. Four new Architecture Decision Records (0016–0019) take the running total to
19, and the patterns catalogue gains **Adapter** and **Iterator** as Implemented. No
Spec Coverage Map row flips: Milestone 3 is C++-side ergonomics over an already-✅
core — §2.2's "`std::bad_alloc` (C++)" half is now satisfied by ADR-0016, but the row
stays 🚧 pending the dynamic-growth half (Milestone 5). Full release notes in
[`docs/releases/v0.3.0.md`](docs/releases/v0.3.0.md).

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

### Added (M3.4)

- [ADR-0019](docs/adr/0019-free-list-diagnostic-iterator.md) and
  [`free_list_iterator.hpp`](src/main/cpp/it/d4np/memorypool/free_list_iterator.hpp) —
  `it::d4np::memorypool::FreeListIterator` / `FreeListView`, a read-only
  **Iterator** (LegacyForwardIterator) over the implicit free list for
  diagnostics. `value_type` is `const void*` (a free-slot address);
  `FreeListView` is the range adaptor (`begin()` / `end()`, constructible from a
  `const memory_pool_t*` or a `Pool&`) so the walk composes with range-`for`,
  `std::distance`, `std::find`, and the rest of `<algorithm>`. Diagnostics-only:
  the walk is O(free_count) and must never touch the allocation hot path.
- The entire diagnostic surface is gated behind the `PBR_MEMORY_POOL_DIAGNOSTICS`
  macro (ADR-0019 §1), defaulting to `1` in debug builds (`!NDEBUG`) and `0` in
  release builds (`NDEBUG`); an explicit definition wins. The new CMake option
  `PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS` (default `OFF`) is the documented opt-in —
  when `ON` it forces the macro to `1` as a PUBLIC compile definition on
  `pbr_memory_pool` so the library and every linking consumer agree.
- Three gated public C accessors backing the traversal:
  `memory_pool_debug_free_list_head`, `memory_pool_debug_free_list_next`, and
  `memory_pool_debug_free_count` — NULL-tolerant, `const`-correct, ANSI C
  C89-compatible. They keep the ADR-0009 §1 next-link layout encapsulated inside
  [`memory_pool.cpp`](src/main/cpp/it/d4np/memorypool/memory_pool.cpp) (Pimpl
  boundary intact), and are exercised by `c_consumer_min.c` under the C89/C99 CI
  jobs (under the same macro guard).
- Dedicated `free_list_iterator` CTest binary
  ([`free_list_iterator_test.cpp`](src/test/cpp/it/d4np/memorypool/free_list_iterator_test.cpp))
  with five cases when diagnostics are enabled (ascending strided walk of a fresh
  pool with `free_count` vs `std::distance` cross-check, allocation shrinks the
  list, a freed block returns to the head and is walked first, exhausted-pool
  empty range, LegacyForwardIterator behaviours) plus a placeholder case when the
  surface is gated out, so the binary builds in every configuration.
- **Iterator** added to [`docs/patterns/README.md`](docs/patterns/README.md)
  *Adopted / Planned* table as row #6, status `Implemented`.

### Added (M3.5)

- Dedicated `container_integration` CTest binary
  ([`container_integration_test.cpp`](src/test/cpp/it/d4np/memorypool/container_integration_test.cpp))
  exercising the M3.3 `PoolAllocator<T>` (ADR-0018) end-to-end through
  `std::list` (pool fast path, with diagnostics-gated free-count delta
  assertions and `std::string` elements for non-trivial construct/destroy),
  `std::vector` (heap fallback, contents/growth/copy/`<algorithm>` interop),
  and a small hand-written allocator-aware `ForwardList<T, Allocator>` driven
  with both `std::allocator` and `PoolAllocator`. Seven `TEST_CASE`s.
- The pool-sizing recipe for node-based containers (deferred to M3.5 by
  ADR-0018 §3) is documented as a worked example in the test file header:
  `block_size ≥ sizeof(rebound node)`; an undersized pool degrades safely to
  the heap fallback. The comprehensive README usage section remains M7.2.

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

### Spec Coverage Map flips

None. Milestone 3 adds C++-side ergonomics over the already-✅ single-threaded core,
so no traceability row changes state. ADR-0016 satisfies the "`std::bad_alloc` (C++)"
clause of **§2.2**, but that row stays 🚧 because its dynamic-growth clause lands in
Milestone 5. Coverage at the close of Milestone 3 is unchanged from v0.2.0: eight ✅,
one 🚧 (§6.3), and §2.2 / §2.4 / §6.3-concurrent in flight for Milestones 4–5.

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

[Unreleased]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v1.0.1
[1.0.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v1.0.0
[0.6.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.6.0
[0.5.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.5.0
[0.4.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.4.0
[0.3.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.3.0
[0.2.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.2.0
[0.1.0]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.1.0
