# Documentation translations (i18n)

Translations of the project's reader-facing documentation. **English is the single normative source** — a translation is a convenience layer, never a second source of truth, and where it conflicts with the English original the English wins. The architecture is fixed by [ADR-0032](../adr/0032-documentation-i18n-architecture.md); this guide is the operational how-to.

This guide is intentionally English-only (it is contributor/agent tooling, per [`AGENTS.md`](../../AGENTS.md) §2).

## Languages

| Code | Language | Index |
|------|----------|-------|
| `zh-Hans` | Simplified Chinese (简体中文) | [`zh-Hans/README.md`](zh-Hans/README.md) |
| `ja` | Japanese (日本語) | [`ja/README.md`](ja/README.md) |

## What is translated (the translatable surface)

Only the reader-facing narrative a newcomer needs to evaluate and start using the library (ADR-0032 §2):

| English source | What it is |
|----------------|------------|
| [`README.md`](../../README.md) | Landing page (incl. the **Usage** section) |
| [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) | The specification / contract |
| [`docs/patterns/README.md`](../patterns/README.md) | The patterns-catalogue overview prose |

**Not translated** (English-only, by design): the ADRs (immutable records), `CHANGELOG.md` and `ROADMAP.md` (high-churn), `AGENTS.md` and the workflow / development guides (contributor contract), and the Doxygen API reference (generated from English in-header comments). See ADR-0032 §2 for the rationale.

## Layout

A translated page lives at the **same relative path** as its English source, under `docs/i18n/<lang>/`:

```text
docs/i18n/zh-Hans/README.md                              ← translates ../../README.md
docs/i18n/zh-Hans/docs/specs/01_spec_cpp_memory_pool.md  ← translates ../../docs/specs/01_spec_cpp_memory_pool.md
docs/i18n/zh-Hans/docs/patterns/README.md                ← translates ../../docs/patterns/README.md
docs/i18n/ja/...                                         ← same shape
```

The 1:1 path mapping is what lets the consistency lint (ROADMAP §8.6) pair each translation with its source mechanically. **Untranslated pages are not created as empty stubs** — coverage is recorded in [`translation-status.md`](translation-status.md) and in each language's index, which links untranslated pages to their English original (the fallback).

## Adding or updating a translation

1. Copy the English source to the mirrored path under `docs/i18n/<lang>/` and translate the prose. Keep code blocks, identifiers, symbol names, and the terms marked *keep in English* in [`glossary.md`](glossary.md) **unchanged**; use the glossary's renderings for everything else so terminology stays consistent.
2. Start the page with the standard banner (so a reader always knows it is a derived, possibly-stale artifact):

   ```markdown
   > 🌐 Translation of [`<relative/path/to/english>`](...) as of commit `<short-sha>`.
   > **English is normative** — if this differs from the source, the English version wins.
   ```

3. Record it in [`translation-status.md`](translation-status.md): the source path, the **source commit hash** you translated from (`git rev-parse --short HEAD` of the English file's latest commit), the translated-at commit, the status, and the reviewer.
4. Add the page to the language's index (`<lang>/README.md`).

When you **edit an English source** that has translations, mark those manifest rows `stale` (the §8.6 lint will otherwise flag the commit-hash drift).

## Staleness

`translation-status.md` pins each translation to the source commit it was made from. A translation is **stale** when its source page has changed since that commit. The forthcoming consistency lint (ROADMAP §8.6) turns this into a CI-checkable condition; until then it is a manual check against the manifest.

## Terminology

[`glossary.md`](glossary.md) is the canonical term map (English ↔ `zh-Hans` ↔ `ja`), including explicit *keep in English* entries for terms of art (`free list`, `RAII`, `Pimpl`, `O(1)`, the GoF pattern names, public identifiers). Translators must follow it.
