<!-- markdownlint-disable-file MD041 -->
<!--
Heads-up: this PR body becomes the merge commit's extended description verbatim
(repo is configured with merge_commit_message=PR_BODY and squash_merge_commit_message=PR_BODY).
Before clicking "Merge", scrub any HTML comments and placeholder hints below
so the resulting commit message reads cleanly in `git log`.
-->

## Summary
<!-- One or two sentences: what changes and why it matters. -->

## Motivation
<!-- Link to the spec section, ADR, roadmap item, or issue that prompted this work. -->

## Changes
<!-- Bulleted list of meaningful changes (not a file list). -->

-

## Design Patterns
<!--
List every pattern adopted, refined, or rejected in this PR, with a one-line rationale
and a link to the ADR. If none, write: "None — straightforward implementation."
Pattern names must match docs/patterns/design-patterns.md.
-->

-

## Verification

- [ ] Builds cleanly on the full toolchain matrix
- [ ] Unit tests pass
- [ ] `clang-tidy` clean on the diff
- [ ] ASan + UBSan clean (TSan when threading is involved)
- [ ] Valgrind: `ERROR SUMMARY: 0 errors from 0 contexts`
- [ ] Benchmark numbers attached (when perf-relevant)
- [ ] N/A — docs-only or scaffolding PR

## Documentation Impact

- [ ] `README.md` updated (if user-facing surface changed)
- [ ] `ROADMAP.md` checkbox flipped
- [ ] ADR added/updated (if a non-trivial design decision was made)
- [ ] `docs/patterns/README.md` updated (if a pattern was introduced, refined, or rejected)
- [ ] Spec under `docs/specs/` updated (if behavior diverges)
- [ ] `CHANGELOG.md` updated (for user-visible changes; see [ADR-0004](../docs/adr/0004-versioning-and-release-policy.md) §3)
- [ ] Consistency lint passes — `python tools/consistency_lint.py` (mandatory for post-1.0 PRs; see [`docs/workflow/maintenance.md`](../docs/workflow/maintenance.md))

<!--
Reminder of the agent-vs-human boundary (AGENTS.md §6.1):
  - Agents create branches, commit, push, and DRAFT PRs.
  - The maintainer OPENS and MERGES PRs manually.
  - Never push to master. Never force-push. Never merge.
-->
