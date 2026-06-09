# Release Workflow

Operational guide for cutting a release of `pbr-cpp-memory-pool`. The policy that governs this workflow is [ADR-0004](../adr/0004-versioning-and-release-policy.md); the high-level summary lives in [`AGENTS.md`](../../AGENTS.md) §11.

## When to release

A new tag is cut whenever **a milestone closes** (per ADR-0004 §2) or when a hotfix lands between milestones.

| Trigger                                  | Tag form         | Initiator       |
|------------------------------------------|------------------|-----------------|
| Milestone N closes (N = 1..6)            | `v0.N.0`         | Maintainer      |
| Milestone 7 closes (full release)        | `v1.0.0`         | Maintainer      |
| Milestone feature-complete, under review | `v0.N.0-rc.M`    | Maintainer      |
| Hotfix between milestones                | `v<X.Y.(Z+1)>`   | Maintainer      |

Agents never initiate tags. Agents *prepare* the release PR; the human merges it and creates the tag.

## Boundary recap

The agent-vs-human boundary mirrors the PR-merge boundary in [`AGENTS.md`](../../AGENTS.md) §6.1:

| Action                                                | Who      |
|-------------------------------------------------------|----------|
| Bump version constant in source                       | Agent    |
| Update `CHANGELOG.md` (Unreleased → version)          | Agent    |
| Draft GitHub Release notes                            | Agent    |
| Open the release PR                                   | Maintainer |
| Merge the release PR                                  | Maintainer |
| Create annotated git tag                              | Maintainer |
| Push the tag                                          | Maintainer |
| Publish the GitHub Release                            | Maintainer |
| Build & attach release artifacts                      | CI       |

## Step-by-step — milestone close

The flow below assumes Milestone 1 is closing, producing `v0.1.0`. Adjust the version number for other milestones.

### 1. Open the release-prep branch (agent)

```
git switch -c release/v0.1.0
```

Branch naming for releases: `release/v<X.Y.Z>`. The `release/` prefix joins the existing branch-type vocabulary (`feat/`, `fix/`, `docs/`, …) — it carries the same one-logical-change-per-commit rule.

### 2. Bump the version constant (agent)

A single source of truth for the project version lives in `src/main/cpp/it/d4np/memorypool/version.hpp` (added in Milestone 1.6). It exposes:

```cpp
namespace it::d4np::memorypool {
inline constexpr unsigned PBR_MEMORY_POOL_VERSION_MAJOR = 0;
inline constexpr unsigned PBR_MEMORY_POOL_VERSION_MINOR = 1;
inline constexpr unsigned PBR_MEMORY_POOL_VERSION_PATCH = 0;
inline constexpr const char* PBR_MEMORY_POOL_VERSION_STRING = "0.1.0";
}  // namespace it::d4np::memorypool
```

`CMakeLists.txt` reads these constants when configuring the project (`project(pbr_memory_pool VERSION ...)`); never edit the version in CMake directly.

Bump the three integers and the string to the target version. Commit:

```
git commit -m "chore(release): bump version to 0.1.0"
```

### 3. Roll over `CHANGELOG.md` (agent)

`CHANGELOG.md` follows [Keep a Changelog](https://keepachangelog.com). The `Unreleased` section at the top is renamed to the target version with an ISO date, and a fresh empty `Unreleased` section is inserted above it:

```markdown
## [Unreleased]

## [0.1.0] — YYYY-MM-DD

### Added
- ...

### Changed
- ...
```

Commit:

```
git commit -m "docs(changelog): roll Unreleased into 0.1.0"
```

### 4. Draft the GitHub Release notes (agent)

Add a `docs/releases/v0.1.0.md` file containing the release notes that will be posted to GitHub. The body should:

- Lead with the milestone's headline outcome (one sentence).
- Summarise the *Added / Changed / Fixed* lines from `CHANGELOG.md` in human-readable prose, grouped by theme rather than by category.
- Link to the closing PR and to the roadmap milestone.
- Note which Spec Coverage Map rows flipped to ✅ in this release.
- Call out any known limitations or deferred work.

Commit:

```
git commit -m "docs(release): draft v0.1.0 release notes"
```

### 5. Push and open the release PR (agent → maintainer)

```
git push -u origin release/v0.1.0
```

PR title: `chore(release): v0.1.0`. PR body: the standard template (`.github/PULL_REQUEST_TEMPLATE.md`), with the *Summary* section pointing to `docs/releases/v0.1.0.md` and the *Documentation Impact* checkboxes covering CHANGELOG + release notes + version bump.

**Agent stops here.** The maintainer reviews the PR.

### 6. Merge the release PR (maintainer)

Squash-and-merge, same as any other PR. The merge commit's body becomes the durable `git log` entry; the user-facing release notes live in the GitHub Release created in §8.

### 7. Tag (maintainer)

```
git fetch origin
git checkout master
git pull --ff-only
git tag -a v0.1.0 -m "v0.1.0 — <milestone headline>"
git push origin v0.1.0
```

Tag messages mirror the release-notes headline. Annotated tags only — never lightweight tags (they don't carry a message).

### 8. Publish the GitHub Release (maintainer + CI)

The tag push triggers the `release` CI workflow (added in Milestone 1.12). The workflow:

1. Runs the full test matrix.
2. Builds per-platform artifacts (Linux x86_64, Windows x86_64, macOS arm64) — degrades gracefully if a platform is not yet set up.
3. Creates a **draft** GitHub Release with the body copied from `docs/releases/v0.1.0.md`, attaches the source and binary artifacts plus `SHA256SUMS`.
4. Stops in *Draft* state.

The maintainer reviews the draft release, edits notes if needed, and clicks *Publish*. CI does not auto-publish — the human keeps the final say (mirrors the PR-merge boundary).

## Hotfix flow

A defect found between milestone tags follows a compressed version of the above:

1. `git switch -c fix/<short-name>` from `master`.
2. Fix the bug; add tests.
3. Add a `Fixed` line to `CHANGELOG.md` under `Unreleased`.
4. Open the PR — normal flow.
5. After merge, the maintainer decides whether to cut a patch tag (`v0.1.1`) immediately or batch it with the next milestone tag.
6. If a patch tag is cut, repeat §3–§8 with the patch version.

## Pre-release suffixes

`-alpha.N`, `-beta.N`, `-rc.N` follow SemVer 2.0 ordering: `1.0.0-alpha.1 < 1.0.0-beta.1 < 1.0.0-rc.1 < 1.0.0`. Use them when:

- The milestone is feature-complete but undergoing soak testing → `-rc.N`.
- Major work is partially shippable but known-incomplete → `-alpha.N` / `-beta.N` (rare in this project).

Pre-release tags trigger the same CI workflow as final tags, but the GitHub Release is automatically marked *pre-release*.

## What does *not* belong in this workflow

- Pushing to package registries (vcpkg, Conan) — gated behind v1.0.0 per ADR-0004 §5.
- Maintaining release branches — single-maintainer project; reconsider only if a parallel major-version line emerges.
- Auto-publishing releases without human review — explicitly rejected by ADR-0004 §6.
