# ADR-0032: Documentation i18n architecture

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) (the doc-format taxonomy this localizes a subset of), [ADR-0004](0004-versioning-and-release-policy.md) (post-1.0 cadence the manifest tracks against), [`AGENTS.md`](../../AGENTS.md) §2 (English working-language rule), [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §3.3 (the zero-external-dependency philosophy applied here to the docs toolchain), [ROADMAP](../../ROADMAP.md) §8.1 (the item) and §8.2 (the scaffold that implements this decision), §8.6 (the consistency lint that enforces the manifest).

## Context

Milestone 8 opens the maintained-product phase and adds a **documentation-translation system** — Simplified Chinese and Japanese to start — so the reference implementation is approachable beyond English readers while English stays the normative source. This ADR fixes the *architecture* of that system before any translation is written (§8.3/§8.4) or scaffolded (§8.2), so every later item builds on a settled shape.

The forces:

- **English is, and remains, normative.** [`AGENTS.md`](../../AGENTS.md) §2 already mandates English for every on-disk artifact; translations are a *reader-facing convenience layer*, never a second source of truth. A conflict between a translation and its English source is always a translation bug.
- **Zero external dependencies, applied to docs.** Spec §3.3's posture (no third-party packages in the build graph) extends naturally: a SaaS translation platform would be the project's first runtime/CI dependency on an outside service, for a two-language didactic doc set.
- **Not everything should be translated.** Some artifacts are immutable records (ADRs), append-only ledgers (`CHANGELOG.md`), or high-churn planning (`ROADMAP.md`); translating them guarantees perpetual staleness and drift.
- **Staleness must be detectable.** A translation silently lagging its English source is the central failure mode of any docs-i18n effort; the architecture must make "this page is stale" a *machine-checkable* fact, not a reader's guess.

## Decision

We adopt a **file-based, zero-external-dependency, per-language Markdown tree** under `docs/i18n/<lang>/` that mirrors the relative path of each translatable English page, with **English as the single normative source**, a **machine-checkable translation-status manifest**, a **terminology glossary**, and an **explicit English fallback** for anything not (yet) translated. Languages are identified by BCP-47 codes: **`zh-Hans`** (Simplified Chinese) and **`ja`** (Japanese). The scaffold, manifest, and glossary files are created in §8.2; the translations in §8.3/§8.4; the staleness enforcement in §8.6. This ADR fixes the contract.

### 1. Layout — mirror the English path under `docs/i18n/<lang>/`

A translated page lives at `docs/i18n/<lang>/<same-relative-path-as-the-English-source>`. The English `README.md` → `docs/i18n/zh-Hans/README.md` and `docs/i18n/ja/README.md`; the English `docs/specs/01_spec_cpp_memory_pool.md` → `docs/i18n/<lang>/docs/specs/01_spec_cpp_memory_pool.md`. The 1:1 path mapping makes "which English file does this translate?" mechanical (it is the same relative path), which the §8.6 lint relies on.

### 2. The translatable surface (localized) vs. English-only

**Localized** — the reader-facing narrative a newcomer needs to evaluate and start using the library:

- `README.md` (landing page)
- `docs/specs/01_spec_cpp_memory_pool.md` (the contract / what it does)
- the getting-started / usage material (the README **Usage** section is the canonical surface today; a dedicated guide if one is later split out)
- the **patterns-catalogue overview** — the prose of `docs/patterns/README.md` (the didactic "which patterns and why"), not the per-row ADR links

**English-only** — deliberately *not* translated:

- **ADRs** (`docs/adr/`) — immutable architectural records; a translated ADR cannot be kept in lockstep with an immutable original and invites drift. (This very ADR is English-only.)
- **`CHANGELOG.md`** — an append-only ledger that grows every release; translating it is unbounded perpetual work.
- **`ROADMAP.md`** — high-churn planning that changes most PRs.
- **`AGENTS.md`**, `docs/workflow/`, `docs/development/` — the contributor/agent contract, governed by the English working-language rule (AGENTS.md §2).
- the **Doxygen API reference** — generated from English in-header comments (ADR-0013 §2); the API contract is not prose to localize.

### 3. English is normative; untranslated falls back to English

Translations are derived artifacts. Where a translation is absent or stale, the **English original is authoritative and is the fallback**: a reader follows the link to the English page. We do **not** create empty stub files for untranslated pages — coverage is recorded in the manifest and a per-language index (`docs/i18n/<lang>/README.md`, §8.2) that links untranslated pages to their English source. Every translated page carries a short header banner naming its English source and the source commit it was translated from, and stating that English is normative.

### 4. Machine-checkable translation-status manifest

A single manifest (`docs/i18n/translation-status.md`, created in §8.2) records, per (language, page): the **source path**, the **source commit hash** the translation was made from, the **translated-at commit**, a **status** (e.g. `translated` / `stale` / `missing`), and a **reviewer**. Because each entry pins the source commit, the §8.6 consistency lint can flag any translation whose recorded source commit is older than the English file's latest commit — turning staleness into a CI-detectable condition rather than a reader's surprise.

### 5. Terminology glossary

A glossary (`docs/i18n/glossary.md`, §8.2) maps each canonical term to its `zh-Hans` and `ja` rendering, **including explicit "keep in English" entries** for established technical terms (`free list`, `RAII`, `Pimpl`, `O(1)`, …) so translators are consistent and don't over-translate terms of art.

## Alternatives Considered

- **A SaaS translation platform (Crowdin / Transifex / Weblate).** Rejected: it introduces the project's first dependency on an external service in the docs pipeline, against the spec §3.3 philosophy, and is heavy machinery for a two-language, file-based didactic doc set. File-based Markdown on GitHub renders directly and needs nothing.
- **Localize everything, including ADRs / `CHANGELOG` / `ROADMAP`.** Rejected: ADRs are immutable (a translation can never be re-synced), and the ledger/roadmap churn every release, so their translations would be perpetually stale — the opposite of a trustworthy normative record. The English-working-language rule (AGENTS.md §2) already covers the contract docs.
- **`gettext` / `.po` catalogues.** Rejected: designed for short UI strings, not long-form prose; adds a tooling dependency and an extraction/merge build step for no benefit over per-page Markdown.
- **One big bilingual file per page (side-by-side English + translation).** Rejected: doubles the churn of every English edit, is hard to read, and defeats the 1:1 staleness tracking.
- **A branch (or fork) per language.** Rejected: merge-conflict treadmill, no single browsable tree, and translations drift from `master` invisibly.
- **No manifest — track staleness by convention.** Rejected: "translation silently lags source" is the dominant failure mode; without a commit-pinned manifest the §8.6 lint cannot detect it, and the system degrades to untrustworthy stale pages.

## Consequences

**Positive**

- One settled shape for every later i18n item: §8.2 scaffolds it, §8.3/§8.4 fill it, §8.6 enforces it.
- English stays the single source of truth; translations are clearly a convenience layer with an explicit fallback, so a missing/stale page never misleads.
- Staleness is a **machine-checkable** fact (manifest + §8.6 lint), not a reader's gamble.
- Zero external dependencies — consistent with the whole project; GitHub renders the translated Markdown with no toolchain.
- The translatable surface is bounded (four English areas), so the per-release translation burden is predictable.

**Negative / limitations**

- Translations are **maintained by hand** and must be re-synced when their English source changes; the manifest makes the lag visible but does not fix it. The per-release maintenance step is real (and is part of what the §8.5 maintenance protocol will codify).
- Contributors editing a localized English page should expect to mark its translations stale in the manifest (or the §8.6 lint will flag it) — a small new obligation, surfaced via §8.7.
- Two languages now; adding a third is cheap structurally (a new `<lang>/` tree + manifest column) but multiplies the translation-maintenance work.

**Documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0032.
- [ROADMAP](../../ROADMAP.md) §8.1 — checkbox flipped.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry.

The scaffold itself (`docs/i18n/README.md`, the `<lang>/` trees, `translation-status.md`, `glossary.md`) is **§8.2**, not this PR — this ADR is the decision; §8.2 is the implementation.

## References

- [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) — the Doxygen/Markdown split; i18n localizes a subset of the Markdown narrative only.
- [`AGENTS.md`](../../AGENTS.md) §2 — English as the working language for on-disk artifacts.
- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §3.3 — the zero-external-dependency philosophy.
- [ROADMAP](../../ROADMAP.md) §8.1–§8.7 — the milestone this ADR opens.
- BCP 47 language tags — `zh-Hans`, `ja`.
