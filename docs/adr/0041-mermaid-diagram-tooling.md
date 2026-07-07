# ADR-0041: Mermaid as the in-repo diagram tooling

- **Status:** Accepted
- **Date:** 2026-07-07
- **Deciders:** Daniel Polo (maintainer / project architect)
- **Related:** issue #105, spec [§4.2](../specs/01_spec_cpp_memory_pool.md#42-component-diagram-c4), [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md), [ADR-0032](0032-documentation-i18n-architecture.md)

## Context

The specification review (#105) asked for a **C4 component diagram** of the pool
internals. Until now the project has expressed architecture purely in prose plus one
ASCII sketch of the intrusive free list (spec §4). No diagram-authoring convention exists
yet, so adding the first real diagram forces a one-time, repo-wide decision about *how*
diagrams are authored, versioned, and rendered.

The forces are specific to this repository:

- **Docs are Markdown**, rendered in three places: on GitHub, in the Doxygen HTML API site
  ([ADR-0027](0027-doxygen-html-site-and-publication-pipeline.md)), and mirrored by the
  `zh-Hans` / `ja` translations ([ADR-0032](0032-documentation-i18n-architecture.md)).
- **Docs pay for zero build tooling.** The only doc gates are `markdownlint`, the Lychee
  link check, and a dependency-free (pure-stdlib Python) consistency lint. Introducing a
  renderer that needs Java, Graphviz, or a network round-trip would be a large new
  dependency for a single picture.
- **A diagram must render for a reader browsing GitHub** with no local toolchain, and must
  **diff cleanly** in review like the rest of the docs.

## Decision

We adopt **Mermaid** as the canonical in-repo diagramming tool. Diagrams are authored as
fenced ` ```mermaid ` code blocks embedded directly in the Markdown that discusses them.
GitHub renders Mermaid natively, so no build step, no checked-in raster/vector binary, and
no external renderer is introduced. For **C4-model** diagrams we express the *Component*
level as a Mermaid `flowchart` whose `subgraph` blocks delimit the C4 boundaries (system /
public surface / core engine / external store), rather than Mermaid's experimental
`C4Component` DSL. The tiny ASCII free-list sketch in §4 stays as-is — it is a byte-level
memory layout, not an architecture diagram, and reads perfectly as text.

## Alternatives Considered

- **PlantUML (including the C4-PlantUML macro library).** Rejected: it needs a Java +
  Graphviz renderer or a server round-trip, and GitHub does not render PlantUML inline. We
  would have to check in generated SVG/PNG and keep it in lockstep with its source — exactly
  the silent drift the consistency lint exists to prevent, and an awkward fit for the i18n
  mirrors.
- **Checked-in SVG/PNG exported from a GUI (draw.io, Excalidraw, …).** Rejected: binary
  artifacts do not diff in review, go stale invisibly, and bloat the repository; there is no
  textual source of truth a reviewer can read.
- **Mermaid's native `C4Component` DSL.** Rejected *for now*: it is marked experimental in
  Mermaid, and its automatic layout produces overlapping, hard-to-read output for the
  ~10-component diagram this project needs. A `flowchart` with explicit `subgraph`
  boundaries renders reliably on GitHub and gives us layout control. The C4 *model* (its
  levels, boundaries, and typed relationships) is notation-independent, so drawing it with a
  flowchart is fully faithful to C4.
- **ASCII art only (the status quo).** Kept for the small in-band-pointer sketch, but
  rejected as the general tool: it does not scale to a multi-component diagram with typed,
  directional relationships and reads poorly beyond a handful of boxes.

## Consequences

- **Wins.** No new build or tooling dependency; each diagram lives next to the prose it
  illustrates, diffs as plain text, and renders on GitHub, in editors (VS Code), and in
  mkdocs-material-style viewers. Where Mermaid is unavailable, the reader still sees a
  legible source block rather than a broken image.
- **Costs / limits.** Rendering depends on the viewer; a bare `cat` of the file shows the
  Mermaid source, not a picture. Complex graphs can hit Mermaid layout quirks — mitigated by
  keeping each diagram to a single C4 level and using explicit `subgraph`s for structure.
- **i18n.** A Mermaid block inside a translated source counts as source content: translators
  copy it verbatim (the labels are English identifiers and pattern names) or localize the
  labels, and the change is tracked like any other source edit through the
  `translation-status.md` manifest.
- **Tooling / testing.** No CI change. `markdownlint` already tolerates the block (`MD013`
  and `MD040` are disabled repo-wide), and the Lychee link check ignores code fences.
- **Documentation.** This ADR is the convention; the first diagram lands in spec §4.2.
  Future diagrams (a Container-level view, per-subsystem sequence diagrams, …) follow the
  same rule and cite this ADR.

## References

- The C4 model — <https://c4model.com> (notation-independent architecture diagramming).
- Mermaid — <https://mermaid.js.org> and GitHub's native Mermaid rendering.
- [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) — Doxygen for the API contract,
  Markdown for the narrative.
- [ADR-0032](0032-documentation-i18n-architecture.md) — documentation i18n architecture.
- Issue #105 — specification review that requested the C4 diagram.
