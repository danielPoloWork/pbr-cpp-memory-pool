# ADR-0033: English as the specification's normative language

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0032](0032-documentation-i18n-architecture.md) (the i18n system that assumes a single English-normative source), [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) (doc-format taxonomy), [`AGENTS.md`](../../AGENTS.md) §2 (English working-language rule), [`docs/workflow/documentation.md`](../workflow/documentation.md) (specs are frozen contracts), [ROADMAP](../../ROADMAP.md) §8.3 / §8.4 (the translations this unblocks).

## Context

The specification [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) was authored in **Italian** — it is the project's original contract, carried verbatim since Milestone 0 (preserved in git history at commit `3ccff68` and earlier). Two standing rules sit in tension with that:

- [`AGENTS.md`](../../AGENTS.md) §2 mandates **English for every on-disk artifact**; the Italian spec has been the one long-standing exception.
- [ADR-0032](0032-documentation-i18n-architecture.md) just established a documentation-translation system whose central premise is that **English is the single normative source**, and which banners every translated page with *"English is normative — if this differs from the source, the English version wins."*

Milestone 8.3 / 8.4 translate the spec into Simplified Chinese and Japanese. Translating from the Italian original would make the "English is normative" banner incoherent for the spec, and would leave the spec with **no English baseline** — the project's working audience (and every other artifact) is English. The spec needs a single, English, normative source before it can be localized consistently.

## Decision

We **translate the specification to English in place**, so `docs/specs/01_spec_cpp_memory_pool.md` becomes the **English normative source**. The translation is **faithful**: every functional and non-functional requirement, the API surface, the Free List description, the diagram, and the verification strategy are preserved with identical meaning — only the prose language changes. The spec's **"frozen contract" property is about its requirements, not its language**, so this is a presentation change, not a contract change, recorded here per the [documentation.md](../workflow/documentation.md) rule that spec edits carry a paper trail. The **Italian original is preserved in git history** (commit `3ccff68`); no Italian sibling file is kept (Italian is not an i18n target language — see Alternatives). With this, the spec joins every other artifact under the AGENTS.md §2 English rule and is translatable like the rest of the surface (zh-Hans in §8.3, ja in §8.4) with a coherent banner.

## Alternatives Considered

- **Keep the spec in Italian; translate each target language from the Italian original.** Rejected: it leaves the AGENTS.md §2 inconsistency in place, makes the ADR-0032 "English is normative" banner false for the spec, and gives the spec **no single normative source** — Italian would be a de-facto source that no English-working contributor maintains, inviting drift between the Italian spec and every English document that references it.
- **Keep Italian and add Italian (`it`) as an i18n target language.** Rejected: Italian was the *original*, not a translation; modelling it as a target inverts the relationship, and `it` is not in the project's chosen language set (`zh-Hans`, `ja`, ADR-0032).
- **A dual-language spec file (Italian + English side by side).** Rejected: doubles churn, and re-introduces the "which language wins" ambiguity this ADR exists to remove.
- **Leave the spec untranslated and drop it from the i18n surface.** Rejected: it is explicitly part of the translatable surface (ADR-0032 §2) and is the contract a newcomer most needs; and it would still leave the §2 inconsistency.

## Consequences

**Positive**

- One coherent rule: **English is normative everywhere**, including the spec — the ADR-0032 banner and the §8.6 lint now hold uniformly.
- AGENTS.md §2 is satisfied with no standing exception.
- The spec is now readable by the project's English-working audience, and §8.3 / §8.4 translate from a clean English source.

**Negative / limitations**

- The frozen spec's file changed language. Mitigated: the translation is faithful (requirements unchanged), this ADR is the paper trail, and the Italian original remains in git history. A reviewer should diff the English against commit `3ccff68` to confirm no requirement shifted.
- The original Italian prose is no longer the working file (only in history) — an acceptable loss given the original author authored both the Italian and this decision.

**Documentation updates landing in the same PR**

- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) — translated to English in place.
- [`docs/adr/README.md`](README.md) — index row for ADR-0033.
- [`docs/workflow/documentation.md`](../workflow/documentation.md) — a note that the spec is maintained in English (this ADR).
- [ROADMAP](../../ROADMAP.md) — a new Milestone 8 item (§8.9) recording this prerequisite; checkbox checked.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Changed` entry.

## References

- [ADR-0032](0032-documentation-i18n-architecture.md) — the i18n architecture this unblocks.
- [`AGENTS.md`](../../AGENTS.md) §2 — the English working-language rule now uniformly satisfied.
- [`docs/workflow/documentation.md`](../workflow/documentation.md) — specs are frozen; edits carry a paper trail (this ADR).
- The Italian original — git history at commit `3ccff68` (and earlier).
