# ADR-0040: Pull-request metadata policy (assignee, labels, milestone)

- **Status:** Accepted
- **Date:** 2026-06-15
- **Deciders:** Project architect (maintainer), agent
- **Related:** [`AGENTS.md`](../../AGENTS.md) §6.4, [ADR-0004](0004-versioning-and-release-policy.md) / [`docs/workflow/maintenance.md`](../workflow/maintenance.md) (the release-milestone scheme), [`docs/workflow/git-workflow.md`](../workflow/git-workflow.md)

## Context

Agent-opened PRs so far carried only a **title** and a **body** (`AGENTS.md` §6.4). GitHub's structured PR metadata — **reviewers, assignees, labels, projects, milestone** — was left unset, so the PR list and the release view carry no at-a-glance type, ownership, or release-grouping signal, and nothing ties a PR to the release it ships in.

The maintainer asked that every PR also set these fields automatically, and that the recent PRs be backfilled. Inspecting the repository surfaced concrete constraints that shape what is achievable:

- **Reviewers** — the repository's **only collaborator is the maintainer** (`danielPoloWork`), who is also the author of every agent-opened PR (the agent authenticates as that account via `gh`). GitHub **forbids requesting a review from the PR author**, and there is no other collaborator or team. So a reviewer cannot be set today.
- **Labels** — only the nine GitHub default labels existed; none expressed the change *type*.
- **Milestones** — none existed.
- **Projects** — there is no classic Project, and the `gh` token lacks the `read:project` / `project` OAuth scope, so a Projects (v2) board can be neither read nor written without a deliberate scope grant by the maintainer.

## Decision

Every agent-opened PR sets, in addition to its title and body:

- **Assignee** — the maintainer (`--assignee @me`; the agent authenticates as the owner account `danielPoloWork`).
- **Label** — **exactly one type label** matching the lead commit's Conventional-Commit `type`, per the map in `AGENTS.md` §6.4 (`feat`/`fix`/`refactor`/`perf`/`test`/`build`/`chore`/`ci` map to a like-named label; **`docs` reuses the built-in `documentation` label**). One PR carries one type, mirroring the one-logical-change-per-PR rule.
- **Milestone** — the current open **release milestone** under a **per-release** scheme (e.g. `v1.1.1`). The SemVer level of the next release is decided by [`docs/workflow/maintenance.md`](../workflow/maintenance.md); the agent creates the milestone if the next release does not have one yet, and the milestone closes when that version is tagged.

Two fields are **deferred**, with the rule written so they switch on without a redesign:

- **Reviewers** — not set while the sole collaborator is the PR author. When a second collaborator or a review team exists, the rule gains `--reviewer <user|org/team>`.
- **Projects** — not set until the `gh` token carries the `project` scope (`gh auth refresh -s read:project,project`) and a board exists; the rule then gains `--project <name>`.

The mechanism is the **agent contract** (`AGENTS.md` §6.4) carrying the canonical `gh pr create` invocation and the type→label map — a judgment-light, deterministic step the agent performs on every PR — not a CI Action. The eight type labels and the first `v1.1.1` milestone were created as part of this change, and the open/recent PRs (#89, #90, #91) were backfilled.

## Alternatives Considered

- **Leave PRs metadata-less (status quo).** Zero overhead, but the PR list and release view stay signal-poor and no PR is tied to its release. Rejected — the maintainer asked for the structure.
- **Label by *area* (e.g. `pool`, `freelist`, `threading`, `i18n`) instead of, or in addition to, type.** Richer filtering, but area is a judgment call per PR (many PRs span areas) and would dilute the deterministic one-label rule. Rejected as the *primary* scheme; area labels can be added later as a non-exclusive secondary set if the volume justifies it.
- **A single rolling "Maintenance" milestone** for all post-1.0 PRs. Simple, but it never closes and conveys no release grouping — the very signal a milestone exists to give. Rejected in favour of per-release milestones that close at each tag.
- **A CI auto-labeler (`actions/labeler`) keyed on changed paths.** Moves labeling off the agent, but keys on file paths (an area signal) rather than the Conventional-Commit type, needs its own config to maintain, and cannot set assignee/milestone. Rejected; the agent already knows the commit type at PR-open time.
- **Force a reviewer anyway (self-review / a bot).** Not possible (GitHub blocks author self-review) and a bot reviewer adds noise without review value. Rejected; reviewers wait for a real second reviewer.

## Consequences

- A new agent obligation (`AGENTS.md` §6.4): open every PR with `--assignee @me --label <type> --milestone <vX.Y.Z>`, creating the milestone when the next release lacks one.
- The label set grows by eight type labels; `documentation` is reused for `docs`. The repo now carries a `v1.1.1` release milestone.
- **Reviewers** remain unset until a second collaborator/team is added — a known, documented gap, not an oversight. **Projects** remain unset until the maintainer grants the `project` OAuth scope and creates a board; the rule already names the switch-on step.
- No API/ABI/build/code impact — process and repository-metadata only; recorded in `CHANGELOG` under `Unreleased`.
- The policy is GitHub-state, not repo-file, so it is **not** enforced by `tools/consistency_lint.py` (which checks files); compliance rides on the agent contract and PR review.

## References

- [`AGENTS.md`](../../AGENTS.md) §6.4 — the PR contract carrying the metadata rule and the type→label map.
- [`docs/workflow/maintenance.md`](../workflow/maintenance.md) — the SemVer decision tree that names the next release (hence the milestone).
- [`docs/workflow/git-workflow.md`](../workflow/git-workflow.md) — the broader git/PR conventions.
- [GitHub: about pull request reviews](https://docs.github.com/pull-requests) — author cannot be a reviewer of their own PR.
