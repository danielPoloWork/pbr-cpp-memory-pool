# ADR-0036: Extract the session journal from ROADMAP.md into dated per-session files

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Project architect (maintainer), agent
- **Related:** [ADR-0032](0032-documentation-i18n-architecture.md) (precedent for a documentation-structure ADR), [`ROADMAP.md`](../../ROADMAP.md), [`AGENTS.md`](../../AGENTS.md) §7.3 / §7.6, [`docs/journal/`](../journal/)

## Context

`ROADMAP.md` carried two documents with opposite lifecycles in one file:

- a **forward plan** — the numbered, checkbox-driven milestone list, which is stable and edited surgically (flip a checkbox, append an item); and
- a **session journal** — the *Session Checkpoint* section: dated, append-only end-of-session notes ("what got done / where things stand / resume here").

By the close of Milestone 8 the journal had grown to **ten entries spanning ~135 lines — roughly 45% of the 301-line file** — and it only ever grows. In a maintained product running for years the log would dwarf the plan, push the actual roadmap below the fold, and make every roadmap diff noisy with unrelated checkpoint prose. The two concerns also have different audiences: the plan answers *"what is left to do"*, the journal answers *"what happened and how do I resume"*.

The checkpoint is load-bearing — onboarding and the start-of-session routine rely on finding the **latest** checkpoint quickly — so any split has to keep "resume from a known point" a one-click operation.

## Decision

We **move the session checkpoints out of `ROADMAP.md` into `docs/journal/`**, one Markdown file per session under a `YYYY/MM/` folder tree (`docs/journal/<YYYY>/<MM>/<YYYY-MM-DD>-<short-slug>.md`). [`docs/journal/README.md`](../journal/README.md) is the index (newest first, grouped by year and month).

`ROADMAP.md` keeps a short **Session journal** section that links to the journal and carries a single **Latest checkpoint** pointer to the most recent file, preserving the resume-from-a-known-point workflow without the inline bulk. The agent rule for writing checkpoints is codified in [`AGENTS.md`](../../AGENTS.md) §7.6.

## Alternatives Considered

- **Leave it in `ROADMAP.md`.** Rejected — the file already mixes plan and log at a 55/45 split and grows without bound; the problem only worsens.
- **One file per period (per-year or per-month), entries appended within it.** Fewer files, but a per-period file re-grows the same "ever-appending document" problem at a smaller scale, and a single session's checkpoint is no longer addressable by its own URL. Rejected in favour of one-file-per-session, which gives every checkpoint a stable link and a clean, reviewable diff.
- **A single flat `docs/journal/` directory (no year/month folders).** Works at ten entries; does not scale to hundreds. The `YYYY/MM/` tree keeps any one directory small and makes chronological browsing obvious. Rejected for the same scalability reason that motivated the split.

## Consequences

- **No API/ABI/build impact** — documentation-only.
- `ROADMAP.md` drops from 301 to ~171 lines and reads as a plan again; checkpoint diffs no longer touch it (only the one-line Latest-checkpoint pointer moves).
- A new agent obligation (`AGENTS.md` §7.6): at the close of a state-changing session, create the journal file, update the index, and move the `ROADMAP.md` pointer — in the same PR as the work, like any other doc update.
- Relative links inside a checkpoint must account for the new depth: from `docs/journal/<YYYY>/<MM>/` a `../../../` prefix reaches `docs/`, `../../../../` reaches the repo root. The two migrated entries that contained file links were rewritten accordingly; the internal-link CI check (`docs.yml`) guards this going forward.
- The ten historical checkpoints were migrated verbatim (prose unchanged); only the relative links were re-based and a one-line provenance banner added to each.
- This ADR is the documented decision a future reader needs to understand why checkpoints are not in the roadmap. The companion governance question — *when a new feature warrants a new milestone vs. an appended item* — is handled separately and is not in scope here.

## References

- [ADR-0032](0032-documentation-i18n-architecture.md) — prior documentation-structure ADR (i18n), the precedent for recording a docs-layout decision as an ADR.
- [`ROADMAP.md`](../../ROADMAP.md) — the *Session journal* pointer section.
- [`docs/journal/README.md`](../journal/README.md) — the journal index.
- [`AGENTS.md`](../../AGENTS.md) §7.6 — the agent rule for writing checkpoints.
