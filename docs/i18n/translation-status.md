# Translation status manifest

Machine-checkable record of every translation's freshness, per [ADR-0032](../adr/0032-documentation-i18n-architecture.md) §4. Each row pins the **source commit** a translation was made from, so a translation is **stale** when its English source has changed since that commit. The consistency lint (ROADMAP §8.6) reads this file.

Status vocabulary:

| Status | Meaning |
|--------|---------|
| `missing` | No translation exists yet; readers fall back to the English source. |
| `translated` | Up to date with the recorded source commit. |
| `stale` | The English source changed after the recorded source commit; needs a refresh. |

`Source commit` is the short SHA of the English source's latest commit at translation time; `Translated at` is the commit that added/updated the translation. Both are `—` while a row is `missing`.

## `zh-Hans` (Simplified Chinese)

| Source page | Source commit | Translated at | Status | Reviewer |
|-------------|:-------------:|:-------------:|:------:|----------|
| [`README.md`](../../README.md) | `d38b598` | `d38b598` | `stale` | — |
| [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) | `2e55dfa` | `2e55dfa` | `stale` | — |
| [`docs/patterns/README.md`](../patterns/README.md) | `524f0cc` | `524f0cc` | `translated` | — |

## `ja` (Japanese)

| Source page | Source commit | Translated at | Status | Reviewer |
|-------------|:-------------:|:-------------:|:------:|----------|
| [`README.md`](../../README.md) | `d38b598` | `d38b598` | `stale` | — |
| [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) | `612f9d2` | `612f9d2` | `stale` | — |
| [`docs/patterns/README.md`](../patterns/README.md) | `6c6aeb7` | `6c6aeb7` | `translated` | — |

> Seeded by ROADMAP §8.2 with the full translatable surface at `missing`. The
> `zh-Hans` rows are filled by §8.3 and the `ja` rows by §8.4, each recording the
> source commit it was translated from.
