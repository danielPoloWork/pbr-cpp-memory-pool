# ADR-0037: A new feature is planned on the roadmap — as a new milestone or an appended item

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Project architect (maintainer), agent
- **Related:** [ADR-0004](0004-versioning-and-release-policy.md) (versioning), [ADR-0034](0034-post-release-maintenance-protocol.md) (post-release governance), [ADR-0036](0036-session-journal-extraction.md) (the companion docs-structure split), [`AGENTS.md`](../../AGENTS.md) §7.3, [`docs/workflow/maintenance.md`](../workflow/maintenance.md), [`ROADMAP.md`](../../ROADMAP.md)

## Context

`ROADMAP.md` is the project's plan of record: numbered, checkbox-driven items grouped into milestones, each milestone closing as a release. Through `v1.1.0` the plan was built out as Milestones 0–8 and is now **complete** — every planned item is checked.

The existing rule ([`AGENTS.md`](../../AGENTS.md) §7.3) covered only *incremental* work: *"new work that emerges goes at the bottom of the relevant section with a fresh number."* That is sound while a milestone is open, but it leaves two gaps now that the planned roadmap is closed:

1. **There is no obvious home for a genuinely new feature.** Appending it to a closed, thematically-unrelated milestone (e.g. tucking a new capability under "Milestone 8 — Internationalization") is misleading and makes the roadmap illegible.
2. **Nothing stated that a new feature must reach the roadmap at all.** The post-release maintenance protocol ([ADR-0034](0034-post-release-maintenance-protocol.md)) describes how a change maps to a SemVer level and even calls "closing a roadmap milestone" the canonical MINOR — but it never said *when to create* a milestone, so a feature could ship as a bare PR with no plan entry.

We want every feature traceable on the roadmap *before or as* it lands, while not forcing milestone ceremony onto small additive changes or fixes.

## Decision

**Every feature is placed on the roadmap as part of the PR that starts its implementation. Whether it is a new milestone or an item under an existing milestone is a deliberate judgment on scope**, made as follows:

- **New milestone** (the next sequential `Milestone N`) — when the feature is a **cohesive capability with its own arc**: typically an ADR + implementation + tests + docs across several numbered items, comparable in scope to Milestones 2–8. The milestone is created in the same PR that starts the work, and closing it is the canonical **MINOR** bump.
- **Appended item(s)** — when the work **extends a still-open milestone** or is a small additive task that clearly belongs to an already-defined theme. A fresh `<milestone>.<task>` number is used; numbers are never reused or renumbered.
- **Neither (a maintenance change)** — a bug fix, docs/i18n, packaging, perf, or CI change that is not itself a feature is **not** a roadmap milestone. It is governed by the [maintenance protocol](../workflow/maintenance.md) decision tree (PATCH/MINOR), recorded in `CHANGELOG.md`, and gets an ADR only if it carries a design decision.

The **default for a genuinely new capability post-1.0 is a new milestone.** When a feature could plausibly be either, the reasoning is recorded in the feature's ADR or the PR body.

## Alternatives Considered

- **Always append items, never add milestones post-1.0.** Rejected — appending unrelated capabilities under closed milestones makes the roadmap a junk drawer; the milestone boundaries (which also map to MINOR releases) lose meaning.
- **Always create a new milestone for any new work.** Rejected — milestone ceremony (a multi-item arc, a MINOR close) is disproportionate for a one-line addition or a fix; the maintenance protocol already handles those.
- **Leave it implicit / case-by-case with no written rule.** Rejected — the gap already produced ambiguity (a complete roadmap with no stated home for the next feature). A documented judgment rule with a stated default removes the recurring question without over-constraining.

## Consequences

- **No API/ABI/build impact** — process/documentation only.
- `AGENTS.md` §7.3 now carries the milestone-vs-item-vs-maintenance judgment and the "every feature reaches the roadmap" requirement; the maintenance protocol cross-references it from the MINOR branch of its decision tree.
- The roadmap stays legible as it grows: new capabilities arrive as `Milestone 9`, `10`, … rather than as orphaned items, and each new milestone's close lines up with a MINOR release.
- A small judgment cost per feature (milestone or item?), bounded by the stated default and the requirement to record the reasoning when non-obvious.
- This is the companion to [ADR-0036](0036-session-journal-extraction.md): together they keep `ROADMAP.md` a forward plan — checkpoints moved out (0036), and new work placed deliberately within it (0037).

## References

- [`AGENTS.md`](../../AGENTS.md) §7.3 — the operational rule this ADR records.
- [`docs/workflow/maintenance.md`](../workflow/maintenance.md) — the SemVer decision tree; milestone close = MINOR.
- [ADR-0004](0004-versioning-and-release-policy.md) — versioning & release policy.
- [ADR-0034](0034-post-release-maintenance-protocol.md) — post-release maintenance protocol.
- [ADR-0036](0036-session-journal-extraction.md) — session-journal extraction (companion docs-structure decision).
