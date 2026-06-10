# ADR-0008: Delegate Tag Creation and Push to the Agent

- **Status:** Accepted
- **Date:** 2026-06-10
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0004](0004-versioning-and-release-policy.md) §6 (the boundary this ADR amends), [`AGENTS.md`](../../AGENTS.md) §11, [`docs/workflow/release.md`](../workflow/release.md) §2 (boundary recap) and §7 (Tag step), [`AGENTS.md`](../../AGENTS.md) §6.1 (the PR-merge boundary this ADR keeps untouched as the parallel reference point)

## Context

[ADR-0004](0004-versioning-and-release-policy.md) §6 established the original agent-vs-human boundary for releases, mirroring the PR-merge boundary in [`AGENTS.md`](../../AGENTS.md) §6.1. Under that boundary, the agent prepared the release (version bump, `CHANGELOG.md` roll-up, release-notes draft, release PR) and the maintainer carried out every step from `git tag -a` onward — tag creation, tag push, and the *Publish* click on the draft GitHub Release.

The first real exercise of the policy — the `v0.1.0` release closing Milestone 1 — surfaced friction in a single-maintainer + AI-agent setup that the original boundary had not anticipated:

1. **The tag-push step is not the meaningful checkpoint.** The release PR's merge is the human checkpoint that decides *what* ships. The tag is a mechanical follow-up: `git tag -a v<X.Y.Z> -m "<headline>"` on the merge commit, then `git push origin v<X.Y.Z>`. The maintainer typing those commands adds no security or audit value that the merged PR did not already provide.
2. **The Publish click *is* the meaningful checkpoint.** The draft GitHub Release that `release.yml` produces is what becomes visible to the world. The maintainer reviewing the auto-built artifacts, `SHA256SUMS`, and notes before clicking *Publish* is the actual go/no-go gate.
3. **Tags pre-publication are reversible at low cost.** A tag that was just pushed but whose `release.yml` run failed (or pointed at the wrong commit) can be deleted (`git tag -d` + `git push --delete`) and re-pushed. The pre-`v0.6` discussion in ADR-0004 §Consequences already acknowledged this as an accepted operational reality.
4. **Cadence cost without governance benefit.** Forcing the human through `git tag -a` + `git push origin` on every milestone close (and every hotfix tag) costs per-release friction with no corresponding governance signal — the merge has already been made; the tag merely materialises that decision in `refs/tags/`.

The maintainer explicitly authorised the delegation in conversation on 2026-06-10 — *"lancia sempre tu i comandi di tag"* — making the policy change durable rather than per-release.

The parallel boundary in [`AGENTS.md`](../../AGENTS.md) §6.1 (agents never *merge* PRs) is **not** affected by this ADR. The PR-merge is the *what-ships* checkpoint and stays human-only; this ADR only relaxes the tag-creation and tag-push steps that materialise an already-merged release.

## Decision

We amend ADR-0004 §6 as follows. The agent-vs-human boundary for releases is now:

| Action                                                       | Who does it      |
|--------------------------------------------------------------|------------------|
| Bump version constant in source                              | Agent            |
| Update `CHANGELOG.md` (roll `Unreleased` into version)       | Agent            |
| Draft GitHub Release notes (`docs/releases/v<X.Y.Z>.md`)     | Agent            |
| Open the release PR                                          | **Human**        |
| Merge the release PR                                         | **Human**        |
| Create the annotated git tag (`git tag -a v<X.Y.Z>`)         | Agent (new)      |
| Push the tag (`git push origin v<X.Y.Z>`)                    | Agent (new)      |
| Publish the GitHub Release (button on the web UI)            | **Human**        |
| Build & attach release artifacts                             | CI (automated)   |

Operationally:

- The agent runs `git tag -a v<X.Y.Z> -m "v<X.Y.Z> — <milestone headline>"` and `git push origin v<X.Y.Z>` immediately after the corresponding release PR merges to `master`. The tag message mirrors the release-notes headline per [`docs/workflow/release.md`](../workflow/release.md) §7.
- The agent **does not** click *Publish* on the draft GitHub Release. The maintainer reviews the draft (body, artifacts, `SHA256SUMS`) on the web UI and clicks the button. CI deliberately stops in *Draft* state — `release.yml` is unchanged by this ADR.
- The agent **does not** delete or amend a tag that has been *published* (i.e. whose draft GitHub Release was promoted to a real Release by the maintainer). Republishing under the same tag breaks consumers' pinning, which `pre-1.0` ADR-0004 §Consequences explicitly flagged as mildly disruptive but acceptable only in extreme cases.
- The agent **may** delete and re-push a tag whose `release.yml` run failed visibly *before* the human Published — for example, the wrong commit was tagged, or the build matrix flaked in a way that produced corrupt artifacts. This is the documented escape hatch.

The change is durable across sessions and projects within this repository; the maintainer's authorisation is recorded in the agent's project-scoped memory.

## Alternatives Considered

- **Keep ADR-0004 §6 unchanged.** Rejected. The original boundary survived its first contact with reality (the `v0.1.0` release) and produced exactly the friction listed in *Context* item 4. The case for treating tag-push as a governance step did not survive examination — the actual checkpoint is the *Publish* click.
- **Delegate the Publish click too.** Rejected. The Publish-as-final-gate model is the *one* place in the release flow where a human review of the *produced* artifacts (not just the source diff) makes sense — the maintainer sees the actual `pbr-memory-pool-<version>-<platform>.tar.gz` files, the `SHA256SUMS`, and the rendered release body before they become world-visible. Removing that gate trades a small ergonomic win for a real loss of pre-publication review.
- **Use a scoped, tag-only personal-access token issued specifically to the agent.** Considered. The `gh` CLI token already in the agent's session has `repo` scope, which permits tag pushes; introducing a parallel narrower token would not add governance — the same set of operations would already be possible. Reconsider if the project grows past one maintainer and a finer-grained permission split between humans becomes useful.
- **Supersede ADR-0004 entirely with a new revised release ADR.** Rejected. ADR-0004 covers six topics — SemVer scheme, tag cadence, CHANGELOG format, release artifacts, distribution phases, and the agent-vs-human boundary — and only the last one shifts. Superseding the whole document for a single-row change in one table buries the historical decision under noise. A targeted amending ADR (this one) is the proportional move; the `Related:` header makes the chain discoverable.

## Consequences

**Positive**

- The full release flow from "merge release PR" through "draft GitHub Release created" can now be agent-driven. The maintainer's mandatory work compresses to: review and merge the release PR, then review and Publish the draft Release.
- Hotfix tags (`v0.X.(Y+1)`) inherit the same delegation — small fix PRs no longer require the maintainer to remember the `git tag -a` ritual after merging.
- The change is reversible: the maintainer can revoke the authorisation at any time by stating so; the next ADR in the chain would then reinstate the human-only steps.

**Negative**

- The agent's effective write surface to the remote widens by two commands (`git tag` is a no-op locally; `git push origin v<X.Y.Z>` is the visible change). Mitigated by: (a) the `Publish` checkpoint remains human-only, so nothing becomes world-visible without human review; (b) the agent is bound by the no-amend / no-delete-of-published-tags clause above.
- The boundary documented across `AGENTS.md`, `docs/workflow/release.md`, and ADR-0004 must be kept consistent across the next doc-sync PR. The mitigation is the same PR that introduces this ADR — `AGENTS.md` §11 and `docs/workflow/release.md` §2/§7 are updated in lockstep, with explicit links back to this ADR.

**Required documentation updates landing in the same PR as this ADR**

- [`AGENTS.md`](../../AGENTS.md) §11 — the boundary table flips the *Create the annotated git tag* and *Push the tag* rows from Human to Agent; a footnote points to this ADR.
- [`docs/workflow/release.md`](../workflow/release.md) §2 (Boundary recap) and §7 (Tag step) — same table change; §7's prose changes from *"Tag (maintainer)"* to *"Tag (agent)"* with a one-line note that the maintainer remains the *Publisher*.
- [`docs/adr/README.md`](README.md) — index row for ADR-0008.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Changed` entry under *Process* / *Workflow* documenting the boundary shift.

ADR-0004 itself is **not** edited — Accepted ADRs are immutable per [`docs/adr/README.md`](README.md#lifecycle). Readers of ADR-0004 §6 reach this amendment through the `Related:` chain in this ADR and through the section's own `Related:` line being added to AGENTS.md and release.md (which the same PR updates).

## References

- [ADR-0004 §6](0004-versioning-and-release-policy.md#6-agent-vs-human-boundary) — the boundary this ADR amends.
- [`AGENTS.md`](../../AGENTS.md) §11 — the policy summary kept in lockstep with ADR-0004 and (now) this ADR.
- [`AGENTS.md`](../../AGENTS.md) §6.1 — the PR-merge boundary, deliberately untouched by this ADR; the human-merges-PRs checkpoint remains the *what-ships* gate.
- [`docs/workflow/release.md`](../workflow/release.md) — the operational guide whose §2 boundary table and §7 *Tag* step are rewritten in the companion edit to this ADR.
- 2026-06-10 maintainer authorisation message ("lancia sempre tu i comandi di tag") — the conversation that prompted the change; persisted in the agent's project-scoped memory so subsequent sessions do not re-ask.
