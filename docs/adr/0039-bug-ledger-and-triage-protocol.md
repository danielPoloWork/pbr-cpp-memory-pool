# ADR-0039: In-repo bug ledger and agent triage protocol

- **Status:** Accepted
- **Date:** 2026-06-15
- **Deciders:** Project architect (maintainer), agent
- **Related:** [ADR-0036](0036-session-journal-extraction.md) (dated per-file documentation precedent), [ADR-0034](0034-post-release-maintenance-protocol.md) (maintenance governance), [ADR-0035](0035-agent-runnable-consistency-lint.md) (the lint this extends), [`docs/workflow/maintenance.md`](../workflow/maintenance.md), [`AGENTS.md`](../../AGENTS.md) §7.7, [`docs/bugs/`](../bugs/)

## Context

Entering the maintained-product phase (post-`v1.0.0`), the project tracks two halves of a defect's life but not the whole:

- A **fixed** defect already has a home — the `Fixed` category of the `CHANGELOG` ([ADR-0038](0038-changelog-version-split.md)) plus the hotfix/PATCH flow in [`docs/workflow/maintenance.md`](../workflow/maintenance.md). That records *what was fixed and in which release*.
- A **known-but-open** defect, and the **triage** of an incoming report (reproduction attempt, root-cause analysis, the verdict — including *rejected*), had **no durable record**. It lived only in chat or in a transient issue.

This is a gap for a reference repository whose whole premise is that every important artifact is versioned, reviewable, and readable offline (ADRs, the patterns catalogue, the session journal). A defect's investigation is exactly such an artifact: it has evidence, a root cause, an impact assessment, and a verdict that a future reader should be able to reconstruct without access to the original conversation.

A second, agent-specific force: the maintainer wants two repeatable behaviours from the agent — (1) when asked to *hunt* for bugs, the agent should land each **verified** finding as a durable record rather than a chat message; (2) when a **third party** reports a bug, the agent must **verify it (reproduce + root-cause) before** accepting it, never transcribe an unconfirmed claim into the record. These are judgment-bearing behaviours, so they belong in the agent contract, not in a deterministic hook.

## Decision

We add an **in-repo bug ledger** under [`docs/bugs/`](../bugs/) and codify the agent's bug-handling protocol.

**Storage & identity (the "ID + date-tree mix").** One Markdown file per defect, named `BUG-NNNN-<short-kebab-slug>.md`, stored under a discovery-date folder tree: `docs/bugs/<YYYY>/<MM>/BUG-NNNN-<slug>.md`.

- `NNNN` is a **zero-padded, globally monotonic** id (like ADR numbers — never reused, never renumbered), giving every defect a short stable handle (`BUG-0007`) for cross-references from commits, PRs, and the `CHANGELOG` `Fixed` line.
- The `<YYYY>/<MM>` folders (by **discovery** date) keep any one directory small — the same idiom already used by the session journal ([ADR-0036](0036-session-journal-extraction.md)), so there is no new convention to learn.
- [`docs/bugs/README.md`](../bugs/README.md) is the index (newest first) resolving every id to its path; [`docs/bugs/template.md`](../bugs/template.md) is the per-bug template.

**The ledger is the source of truth.** Bug records live in the repo and are reviewed like any other artifact. A GitHub issue, if one exists, is referenced from the record but is not authoritative — consistent with ADRs and the journal living in-repo, and with the project's offline-readable, self-contained premise.

**Structured frontmatter** carries the queryable state: `id`, `title`, `status`, `severity`, `reporter`, `discovered`, `affected-versions`, and (once closed) `fixed-in`. The lifecycle is `open → confirmed → fixed`, with the terminal states `wontfix`, `duplicate`, and `cannot-reproduce`.

**Agent triage protocol** (codified in [`AGENTS.md`](../../AGENTS.md) §7.7):

- *"find / hunt for bugs"* → the agent creates a ledger file only for a **verified, reproducible** defect — never a speculative one.
- *third-party report* → the agent **reproduces and root-causes first**; only on confirmation does it create a `confirmed` record (with `reporter: third-party` and the reproduction as evidence). A report it **cannot** substantiate is **still recorded** — as `cannot-reproduce` (or `rejected`/`duplicate`) with the investigation that reached that verdict — so the triage trail is preserved rather than lost.
- A fix then flows through the existing maintenance machinery: the fixing PR flips the record to `fixed`, fills `fixed-in`, and adds the `CHANGELOG` `Fixed` line — the ledger and the changelog cross-reference each other.

**Integrity is enforced by the consistency lint** ([ADR-0035](0035-agent-runnable-consistency-lint.md)): a new `bugs` check validates each record's frontmatter (required keys, allowed `status`/`severity`/`reporter` vocabularies), the filename↔`id` agreement, the path↔`discovered`-date agreement, globally-unique non-gapped ids, the index↔files bijection, and that a `fixed` record names its `fixed-in`. With zero records the check is a no-op, so the gate is green from the moment the scaffold lands.

## Alternatives Considered

- **GitHub Issues as the source of truth.** The platform-native tracker, free workflow and search. Rejected as the *authority* because it breaks the repo's self-contained, offline-readable premise (the investigation would not travel with a clone or a release tarball) and splits the record from the code and the ADRs it references. Issues remain welcome as a *front door* — their number is cross-referenced from the record.
- **Flat `docs/bugs/BUG-NNNN-slug.md` (no date folders), pure ADR shape.** Simplest, and the id already gives identity. Rejected on the maintainer's explicit concern: a single directory accreting every defect over a multi-year maintained life grows unwieldy (and is awkward to browse on Windows). The date-tree mix keeps each directory small at no cost to the stable id.
- **Pure date-tree `…/<YYYY>/<MM>/<YYYY-MM-DD>-slug.md` (journal shape, no id).** Matches the journal exactly, but the filename-as-identity is long and not resolvable from a short handle — cross-referencing a defect from a commit or `CHANGELOG` line would need the full dated slug. Rejected in favour of keeping the short monotonic `BUG-NNNN` id *and* the date folders.
- **Record only confirmed defects; drop unsubstantiated reports.** Fewer files. Rejected because the *triage* — the evidence that a reported bug could **not** be reproduced — is itself valuable institutional memory for a reference project, and prevents the same rejected report from being re-litigated.
- **A deterministic hook that auto-creates the file on a trigger phrase.** Rejected because deciding a defect is *real* (reproduce + root-cause, or refute a third-party claim) is a judgment task a hook cannot perform. A hook can enforce *structure* — that role is filled by the consistency-lint `bugs` check; the *judgment* lives in the agent contract.

## Consequences

- **No API / ABI / build impact** — documentation, process, and tooling only.
- A new agent obligation (`AGENTS.md` §7.7): verified defects and triaged third-party reports become ledger files in the same PR as the investigation, like any other doc-with-the-work rule.
- The maintained-product governance gains a defect-lifecycle section in [`docs/workflow/maintenance.md`](../workflow/maintenance.md) tying the ledger to the existing `Fixed`/hotfix/security flows; a `fixed` record and its `CHANGELOG` line are kept in lockstep by convention and, partly, by the lint.
- The consistency lint grows a sixth-plus `bugs` check and its remediation row; CI re-runs it via the existing `consistency` job. The path↔`discovered` agreement means the lint reads frontmatter, so a malformed date or a misfiled record fails fast and locally.
- Relative links inside a bug record must account for the `docs/bugs/<YYYY>/<MM>/` depth (`../../../` reaches `docs/`, `../../../../` the repo root) — the same rule the journal already follows; the `docs.yml` link check guards it.
- This is a **maintenance/process change, not a feature** ([`AGENTS.md`](../../AGENTS.md) §7.3, [ADR-0037](0037-new-feature-roadmap-placement.md)): no roadmap milestone, recorded in `CHANGELOG`, justified by this ADR.

## References

- [ADR-0036](0036-session-journal-extraction.md) — the dated per-session-file precedent the date-tree reuses.
- [ADR-0034](0034-post-release-maintenance-protocol.md) — the maintenance governance the lifecycle plugs into.
- [ADR-0035](0035-agent-runnable-consistency-lint.md) — the consistency lint extended with the `bugs` check.
- [ADR-0038](0038-changelog-version-split.md) — the `CHANGELOG` whose `Fixed` category records the closing side of a defect.
- [`docs/bugs/README.md`](../bugs/README.md) — the ledger index and how-to.
- [`docs/workflow/maintenance.md`](../workflow/maintenance.md) — the defect-lifecycle governance section.
- [`AGENTS.md`](../../AGENTS.md) §7.7 — the agent triage protocol.
