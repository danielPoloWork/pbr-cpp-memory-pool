# ADR-0034: Post-release maintenance protocol

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0004](0004-versioning-and-release-policy.md) (versioning & release policy this builds on), [ADR-0008](0008-delegate-tag-creation-and-push-to-the-agent.md) (tag delegation), [`AGENTS.md`](../../AGENTS.md) §11 (versioning & release / agent-vs-human boundary), [`docs/workflow/release.md`](../workflow/release.md) (the mechanical release steps), [`docs/workflow/maintenance.md`](../workflow/maintenance.md) (the operational protocol this ADR records), [ROADMAP](../../ROADMAP.md) §8.5 (the item).

## Context

With `v1.0.0` the project entered the **maintained-product phase**: the public API is frozen under SemVer, and from here on most work is fixes, security responses, additive features, and the eventual deprecation/removal of surface — not greenfield milestones. [ADR-0004](0004-versioning-and-release-policy.md) defined the *pre-1.0* cadence in detail (a MINOR per milestone) and stated the post-1.0 rule in one line ("standard SemVer"), but it did not spell out the **governance** a maintained product needs: how to *decide* a release is a patch vs a minor vs a major, how an urgent fix reaches users when `master` has moved on, how security issues are handled, and how API is retired without breaking consumers. The `v1.0.1` packaging patch already exercised the patch path ad hoc; before more post-1.0 releases accrue, that governance should be written down once so every future fix follows the same rules. ROADMAP §8.5 is that work.

## Decision

We adopt a **post-release maintenance protocol**, recorded operationally in [`docs/workflow/maintenance.md`](../workflow/maintenance.md), with these load-bearing decisions:

1. **The version-protected surface is named explicitly.** SemVer for this project protects the C ABI, the C++ public types, the compile-time knobs, and the CMake imported-target/package name — so "is this breaking?" has a concrete referent, not a vague "the API".

2. **A three-question decision tree fixes the SemVer level.** (a) Does it break existing consumer code (remove/rename/alter a public symbol, knob, or target)? → **MAJOR**. (b) Does it add backward-compatibly (new surface, new opt-in, a milestone's additive work, or a deprecation)? → **MINOR**. (c) Otherwise (fix / docs / packaging / perf, no public-API change)? → **PATCH**. Ambiguous changes round **up**, because an under-numbered release is the one that betrays consumers who trusted SemVer.

3. **Mechanics are unified with the milestone-close flow.** A patch/minor/major release uses the exact same steps as a milestone close ([`release.md`](../workflow/release.md)); only which component of `version.hpp` moves differs. No second release procedure.

4. **Hotfixes branch by releasability.** If `master` is releasable (the normal state — `master` *is* the release line), fix on `master` and cut the next PATCH. If `master` carries unreleased not-yet-shippable work, branch the hotfix **from the released tag**, cut the PATCH there, and **forward-port to `master`** (mandatory, same task). A hotfix is the minimal change only.

5. **Security fixes are private-first and `Security`-categorized.** Report via GitHub private vulnerability reporting (a `SECURITY.md` is a planned addition), fix under embargo, release coordinated with the advisory, and record under the Keep-a-Changelog `Security` category.

6. **Deprecation precedes removal.** Public API is retired by deprecating in a MINOR (`@deprecated` + `Deprecated` changelog line + an ADR; the symbol keeps working), honouring a window of at least the rest of the MAJOR line, then removing in the next MAJOR. A deprecation itself ships in a MINOR (it is backward-compatible).

The agent-vs-human release boundary and the tag delegation are **unchanged** — this protocol layers governance on top of [ADR-0004](0004-versioning-and-release-policy.md) / [ADR-0008](0008-delegate-tag-creation-and-push-to-the-agent.md), it does not amend them.

## Alternatives Considered

- **No written protocol — decide each release ad hoc.** Rejected: the maintained phase is exactly where consistency matters (consumers rely on the version number's meaning), and a single maintainer's ad-hoc calls drift over time. The `v1.0.1` patch showed the questions recur; answering them once is cheap.
- **Trunk-only, no hotfix-from-tag path.** Rejected: it works while `master` is always releasable, but the moment `master` holds unreleased breaking work, an urgent fix would be unshippable without either shipping the unfinished work or reverting it. The hotfix-from-tag + forward-port path is the standard escape and costs nothing until needed.
- **Fold everything into `release.md`.** Rejected: `release.md` is the *mechanical* runbook (the keystrokes); mixing the *governance* (which level, when, deprecation policy, security) into it would bloat the runbook and bury the decision rules. Two documents, cross-linked, each with one job — mirroring the ADR-vs-runbook split already used elsewhere.
- **A heavyweight branching model (release branches per minor, LTS lines).** Rejected as premature for a single-maintainer reference library: `master` as the release line plus the hotfix-from-tag escape covers the realistic cases without a permanent multi-branch maintenance burden. A future ADR can introduce release branches if real demand (multiple supported majors) appears.
- **Define security handling only if/when needed.** Rejected: a documented private-reporting + embargo + `Security`-changelog path is table stakes for anything calling itself production-quality, and writing it before the first report is the point.

## Consequences

**Positive**

- Every post-1.0 release has a deterministic, recorded answer to "what version number, and how does the fix flow?" — the version number stays trustworthy.
- The decision tree + "round up when ambiguous" rule protects consumers from silent SemVer violations.
- The hotfix/backport and deprecation paths are defined *before* they are first needed, so the first security report or first API retirement is executed, not improvised.
- No new release mechanics to learn — it reuses `release.md` and the existing agent/human boundary.

**Negative / limitations**

- It is a single-maintainer-scale protocol: no parallel release branches / LTS lines (deliberately — that complexity is deferred until real demand, and would need its own ADR).
- A `SECURITY.md` is referenced but not yet added; until it exists, security reporting relies on GitHub's private-advisory feature. (A small follow-up.)
- The protocol is process, enforced by review and (from §8.6) the consistency lint — not by tooling that can *prevent* a wrong version bump. The M8.6 lint checks version-constant lockstep, which catches the most common mechanical error.

**Documentation updates landing in the same PR**

- [`docs/workflow/maintenance.md`](../workflow/maintenance.md) — the protocol itself (new).
- [`docs/adr/README.md`](README.md) — index row for ADR-0034.
- [ROADMAP](../../ROADMAP.md) §8.5 — checkbox flipped.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry.
- A pointer to `maintenance.md` from the workflow guides / README repository-layout is added for discoverability.

## References

- [ADR-0004](0004-versioning-and-release-policy.md), [ADR-0008](0008-delegate-tag-creation-and-push-to-the-agent.md) — the policies this builds on.
- [`docs/workflow/maintenance.md`](../workflow/maintenance.md), [`docs/workflow/release.md`](../workflow/release.md) — the operational protocol and the mechanical runbook.
- [Semantic Versioning 2.0.0](https://semver.org/), [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).
