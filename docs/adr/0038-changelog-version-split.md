# ADR-0038: Split the changelog into one immutable Markdown file per release

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Project architect (maintainer), agent
- **Related:** [ADR-0004](0004-versioning-and-release-policy.md) (refines §3 — the CHANGELOG mechanics), [ADR-0036](0036-session-journal-extraction.md) / [ADR-0037](0037-new-feature-roadmap-placement.md) (companion docs-structure decisions), [`CHANGELOG.md`](../../CHANGELOG.md), [`docs/workflow/release.md`](../workflow/release.md), [`tools/consistency_lint.py`](../../tools/consistency_lint.py)

## Context

`CHANGELOG.md` had grown to **1129 lines** across ten version blocks (`0.1.0`–`1.1.0` plus `Unreleased`) and only ever grows. As with the session journal ([ADR-0036](0036-session-journal-extraction.md)), a single ever-growing file is a scaling smell for a long-lived product.

Two scaling models were weighed against the constraint that the changelog is a **standardized, tool-facing artifact** (Keep a Changelog 1.1.0; [ADR-0004](0004-versioning-and-release-policy.md) §3), not a free-form log:

- A **calendar split** (folders by year/month, like the journal). Rejected: a changelog is keyed by **version, not date** — releases do not align to months, so `v1.0.0` / `v1.0.1` / `v1.1.0` would all land in one `2026/06` bucket, a meaningless grouping. The journal is date-keyed (events); the changelog is version-keyed (releases).
- An **executable/XML migration model** (Liquibase-style `<databaseChangeLog>` with `<include>`, `<rollback>`, `context`, `preconditions`). Rejected: that is the right tool for **database schema migrations** — executable changesets with real rollback SQL — and has no referent in a header/static-library memory pool with no database. Its sound enterprise principles (immutability, one file per release, ordered master, atomic changes, audit trail, rollback points) are already satisfied here by Markdown + git: released entries are immutable, Conventional Commits give atomic units, and annotated signed tags ([ADR-0008](0008-delegate-tag-creation-and-push-to-the-agent.md)) are the audit trail and rollback points (`git revert` + a PATCH release).

## Decision

We keep **Keep a Changelog (Markdown)** and split it **by version**: each released version is an **immutable** file `docs/changelog/v<MAJOR>/v<X.Y.Z>.md` containing that release's dated block and its own `[X.Y.Z]:` link definition. The root **`CHANGELOG.md` remains the canonical file** but carries only the preamble, the live `[Unreleased]` block, and a *Released versions* **index** table (newest first) linking to the per-version files.

On release, the `Unreleased` entries are **moved** into a new per-version file (never edited in place afterwards), the index gains a row, and the `[Unreleased]` compare link is advanced ([`release.md`](../workflow/release.md) §3). The `version-lockstep` consistency check now reads the latest version from the per-version files rather than from a dated block in the root.

## Alternatives Considered

- **Leave it as one file.** Rejected for the same unbounded-growth reason that motivated [ADR-0036](0036-session-journal-extraction.md); 1129 lines was already two-thirds historical.
- **Calendar split (year/month).** Rejected — wrong axis for a version-keyed artifact (see Context).
- **XML / Liquibase migration changelog.** Rejected — right pattern, wrong domain: it is for executable DB migrations, not a human release changelog; it would also break the Keep a Changelog convention and the Markdown-section extraction in `release.yml` (see Context).
- **One file per *feature* within a release** (Liquibase's large-team option). Rejected as premature: one file per release matches our one-PR-per-item cadence and keeps the entry count low; revisit only if a release ever accretes many parallel feature streams.

## Consequences

- **No API/ABI/build impact** — documentation/tooling only.
- Root `CHANGELOG.md` drops from 1129 to ~100 lines and reads as "what's pending + an index"; the nine historical entries moved **verbatim** (content integrity verified block-by-block) into `docs/changelog/`.
- The Keep a Changelog convention and the root-file location ([ADR-0004](0004-versioning-and-release-policy.md) §3) are preserved; this ADR **refines** ADR-0004's "roll Unreleased into a dated block in the same file" mechanic into "move it into a new per-version file + index row" — captured in [`release.md`](../workflow/release.md) §3.
- `tools/consistency_lint.py` `version-lockstep` now globs `docs/changelog/**/vX.Y.Z.md` for the latest version (mirroring its existing `docs/releases/` scan) instead of matching the topmost `## [X.Y.Z]` block.
- Each per-version file is self-contained (own link definition) so its `## [X.Y.Z]` reference link resolves and markdownlint stays clean.
- Completes the docs-structure trilogy with [ADR-0036](0036-session-journal-extraction.md) (journal out of ROADMAP) and [ADR-0037](0037-new-feature-roadmap-placement.md) (new-feature placement): plan, journal, and changelog each scale on their natural axis.

## References

- [ADR-0004](0004-versioning-and-release-policy.md) §3 — the CHANGELOG policy this refines.
- [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/) · [Semantic Versioning 2.0.0](https://semver.org/).
- [`docs/workflow/release.md`](../workflow/release.md) §3 — the updated roll-over mechanic.
- [ADR-0036](0036-session-journal-extraction.md) / [ADR-0037](0037-new-feature-roadmap-placement.md) — companion docs-structure decisions.
