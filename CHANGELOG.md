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

- **`std::pmr::memory_resource` adapter — `PoolMemoryResource`.** A new header-only Adapter
  ([`pool_memory_resource.hpp`](src/main/cpp/it/d4np/memorypool/pool_memory_resource.hpp))
  binds one `Pool` behind the runtime `std::pmr::memory_resource` interface, so a single
  resource can back any `std::pmr` container through `std::pmr::polymorphic_allocator` without
  the per-type `PoolAllocator<T>` rebind — the "door left open" in ADR-0018. Deterministic
  `(bytes, alignment)` routing to the pool (over-sized / over-aligned requests fall back to a
  configurable upstream resource; exhaustion throws `std::bad_alloc`), `is_equal` by
  `(pool, upstream)` identity, gated behind `PBR_MEMORY_POOL_HAS_PMR` where `<memory_resource>`
  is available. Purely additive and ABI-compatible — the frozen C ABI and existing C++ types
  are unchanged; opens roadmap Milestone 9 (a `v1.2.0` candidate).
  [ADR-0042](docs/adr/0042-pmr-memory-resource-adapter.md). Closes #107.
- **C4 component diagram of the pool internals** in the specification
  ([`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md), §4.2),
  authored in Mermaid. [ADR-0041](docs/adr/0041-mermaid-diagram-tooling.md) records Mermaid
  as the in-repo diagram tooling — C4 levels are drawn as flowcharts with `subgraph`
  boundaries, and PlantUML, checked-in images, and Mermaid's experimental `C4Component` DSL
  were considered and rejected. Closes the last open item of the specification review.
  Documentation-only; no API change. Refs #105.

### Changed

- **`zh-Hans` / `ja` README translations re-synced to `v1.1.2`.** Carries the English
  README's `v1.1.2` deltas into both locales — the `v1.1.2` status badge (and its
  release-tag link) and the new `v1.1.2` status paragraph (the four BUG-0001…0004
  fixes, the `docs-site` badge removal, and the translation re-sync). The
  `translation-status.md` manifest's two README rows are re-pinned to the current
  source commit (`d38b598`) and flipped from `stale` back to `translated`, clearing
  the `i18n-freshness` flag the `v1.1.2` release raised. Documentation-only; no API change.
- **Specification reconciled with the as-built system.**
  [`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md) was the
  original greenfield brief and had drifted from the delivered (`v1.0.0`-frozen) library. It
  now cross-links the realizing ADRs, formalizes the `§2`/`§3` subsection anchors already
  referenced across the ADR set, disambiguates the `§2.2` dynamic-growth model (non-contiguous
  chunk-linking; ADR-0022/0023/0024), documents the `§4.1`
  block-size/alignment/strict-aliasing constraints (ADR-0009), the `§5.3` error semantics
  (ADR-0012/0016) and `§5.4` introspection (ADR-0015/0025), and adds a `§7` spec→ADR map plus
  the explicitly deferred items (#107 `pmr`, #108 fuzzing, #109 hardening). The
  `zh-Hans`/`ja` spec translation rows are marked `stale` pending a follow-up re-sync.
  Documentation-only; no API change. Refs #105.

## Released versions

Each released version is an **immutable** entry under [`docs/changelog/`](docs/changelog/) — one file per release, newest first ([ADR-0038](docs/adr/0038-changelog-version-split.md)). They are never edited after release; only the `Unreleased` block above changes during development.

| Version | Date | Highlights |
|---------|------|------------|
| [1.1.2](docs/changelog/v1/v1.1.2.md) | 2026-06-15 | Maintenance — InstrumentedPool/core bug fixes (BUG-0001…0004) + docs |
| [1.1.1](docs/changelog/v1/v1.1.1.md) | 2026-06-15 | Maintenance — bug ledger, PR-metadata policy, journal split, SECURITY.md, CI Node bump |
| [1.1.0](docs/changelog/v1/v1.1.0.md) | 2026-06-14 | Internationalization (zh-Hans / ja) & post-release governance |
| [1.0.1](docs/changelog/v1/v1.0.1.md) | 2026-06-14 | Packaging patch — vcpkg port + Conan recipe (byte-identical lib) |
| [1.0.0](docs/changelog/v1/v1.0.0.md) | 2026-06-14 | First stable release — public C ABI and C++ surface frozen |
| [0.6.0](docs/changelog/v0/v0.6.0.md) | 2026-06-14 | Observability & Decorators (InstrumentedPool, PoolObserver) |
| [0.5.0](docs/changelog/v0/v0.5.0.md) | 2026-06-13 | Dynamic growth mode (Composite overflow chunks) |
| [0.4.0](docs/changelog/v0/v0.4.0.md) | 2026-06-13 | Thread-safe variant (compile-time Strategy: none / mutex / lock-free) |
| [0.3.0](docs/changelog/v0/v0.3.0.md) | 2026-06-13 | C++ wrapper & type safety (TypedPool, PoolAllocator, diagnostics) |
| [0.2.0](docs/changelog/v0/v0.2.0.md) | 2026-06-11 | Core memory pool — single-threaded O(1) MVP |
| [0.1.0](docs/changelog/v0/v0.1.0.md) | 2026-06-10 | Build system & project skeleton |

[Unreleased]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/compare/v1.1.2...HEAD
