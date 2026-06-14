# ADR-0035: Agent-runnable consistency lint

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0034](0034-post-release-maintenance-protocol.md) (the maintenance protocol whose invariants this enforces), [ADR-0004](0004-versioning-and-release-policy.md) (version constants in lockstep), [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) / [ADR-0003](0003-design-patterns-policy.md) (the catalogue this checks), [ADR-0032](0032-documentation-i18n-architecture.md) (the i18n manifest this checks for staleness), [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 (the zero-external-dependency posture extended here to tooling), [`docs/workflow/documentation.md`](../workflow/documentation.md), [ROADMAP](../../ROADMAP.md) §8.6 (the item) / §8.7 (wiring it into the agent contract).

## Context

The project's congruence is, today, maintained by hand and by review: the version constant in `version.hpp` must match the CHANGELOG, the README badge, and the newest release notes; every ADR must be indexed; every catalogued pattern must have a real ADR and a real code location; the Spec Coverage Map must not dangle; an i18n translation must not silently lag its English source; the README milestone table must agree with the ROADMAP checkboxes. These invariants span Markdown, C++ headers, and git history, and they are exactly the things that drift during fast-moving multi-PR sessions — as this session demonstrated (a version badge typo, an i18n source page changing under a translation, a milestone marked complete with an item still unchecked are all *plausible* and *invisible* to the existing gates). The existing `docs.yml` checks markdown lint, internal links, and ADR numbering — but nothing checks cross-artifact congruence. ROADMAP §8.6 asks for a dependency-free, agent-runnable checker that does, wired into CI, exiting non-zero with an actionable report.

## Decision

We add **`tools/consistency_lint.py`** — a **Python 3 standard-library-only** checker (no `pip`, no external package: "dependency-free" in the spec §3.3 sense, applied to tooling) — runnable as `python tools/consistency_lint.py`, and wired into a new **`consistency` job in [`docs.yml`](../../.github/workflows/docs.yml)** (with a full-history checkout for the git-based freshness check). It runs all checks, then prints every failure with an actionable message and exits non-zero if any. The **post-release congruence contract** is the set of six checks:

1. **Version lockstep** — `version.hpp`'s `STRING` equals its `MAJOR.MINOR.PATCH`, and equals the README `Status-vX.Y.Z` badge, the topmost dated `## [X.Y.Z]` CHANGELOG block, and the newest `docs/releases/vX.Y.Z.md`.
2. **ADR index bijection** — every `docs/adr/NNNN-*.md` is listed in the index and every indexed `NNNN-*.md` exists; numbering is sequential with no gap.
3. **Patterns → ADR + code** — every Adopted/Planned catalogue row cites an ADR file that exists *and* a `src/main/cpp/` path that exists.
4. **Spec Coverage Map** — no dangling row: each `| §…` row has a recognised status glyph (⏳/🚧/✅/❎) and a non-empty roadmap-items cell.
5. **i18n freshness** — for each `translated` manifest row, `git log <recorded-commit>..HEAD -- <source>` is empty (the recorded source commit is the source's latest); a non-empty result is a stale translation.
6. **Milestone consistency** — every milestone the README marks ✅ has all its ROADMAP items checked, and no ROADMAP checkbox is malformed.

Language and placement rationale:

- **Python 3, stdlib only.** The checks parse several formats and consult git; a pure-bash implementation would be write-only and brittle. Python 3 is present on every CI runner and the maintainer's box, needs no `pip`, and keeps the script readable and locally runnable — so the "dependency-free" and "agent-runnable" requirements both hold. (It is *tooling*, not part of the library build graph, so it does not affect spec §3.3's runtime/build zero-dependency guarantee.)
- **In `docs.yml`, extending the doc gates.** ROADMAP §8.6 places it alongside the ADR-sanity check; its trigger paths are broadened to include `tools/**` and `version.hpp`. The `consistency` job checks out with `fetch-depth: 0` because the freshness check needs real history.
- **All-checks-then-report, not fail-fast.** A contributor fixing congruence wants the whole list at once.

## Alternatives Considered

- **Pure-bash check (extend the existing `adr-sanity` script).** Rejected: parsing version constants, the patterns table, the spec map, and especially comparing git commits across the i18n manifest is far past what bash does maintainably; it would be unreadable and a magnet for quoting bugs. The ADR-sanity bash stays for its narrow job; the cross-artifact lint is Python.
- **No lint — keep relying on review.** Rejected: this session alone produced several near-misses (a badge typo, a direct-to-master push, a frozen-spec language change that broke ADR anchors) that a mechanical congruence gate would catch instantly. Review does not scale to every invariant on every PR.
- **A third-party doc/lint framework (Vale, a custom pre-commit suite, etc.).** Rejected: an external dependency for a job a 250-line stdlib script does, against the project's zero-dependency ethos.
- **A git pre-commit hook only (no CI).** Rejected: hooks are opt-in and bypassable; the authoritative gate must be in CI. (A hook could call the same script later, additively.)
- **Check exact pattern *symbols* (not just files).** Considered: the M7.5 audit grepped for each symbol. Rejected for the lint: symbol-grep is fragile (rename a class and the lint passes if the file still exists), and the higher-value, robust check is "the cited ADR and the cited file both exist." Symbol-level verification stays a periodic human audit (M7.5).

## Consequences

**Positive**

- Cross-artifact drift becomes a CI failure with a precise message, not a silent inconsistency a reader later trips over. The most common mechanical release error (a version constant out of lockstep) is now gated.
- The lint is the executable form of the ADR-0034 maintenance contract and the §8.7 pre-PR checklist — "run the lint before drafting a post-1.0 PR" has a concrete target.
- Dependency-free and locally runnable: the agent (or any contributor) runs the same check CI runs, with `python tools/consistency_lint.py`.

**Negative / limitations**

- The checks are **heuristic parsers** over Markdown/headers, not a formal model; a sufficiently unusual reformatting of a table could slip past or false-positive. They are pragmatic, and tightened when a real gap appears.
- File-existence (not symbol-existence) for pattern code locations means a public symbol rename that leaves the file in place is not caught here — covered by the build and the periodic M7.5-style audit instead.
- The freshness check needs full git history (`fetch-depth: 0`) and git on `PATH`; it reports a clear failure if neither is available rather than passing silently.
- Adds a small maintenance surface (the script itself) and slightly broadens `docs.yml`'s trigger paths.

**Documentation updates landing in the same PR**

- [`tools/consistency_lint.py`](../../tools/consistency_lint.py) + [`tools/README.md`](../../tools/README.md) (new).
- [`.github/workflows/docs.yml`](../../.github/workflows/docs.yml) — the `consistency` job + broadened triggers.
- [`docs/adr/README.md`](README.md) — index row for ADR-0035.
- [ROADMAP](../../ROADMAP.md) §8.6 — checkbox flipped.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry.

Wiring the lint into the agent contract (the pre-PR checklist in `AGENTS.md` + the PR template) is the separate **§8.7** item.

## References

- [ADR-0034](0034-post-release-maintenance-protocol.md) — the invariants this enforces.
- [`tools/consistency_lint.py`](../../tools/consistency_lint.py) — the implementation and its six checks.
- [Semantic Versioning 2.0.0](https://semver.org/), [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).
