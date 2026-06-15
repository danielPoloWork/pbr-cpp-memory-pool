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

- **Pull-request metadata policy.** Every agent-opened PR now also sets a maintainer
  **assignee**, **exactly one type label** (Conventional-Commit `type` → label;
  `docs` reuses the built-in `documentation` label), and the current open **release
  milestone** (per-release scheme, e.g. `v1.1.1`). The eight type labels
  (`feat`/`fix`/`refactor`/`perf`/`test`/`build`/`chore`/`ci`) and the `v1.1.1`
  milestone were created, and PRs #89–#91 backfilled. **Reviewers** are deferred
  (the sole collaborator is the PR author — GitHub forbids self-review) and
  **Projects** are deferred (the `gh` token lacks the `project` scope); the rule names
  how each switches on. Codified in [`AGENTS.md`](AGENTS.md) §6.4, rationale in
  [ADR-0040](docs/adr/0040-pull-request-metadata-policy.md). Process/repository-metadata
  only; no API change.
- **In-repo bug ledger (`docs/bugs/`) + agent triage protocol.** Known defects and
  the triage of incoming reports now have a durable, reviewable home: one Markdown
  record per defect, `BUG-NNNN-<slug>.md` under a discovery-date tree
  `docs/bugs/<YYYY>/<MM>/`, with a stable monotonic id, structured frontmatter
  (`status`/`severity`/`reporter`/…), an index + template, and a lifecycle
  (`open → confirmed → fixed`, plus `wontfix`/`duplicate`/`cannot-reproduce`). The
  ledger is the source of truth (a GitHub issue is referenced, not authoritative) and
  cross-references the `CHANGELOG` `Fixed` line at close. The agent rule
  ([`AGENTS.md`](AGENTS.md) §7.7) requires a record only for **verified** defects, and
  **verification before acceptance** of third-party reports (unsubstantiated reports
  are still recorded as `cannot-reproduce`/`rejected`). A new `bugs` consistency-lint
  check guards frontmatter, ids, the index bijection, and the `fixed`↔`fixed-in` link.
  Governance in [`docs/workflow/maintenance.md`](docs/workflow/maintenance.md);
  rationale in [ADR-0039](docs/adr/0039-bug-ledger-and-triage-protocol.md).
  Documentation/process/tooling-only; no API change.
- README gains a **Technology stack** section (language standards, build / test /
  docs / tooling, and packaging, with versions — `zero runtime dependencies`) and a
  top-of-page **"Read this in: 简体中文 · 日本語"** pointer to the `docs/i18n/`
  translations. Documentation-only; no API change. (Post-1.0 maintenance; the
  `zh-Hans` / `ja` README translations are re-synced to match in a follow-up PR.)
- **`SECURITY.md`** — the public security policy: supported versions (`1.1.x`),
  private vulnerability reporting via GitHub's advisory feature, the
  coordinated-disclosure expectations, and the in-scope/out-of-scope boundary.
  Realizes the planned addition referenced by the post-release maintenance
  protocol ([`docs/workflow/maintenance.md`](docs/workflow/maintenance.md), which
  now links it).
- **`packaging-smoke` CI workflow** — end-to-end smoke tests for the Phase-2
  packaging recipes that cannot be built on the maintainer's box: a **vcpkg**
  overlay-port install + consumer build (`find_package` + link `pbr::memory_pool`)
  and a **Conan** `conan create` that runs the `test_package`. Both validate the
  recipes against the SHA-pinned `v1.0.0` source tag, so they run on changes to
  `ports/**` / `conan/**` / the workflow and on a weekly schedule (the recipes are
  version-pinned, so the schedule is the main signal for toolchain / registry
  drift). Adds a small `ci/packaging-smoke/vcpkg-consumer/` fixture; the Conan side
  reuses the existing `conan/test_package/`. CI only; no API change.
- **API reference badge** in the README header linking directly to the published
  Doxygen site (`https://danielpolowork.github.io/pbr-cpp-memory-pool/`), so the
  generated API reference is reachable in one click rather than only from prose.
  Mirrored into the `zh-Hans` / `ja` README translations. Documentation-only.
- **`docs/journal/` session journal** — the dated end-of-session checkpoints were
  extracted from `ROADMAP.md` into one file per session under `YYYY/MM/`, with an
  index and a `ROADMAP.md` *Latest checkpoint* pointer ([ADR-0036](docs/adr/0036-session-journal-extraction.md);
  agent rule in [`AGENTS.md`](AGENTS.md) §7.6). `ROADMAP.md` drops from 301 to ~171
  lines and reads as a forward plan again. Documentation-only; the ten historical
  checkpoints were migrated verbatim.
- **New-feature roadmap-placement rule** — [`AGENTS.md`](AGENTS.md) §7.3 now states
  that every feature reaches the roadmap as part of the PR that starts it, and gives
  the judgment for *new milestone* (cohesive capability → `Milestone 9`, `10`, …;
  closes as a MINOR) vs. *appended item* (extends an open milestone) vs. *neither*
  (a maintenance change). Recorded in [ADR-0037](docs/adr/0037-new-feature-roadmap-placement.md)
  and cross-referenced from [`docs/workflow/maintenance.md`](docs/workflow/maintenance.md).
  Documentation/process-only.

### Changed

- **`zh-Hans` / `ja` README translations re-synced to `v1.1.0`.** Carries the
  English README's post-1.0 deltas into both locales — `v1.1.0` status badge and
  banner, the cross-language "read this in" links, the sharpened project
  description, the `v1.1.0` status paragraph, Milestone 8 → complete, and a
  translated **Technology stack** section. The `translation-status.md` manifest's
  two README rows are re-pinned to the current source commit and flipped from
  `stale` back to `translated`, clearing the `i18n-freshness` consistency-lint
  flag the `v1.1.0` release raised. Documentation-only; no API change.
- **CI GitHub Actions bumped off the deprecated Node 20 runtime onto Node 24.**
  `actions/checkout` v4 → v6, `actions/upload-artifact` v4 → v7,
  `actions/download-artifact` v4 → v8, `actions/deploy-pages` v4 → v5,
  `actions/upload-pages-artifact` v3 → v5, and
  `DavidAnson/markdownlint-cli2-action` v16 → v23 — all of which previously ran on
  `node20`, which GitHub is retiring. The markdownlint bump pulls in a newer
  markdownlint that adds the `MD060` (table-column-style) rule; it is disabled in
  [`.markdownlint.json`](.markdownlint.json) because byte-aligned table pipes are
  not achievable for the CJK-width tables in the `docs/i18n/` translations —
  preserving the prior (rule-absent) behaviour. `ilammy/msvc-dev-cmd` stays on `v1`
  (no Node 24 release exists upstream yet); `lukka/get-cmake@latest` and
  `lycheeverse/lychee-action@v2` already run on Node 24 / as composite actions.
  CI / build only; no API/ABI/behaviour change.
- **Changelog split into one immutable file per release.** The nine historical
  entries (`0.1.0`–`1.1.0`) moved **verbatim** out of this file into
  `docs/changelog/v<MAJOR>/v<X.Y.Z>.md`; the root `CHANGELOG.md` now keeps only the
  preamble, `[Unreleased]`, and the *Released versions* index below — dropping from
  1129 to ~100 lines. Keep a Changelog and the root-file location are preserved;
  the `version-lockstep` consistency check, [`release.md`](docs/workflow/release.md)
  §3, and the `AGENTS.md` §11 release contract were updated to match. Rationale
  (incl. why not a calendar or XML/Liquibase split) in
  [ADR-0038](docs/adr/0038-changelog-version-split.md). Documentation/tooling-only.

## Released versions

Each released version is an **immutable** entry under [`docs/changelog/`](docs/changelog/) — one file per release, newest first ([ADR-0038](docs/adr/0038-changelog-version-split.md)). They are never edited after release; only the `Unreleased` block above changes during development.

| Version | Date | Highlights |
|---------|------|------------|
| [1.1.0](docs/changelog/v1/v1.1.0.md) | 2026-06-14 | Internationalization (zh-Hans / ja) & post-release governance |
| [1.0.1](docs/changelog/v1/v1.0.1.md) | 2026-06-14 | Packaging patch — vcpkg port + Conan recipe (byte-identical lib) |
| [1.0.0](docs/changelog/v1/v1.0.0.md) | 2026-06-14 | First stable release — public C ABI and C++ surface frozen |
| [0.6.0](docs/changelog/v0/v0.6.0.md) | 2026-06-14 | Observability & Decorators (InstrumentedPool, PoolObserver) |
| [0.5.0](docs/changelog/v0/v0.5.0.md) | 2026-06-13 | Dynamic growth mode (Composite overflow chunks) |
| [0.4.0](docs/changelog/v0/v0.4.0.md) | 2026-06-13 | Thread-safe variant (compile-time Strategy: none / mutex / lock-free) |
| [0.3.0](docs/changelog/v0/v0.3.0.md) | 2026-06-13 | C++ wrapper & type safety (TypedPool, PoolAllocator, diagnostics) |
| [0.2.0](docs/changelog/v0/v0.2.0.md) | 2026-06-11 | Core memory pool — single-threaded O(1) MVP |
| [0.1.0](docs/changelog/v0/v0.1.0.md) | 2026-06-10 | Build system & project skeleton |

[Unreleased]: https://github.com/danielPoloWork/pbr-cpp-memory-pool/compare/v1.1.0...HEAD
