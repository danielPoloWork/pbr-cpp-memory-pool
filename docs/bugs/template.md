---
id: BUG-NNNN
title: <one-line, imperative or noun description of the defect>
status: open
severity: low
reporter: internal
discovered: YYYY-MM-DD
affected-versions: <e.g. ">=1.0.0,<1.1.1", or "unknown">
fixed-in: <vX.Y.Z once closed; leave empty while open>
---

# BUG-NNNN: <same title as the frontmatter>

> Copy this file to `docs/bugs/<YYYY>/<MM>/BUG-NNNN-<short-kebab-slug>.md`, fill the
> frontmatter, and add an index row to [`README.md`](README.md). `NNNN` is the next
> free globally-monotonic number (see the index). Relative links to repo files use
> paths relative to this depth: `../../../` reaches `docs/`, `../../../../` the root.
> Delete this quote block in the real record.

## Summary

One or two sentences: what goes wrong, observed from the outside.

## Environment

- **Affected versions:** <as in frontmatter>
- **Toolchain / platform:** <compiler + version, OS, arch — when relevant>
- **Configuration:** <relevant compile-time knobs, e.g. `PBR_MEMORY_POOL_THREAD_SAFETY`>

## Reproduction

The smallest steps or code that trigger the defect. A failing test is ideal — link it.

```cpp
// minimal repro
```

## Expected vs. actual

- **Expected:** <what should happen>
- **Actual:** <what happens instead>

## Root cause

The underlying reason, once understood. For an `open` record this may be a hypothesis;
mark it as such. For `cannot-reproduce`/`rejected`, document *why* the report did not
hold up — this is the value of recording it.

## Impact

Who/what is affected and how badly. Justifies the `severity`.

## Fix / workaround

The fix (link the PR and the `CHANGELOG` `Fixed` line once closed), or the interim
workaround for consumers while the record is `open`.

## References

- Fixing PR: <#NNN, once closed>
- `CHANGELOG` entry: <link to the `Fixed` line / `docs/changelog/...` once released>
- Related: <ADR-XXXX, spec section, upstream issue, related BUG-NNNN>
