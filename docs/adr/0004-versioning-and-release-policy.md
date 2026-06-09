# ADR-0004: Versioning and Release Policy

- **Status:** Accepted
- **Date:** 2026-06-09
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §11, [`docs/workflow/release.md`](../workflow/release.md), [`ROADMAP.md`](../../ROADMAP.md)

## Context

`pbr-cpp-memory-pool` is a reference library that downstream consumers will eventually pin against. As a series project (Purpose-Built References), it must release predictably so siblings and consumers can synchronise on known checkpoints. Until this ADR, the repository carried scattered mentions of versioning — `AGENTS.md` §10 names SemVer, `ROADMAP.md` §7.3 plans a `CHANGELOG.md`, §7.7 reserves the v1.0.0 tag — but never spelled out:

- The pre-1.0 tag cadence and what each tag should mean.
- The status of pre-release suffixes (`-alpha`, `-beta`, `-rc`).
- Where release artifacts come from (auto from GitHub vs. CI-built binaries).
- Which package registries the project will publish to, and when.
- The agent-vs-human boundary for tag creation and release publication — left ambiguous, but the PR-merge boundary in `AGENTS.md` §6.1 implies releases should follow the same pattern.
- The release-PR mechanics: who bumps the version, who writes the changelog entry, who drafts the release notes.

Without a policy, releases would either happen ad-hoc (defeating the "predictable reference" property) or slip until v1.0 (making pre-1.0 progress invisible to anyone outside the repo).

## Decision

We adopt the following Versioning & Release Policy, normative for the project and binding on every PR that touches user-visible behaviour.

### 1. Versioning scheme — SemVer

The project follows [Semantic Versioning 2.0.0](https://semver.org). Version numbers are `MAJOR.MINOR.PATCH`, with optional pre-release suffix `-alpha.N`, `-beta.N`, `-rc.N` (single integer counter, no dotted suffixes).

**Pre-1.0 (`0.MINOR.PATCH`) interpretation, project-specific:**

- `MINOR` increments with each completed roadmap milestone.
- `PATCH` increments for fixes that ship between milestone tags.
- Pre-1.0, breaking changes are allowed in a `MINOR` bump — but only if the breaking change is documented in `CHANGELOG.md` under *Changed* / *Removed*.

**Post-1.0 (`MAJOR.MINOR.PATCH`):**

- `MAJOR` increments for source- or ABI-incompatible changes.
- `MINOR` increments for backwards-compatible additions.
- `PATCH` increments for backwards-compatible fixes.

### 2. Tag cadence

Tags are annotated git tags of the form `v<version>`, e.g., `v0.1.0`, `v0.6.0-rc.1`, `v1.0.0`.

| Tag             | Trigger                                                                                  |
|-----------------|------------------------------------------------------------------------------------------|
| `v0.1.0`        | Milestone 1 (Build System & Project Skeleton) closes.                                    |
| `v0.2.0`        | Milestone 2 (Core Memory Pool — single-threaded MVP) closes.                             |
| `v0.3.0`        | Milestone 3 (C++ Wrapper & Type Safety) closes.                                          |
| `v0.4.0`        | Milestone 4 (Thread-Safe Variant) closes.                                                |
| `v0.5.0`        | Milestone 5 (Dynamic Growth Mode) closes.                                                |
| `v0.6.0`        | Milestone 6 (Observability & Decorators) closes.                                         |
| `v1.0.0`        | Milestone 7 (Release & Polish) closes — full reference release.                          |
| `v<X.Y.Z>-rc.N` | Optional, when a milestone is feature-complete but undergoing final review.              |
| `v<X.Y.(Z+1)>`  | Hotfix between milestone tags — small PR with the fix, CHANGELOG patch entry, new tag.   |

Tags are always cut from `master` after the closing PR is merged. No tags on feature branches.

### 3. CHANGELOG

`CHANGELOG.md` follows the [Keep a Changelog 1.1.0](https://keepachangelog.com) format, with the canonical sections *Added*, *Changed*, *Deprecated*, *Removed*, *Fixed*, *Security*. The file lives at the repo root.

- An `Unreleased` section sits at the top and accumulates entries during development.
- Every PR that introduces user-visible change adds a line to the relevant subsection of `Unreleased` in the same PR — never as a follow-up.
- When a tag is cut, the `Unreleased` block is renamed to the version with an ISO date (e.g., `## [0.1.0] — 2026-XX-XX`) and a fresh empty `Unreleased` is created at the top.

The file is introduced in Milestone 1 (since v0.1.0 needs it) and superseded by no later ADR.

### 4. Release artifacts

Every annotated tag of the form `v<version>` triggers a `release` CI workflow that:

1. Re-runs the full test matrix (the same gates as a normal PR — see `AGENTS.md` §10).
2. Builds the library on the supported platforms (Linux x86_64, Windows x86_64, macOS arm64) — once Milestone 1.2 (CMake) and 1.8 (CI) exist; for earlier milestone tags the workflow degrades gracefully.
3. Produces source and binary artifacts:
   - Source: GitHub-auto-generated `.zip` and `.tar.gz` (always).
   - Binaries (post-v0.1.0): per-platform static and shared library + headers, packaged as `pbr-memory-pool-<version>-<platform>.tar.gz`.
   - `SHA256SUMS` for every artifact.
4. Creates a GitHub Release whose body is the corresponding section of `CHANGELOG.md` plus a *What's Changed* link to the milestone in `ROADMAP.md`.
5. Attaches all artifacts and checksums to the release.

The workflow itself is added in Milestone 1.12 (alongside the build system) so the first taggable milestone is also the first that ships artifacts.

### 5. Distribution

Distribution proceeds in two phases:

- **Phase 1 — CMake `find_package` support (Milestone 7.4).** The library exports a CMake config file so consumers can `find_package(pbr_memory_pool CONFIG REQUIRED)` after vendoring via `FetchContent` or installing the artifacts from a GitHub Release.
- **Phase 2 — Package registries (post-v1.0 stretch, new Milestone 7.8/7.9).** vcpkg port and Conan recipe. Both depend on a stable 1.0+ API so they are gated behind v1.0.0.

No registry publishing happens before v1.0.0 — the artifact path is GitHub Releases only during 0.x.

### 6. Agent-vs-human boundary

Releases follow the same agent-vs-human pattern already in force for pull requests (`AGENTS.md` §6.1):

| Action                                                | Who does it      |
|-------------------------------------------------------|------------------|
| Bump version constant in source                       | Agent            |
| Update `CHANGELOG.md` (roll Unreleased into version)  | Agent            |
| Draft GitHub Release notes                            | Agent            |
| Open the release PR                                   | **Human**        |
| Merge the release PR                                  | **Human**        |
| Create the annotated git tag (`git tag -a v<X.Y.Z>`)  | **Human**        |
| Push the tag (`git push origin v<X.Y.Z>`)             | **Human**        |
| Publish the GitHub Release (button on the web UI)     | **Human**        |
| Build & attach release artifacts                      | CI (automated)   |

Agents **never** push tags, **never** publish releases, **never** cut release branches.

## Alternatives Considered

- **CalVer (`YYYY.MM`) versioning.** Rejected because consumer libraries expect SemVer; CalVer makes API-stability semantics harder to communicate.
- **Tag only at v1.0.0 and beyond — no pre-1.0 tags.** Rejected because pre-1.0 progress would be invisible to anyone watching the repo and consumers could not pin against intermediate states.
- **Tag every PR merge automatically.** Rejected because most PRs are incremental and don't represent a meaningful release boundary. The per-milestone cadence is denser than v1.0-only but sparser than per-PR.
- **Auto-publish releases on tag push.** Adopted in part — CI builds artifacts and creates the GitHub Release draft on tag push, but the final *Publish* button stays with the human (matches §6 above).
- **Publish to vcpkg / Conan immediately, even pre-1.0.** Rejected — those registries expect stable APIs, and frequent breaking changes pre-1.0 would burn consumer trust. Defer to post-1.0.
- **Maintain release branches (`release/0.x`).** Rejected for now — single-maintainer project, no need for parallel maintenance lines. Reconsider only if a major-version split appears.

## Consequences

**Positive**

- Predictable per-milestone tag cadence makes progress externally legible.
- A taggable artifact at every milestone closure forces the project to keep `master` in a releasable state at the milestone boundary.
- `CHANGELOG.md` becomes a first-class artifact, updated in the same PR as the change it describes — same rule as the rest of the docs (`AGENTS.md` §7).
- The agent-vs-human boundary stays consistent across PR merges and releases — no surprising delegation.
- Release CI removes a class of manual errors (forgetting checksums, missing artifacts for a platform).

**Negative**

- Per-milestone ceremony cost: each closing PR carries CHANGELOG roll-up + release-notes drafting + version bump. Mitigated by template (in `docs/workflow/release.md`) and by deferring registry publishing to v1.0+.
- A failed milestone close (e.g., a regression discovered the day after tagging) requires either a `vX.Y.(Z+1)` hotfix or, in extreme cases, a `git tag -d` + repush — the latter is mildly disruptive to consumers but unlikely pre-v0.6.

## References

- [Semantic Versioning 2.0.0](https://semver.org)
- [Keep a Changelog 1.1.0](https://keepachangelog.com)
- [`AGENTS.md`](../../AGENTS.md) §11 — normative summary of this policy.
- [`docs/workflow/release.md`](../workflow/release.md) — operational guide for the release process.
- [`ROADMAP.md`](../../ROADMAP.md) — per-milestone close items include the tag and CHANGELOG steps.
