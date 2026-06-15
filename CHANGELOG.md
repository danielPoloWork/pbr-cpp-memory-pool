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

### Removed

- **The `docs-site` CI status badge** was removed from the README header in all three
  locales (English, `zh-Hans`, `ja`). The published Doxygen site is already linked from
  the **API reference** badge, so the separate build-status badge was redundant. The
  `docs-site.yml` workflow itself is unchanged. The badge is removed from the
  translations in the same change; the `translation-status.md` manifest's two README
  rows are marked `stale` here (the English source moved ahead of their pin) and
  re-pinned + flipped back to `translated` in a follow-up once this lands on `master`.
  Documentation-only; no API change.

### Changed

- **`zh-Hans` / `ja` README translations re-synced to `v1.1.1`.** Carries the English
  README's `v1.1.1` deltas into both locales — the `v1.1.1` status badge (and its
  release-tag link) and the new `v1.1.1` status paragraph (bug ledger, PR-metadata
  policy, `SECURITY.md`, `packaging-smoke` CI, session journal, the roadmap-placement
  rule, and the changelog split). The `translation-status.md` manifest's two README
  rows are re-pinned to the current source commit (`23fc6c4`) and flipped from `stale`
  back to `translated`, clearing the `i18n-freshness` flag the `v1.1.1` release raised.
  Documentation-only; no API change.

### Fixed

- **`InstrumentedPool` data race on the growth counter ([BUG-0001](docs/bugs/2026/06/BUG-0001-instrumented-pool-growth-counter-data-race.md)).**
  `notify_if_grew()` read and wrote the non-atomic `last_growths_` on the allocation
  hot path, while the decorator is documented as safe to drive concurrently over a
  thread-safe pool — a data race (UB) under `MUTEX` + dynamic growth. `last_growths_`
  is now `std::atomic` and advanced with a `compare_exchange`, so the growth event is
  emitted once per growth without a race. A concurrent `InstrumentedPool` case was
  added to the ThreadSanitizer stress suite. Header-only; no API change.
- **`InstrumentedPool::deallocate` no longer underflows `live_` ([BUG-0002](docs/bugs/2026/06/BUG-0002-instrumented-pool-live-counter-underflow.md)).**
  A foreign or double-freed pointer (a no-op in the core, ADR-0012) used to decrement
  the unsigned `live_` counter unconditionally, wrapping it to `SIZE_MAX` and
  corrupting `stats()`. The decrement now clamps at zero. The C header's
  `memory_pool_free` note was corrected to stop claiming the Decorator *detects*
  double-free (it counts but cannot distinguish one). Header/doc-only; no API change.
- **`InstrumentedPool` move-assignment now emits `destroyed` for the replaced pool
  ([BUG-0003](docs/bugs/2026/06/BUG-0003-instrumented-pool-move-assign-missing-destroyed-event.md)).**
  Move-assigning over an instrumented pool released its `Pool` and observers without
  notifying `PoolEvent::destroyed`, asymmetric with the destructor. It now notifies
  before reassignment. Header-only; no API change.
- **Overflow guard in `grow_pool` ([BUG-0004](docs/bugs/2026/06/BUG-0004-grow-pool-growth-size-overflow.md)).**
  The dynamic-growth path computed `total * (grow_factor_ - 1)` before any overflow
  check, so that product could wrap `size_t` and feed the downstream `block_size`
  guard an already-wrapped value. Added a `would_overflow_product` guard on the
  growth-count product first, mirroring the create-path guard; on overflow the pool
  falls back to fixed-mode exhaustion. Latent (not runtime-reachable — RAM exhausts
  first); no API change.

## Released versions

Each released version is an **immutable** entry under [`docs/changelog/`](docs/changelog/) — one file per release, newest first ([ADR-0038](docs/adr/0038-changelog-version-split.md)). They are never edited after release; only the `Unreleased` block above changes during development.

| Version | Date | Highlights |
|---------|------|------------|
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

[Unreleased]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/compare/v1.1.1...HEAD
