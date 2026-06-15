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

*Nothing yet.*

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
