# Git Workflow

Full conventions for branches, commits, and pull requests on `pbr-cpp-memory-pool`. The short version is in [`AGENTS.md`](../../AGENTS.md) §5; this document expands it with examples and edge-case guidance.

## 1. Boundary between agent and human

Agents are trusted to:

- Create and switch branches.
- Stage, commit, and push to feature branches.
- Draft pull request titles and bodies.
- Run `gh pr create --draft` only when the user has explicitly authorized PR creation in the current session.

Agents must **never**:

- Push directly to `master`.
- Force-push to `master` under any circumstance.
- Merge or squash-merge a pull request (no `gh pr merge`, no `git merge` into `master`).
- Skip git hooks (`--no-verify`) or signing (`--no-gpg-sign`) unless the user has explicitly asked for it.
- Delete branches the user has not asked to delete.

The human reviews and merges. When in doubt, push the branch and ask.

## 2. Branch naming

Format:

```
<type>/<short-kebab-description>
```

`type` values:

| Type        | Use when…                                                                 |
|-------------|---------------------------------------------------------------------------|
| `feat`      | a new user-visible capability is added                                    |
| `fix`       | a defect is corrected                                                     |
| `refactor`  | code is restructured without behavior change                              |
| `perf`      | a change targets measurable performance                                   |
| `docs`      | only documentation is touched                                             |
| `test`      | tests are added or revised, no production code change                     |
| `build`     | build system, CMake, presets, toolchain                                   |
| `chore`     | tooling, formatting config, repo housekeeping                             |
| `ci`        | continuous-integration pipeline changes                                   |

Description guidance:

- Lowercase kebab-case, ≤40 characters.
- Describe the *what*, not the issue number (issue links live in the commit body).
- Avoid generic names like `feat/update` or `fix/bug` — they age badly.

Examples:

```
feat/free-list-alloc
feat/cpp-wrapper-raii
fix/destroy-double-free
perf/cacheline-aligned-blocks
docs/adr-thread-safety
build/cmake-presets
ci/valgrind-job
```

## 3. Commit messages — Conventional Commits

```
<type>(<scope>): <imperative subject ≤72 chars>

<body — explain WHY, wrap at ~72 cols>

<optional footers>
```

- `type` matches the branch-type vocabulary above.
- `scope` is a short noun referring to a subsystem: `pool`, `freelist`, `threading`, `api`, `build`, `tests`, `bench`, `docs`, `adr`, `ci`.
- Subject in **imperative mood**: "add free-list allocation path", not "added free-list allocation path" and not "adds free-list allocation path".
- Body explains motivation, trade-offs, and links to ADRs or spec sections. The diff already shows the *what*; the commit message must add the *why*.
- Footers:
  - `BREAKING CHANGE: <description>` — any source- or binary-incompatible change.
  - `Refs: ADR-0003` / `Refs: #42` — link to ADRs or issues.

Example:

```
feat(freelist): implement implicit free-list allocation

Store the next-free pointer inside the first bytes of each unallocated
block. This keeps metadata overhead at zero for live blocks and gives
us O(1) alloc and free without an auxiliary bitmap.

The block_size minimum becomes sizeof(void*); enforced in
memory_pool_create() and documented in the public header.

Refs: ADR-0002
```

### Granularity

One logical change per commit. If a PR contains preparatory refactors and the feature itself, split them: reviewers can then evaluate each independently. Rebase / squash before opening the PR if intermediate commits ("wip", "fix typo") survived.

## 4. Pull Requests

### 4.1 Title

PR title equals the lead commit subject — same Conventional Commits format, ≤72 characters.

### 4.2 Body template

```markdown
## Summary
One or two sentences: what changes and why it matters.

## Motivation
Link to the spec section, ADR, roadmap item, or issue that prompted this work.

## Changes
- bulleted list of meaningful changes (not a file list)

## Verification
- [ ] Builds cleanly (`cmake --build`)
- [ ] Unit tests pass
- [ ] Valgrind: `ERROR SUMMARY: 0 errors from 0 contexts`
- [ ] Benchmark numbers attached (when perf-relevant)

## Documentation Impact
- [ ] README.md updated (if user-facing surface changed)
- [ ] ROADMAP.md checkbox flipped
- [ ] ADR added/updated (if a non-trivial design decision was made)
- [ ] Spec updated (if behavior diverges from `docs/specs/`)
```

### 4.3 Drafting flow

1. Branch off `master`: `git switch -c feat/<short-name>`.
2. Make changes; commit in logical units.
3. Push: `git push -u origin feat/<short-name>`.
4. Prepare the PR body. If the user has authorized it: `gh pr create --draft --title "..." --body "$(cat <<'EOF' ... EOF)"`. Otherwise, print the proposed command and wait.
5. **Stop.** The user opens the PR for review and merges manually.

### 4.4 Responding to review

- Address review comments with new commits on the same branch. Avoid `--amend` on commits that are already pushed — create fresh commits instead.
- Squash only when the user requests it.
- After merge, the user deletes the branch. Agents do not delete branches.

## 5. Repository hygiene

- `.gitignore` already excludes build outputs, IDE folders, and CMake artifacts — extend it rather than committing generated files.
- Never commit secrets, credentials, or local toolchain paths.
- Large binary fixtures do not belong in this repo; if needed, discuss with the maintainer first.
