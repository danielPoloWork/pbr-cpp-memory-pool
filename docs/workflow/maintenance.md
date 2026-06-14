# Post-Release Maintenance Protocol

How `pbr-cpp-memory-pool` is governed in the **maintained-product phase** (post-`v1.0.0`): how to decide a release's SemVer level, how a fix reaches users, how security issues and deprecations are handled. This is the **governance** layer; the mechanical step-by-step for cutting any release lives in [`release.md`](release.md), the decision is recorded in [ADR-0034](../adr/0034-post-release-maintenance-protocol.md), and the versioning policy it builds on is [ADR-0004](../adr/0004-versioning-and-release-policy.md). The agent-vs-human boundary (and the tag delegation) is [`AGENTS.md`](../../AGENTS.md) §11 + [ADR-0008](../adr/0008-delegate-tag-creation-and-push-to-the-agent.md).

## SemVer, applied to this project's surfaces

Post-`1.0.0` the project is strict [SemVer 2.0.0](https://semver.org/). The "public API" that the version number protects is, precisely:

- the **C ABI** — the four spec §5 functions and the always-present introspection accessors in [`memory_pool.h`](../../src/main/cpp/it/d4np/memorypool/memory_pool.h);
- the **C++ surface** — `Pool`, `TypedPool<T>`, `PoolAllocator<T>`, `InstrumentedPool`, `PoolObserver`, and the diagnostic iterator, in the public headers.

The compile-time knobs (`PBR_MEMORY_POOL_THREAD_SAFETY`, `PBR_MEMORY_POOL_DIAGNOSTICS`, `PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS`) and the CMake package name / imported target (`pbr::memory_pool`) are part of the contract too — renaming or repurposing them is a breaking change.

| Level | Meaning here | Examples |
|-------|--------------|----------|
| **MAJOR** (`2.0.0`) | A source- or binary-**incompatible** change to the surface above. | Remove/rename a function, change a signature or documented semantics, change the metadata budget in a consumer-visible way, rename the imported target. |
| **MINOR** (`1.x.0`) | **Backward-compatible additions.** Existing code keeps compiling and behaving. | A new public function/type/overload, a new opt-in option, a new milestone's additive feature set, a deprecation (the symbol still works). |
| **PATCH** (`1.0.x`) | **Backward-compatible** bug fixes and changes with **no public-API change**. | A logic fix, a docs/i18n change, a packaging change, a perf improvement, a build-system fix. The shipped library may be byte-identical (e.g. `v1.0.1` — packaging only). |

## Decision tree — which level?

Ask, in order:

1. **Does the change remove, rename, or alter the signature/semantics of any public symbol, knob, or the imported target — such that existing consumer code would fail to compile or link, or behave differently?**
   → **MAJOR.** Requires its own ADR (justifying the break) and a migration note in the release notes. Prefer the deprecation path (below) over an abrupt break.
2. **Does it add new public surface, a new opt-in capability, or a milestone's worth of additive work — while every existing use keeps working?**
   → **MINOR.** (Closing a roadmap milestone is the canonical MINOR — e.g. Milestone 8 → `v1.1.0`.)
3. **Otherwise** — a bug fix, docs/i18n, packaging, perf, or CI change with no public-API change.
   → **PATCH.**

When a change is ambiguous (e.g. a "fix" that subtly changes documented behaviour), treat it as the **higher** level — a wrongly-low version number is the costly mistake (it breaks consumers who trusted SemVer). Record the call in the release notes.

## Version bump & changelog mechanics per level

The mechanics are identical to a milestone close ([`release.md`](release.md) §1–§8); only *which* component of [`version.hpp`](../../src/main/cpp/it/d4np/memorypool/version.hpp) moves differs:

- **PATCH `1.0.x`** — bump `PATCH` only; update the `pool_smoke` version-check `TEST_CASE`. Roll the relevant `Unreleased` lines into `## [1.0.x] — <date>`. Add `docs/releases/v1.0.x.md`. Refresh the README status badge. (`v1.0.1`, the packaging patch, is the worked example.)
- **MINOR `1.x.0`** — bump `MINOR`, reset `PATCH` to `0`. Same changelog roll + release notes; also refresh the README milestone table (a MINOR usually closes a milestone).
- **MAJOR `x.0.0`** — bump `MAJOR`, reset `MINOR`/`PATCH` to `0`. Same mechanics, **plus** the breaking-change ADR and a migration section in `docs/releases/vx.0.0.md`.

In all cases the agent prepares the release PR; the maintainer merges; the agent tags; the maintainer publishes ([AGENTS.md](../../AGENTS.md) §11, [ADR-0008](../adr/0008-delegate-tag-creation-and-push-to-the-agent.md)).

## Hotfix & backport workflow

Two cases, decided by **whether `master` is currently releasable**:

- **`master` is releasable** (the common case — `master` is the release line). Fix on a normal `fix/<name>` branch off `master`, add a test and a `Fixed` `CHANGELOG` line, merge, then cut the next **PATCH** from `master` ([`release.md`](release.md) — *Hotfix flow*). No separate branch is needed.
- **`master` has unreleased changes that cannot ship yet** (e.g. work-in-progress toward the next MINOR/MAJOR). Branch `hotfix/v<X.Y.Z+1>` **from the released tag** (not `master`), apply the minimal fix + test there, cut the PATCH from that branch, then **forward-port** the fix to `master` (cherry-pick) so it is not lost on the next release. The forward-port is mandatory and is part of the same hotfix task.

A hotfix is always the **smallest change that fixes the defect** — no refactors, no unrelated improvements ride along.

## Security fixes

1. **Report privately**, never in a public issue/PR: via [GitHub private vulnerability reporting](https://docs.github.com/code-security/security-advisories) on the repository (a `SECURITY.md` with the contact is a planned addition; until then use the repository's private advisory feature).
2. **Triage & fix under embargo** on a private branch / draft advisory; assess the SemVer level by the decision tree (a fix is usually a **PATCH**, but a fix that must change public behaviour is **MINOR**/**MAJOR**).
3. **Coordinated release**: cut the release, then publish the advisory. Record the fix in `CHANGELOG.md` under a **`Security`** category (Keep a Changelog) with the advisory / CVE reference.
4. Backport to every still-supported release line per the hotfix workflow.

## Deprecation policy

The public API is **not removed abruptly**. To retire or change a public symbol:

1. **Deprecate in a MINOR.** Mark the symbol deprecated in its Doxygen (`@deprecated`) and add a `Deprecated` `CHANGELOG` line; the symbol keeps working unchanged. Record the intent (and the eventual replacement) in an ADR.
2. **Honour a deprecation window** — the symbol remains for at least the rest of the current MAJOR line (≥ one subsequent MINOR), giving consumers time to migrate.
3. **Remove in the next MAJOR**, with the breaking-change ADR (decision-tree step 1) and a migration note.

A deprecation is itself a backward-compatible change (the symbol still works), so it ships in a **MINOR**.

## Consistency lint — failure → remediation

Run `python tools/consistency_lint.py` before drafting any post-1.0 PR ([AGENTS.md](../../AGENTS.md) §6.4; the PR template carries the checkbox; CI re-runs it via the `consistency` job — [ADR-0035](../adr/0035-agent-runnable-consistency-lint.md)). Each failure prints `[check] message`; fix it as follows:

| Failing check | What it means | Remediation |
|---------------|---------------|-------------|
| `version-lockstep` | `version.hpp`, the newest dated `CHANGELOG` block, the README `Status-vX.Y.Z` badge, and the newest `docs/releases/vX.Y.Z.md` disagree. | Move all four together per the per-level mechanics above (and [`release.md`](release.md)). Outside a release PR, they should already equal the last released version. |
| `adr-index` | An ADR file is missing from `docs/adr/README.md` (or vice-versa), or numbering has a gap. | Add the index row for the new `NNNN-*.md` (ADRs are appended with the next sequential number — never reuse/renumber). |
| `patterns` | An Adopted catalogue row cites an ADR or a `src/main/cpp/` path that does not exist. | Fix the moved/renamed path or add the missing ADR link in `docs/patterns/README.md`. |
| `spec-map` | A Spec Coverage Map row has an empty *Roadmap items* cell or no status glyph. | Give the spec row at least one fulfilling roadmap item and a legend glyph (⏳/🚧/✅/❎). |
| `i18n-freshness` | A `translated` page's English source changed after the source commit recorded in the manifest. | Re-sync the affected `docs/i18n/<lang>/…` page to the new source, then update that manifest row's source commit (or, if the source change does not affect the prose, re-pin the commit after reviewing). |
| `milestones` | The README marks a milestone ✅ while a ROADMAP item in it is unchecked, or a checkbox is malformed. | Check the remaining ROADMAP item(s), or correct the README table; fix any `- [ ]`/`- [x]` typo. |

## What this protocol does not change

- The agent-vs-human release boundary and tag delegation are unchanged ([AGENTS.md](../../AGENTS.md) §11, [ADR-0008](../adr/0008-delegate-tag-creation-and-push-to-the-agent.md)).
- The one-PR-at-a-time rule and the documentation-ships-with-code rule still apply to every fix.
- The mechanical release steps still live in [`release.md`](release.md); this document only adds the *which-level / how-a-fix-flows / deprecation / security* governance on top.

## References

- [ADR-0034](../adr/0034-post-release-maintenance-protocol.md) — the decision this document operationalizes.
- [ADR-0004](../adr/0004-versioning-and-release-policy.md) — the versioning & release policy.
- [ADR-0008](../adr/0008-delegate-tag-creation-and-push-to-the-agent.md) — tag creation/push delegated to the agent.
- [`release.md`](release.md) — the mechanical step-by-step for cutting a release.
- [`AGENTS.md`](../../AGENTS.md) §11 — versioning & release, agent-vs-human boundary.
- [Semantic Versioning 2.0.0](https://semver.org/) · [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).
