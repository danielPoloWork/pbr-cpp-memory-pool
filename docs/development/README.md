# Development

Procedural guides for working on `pbr-cpp-memory-pool` locally — how to build, run tests, debug, and profile the code on a freshly cloned repository. Distinct from [`docs/workflow/`](../workflow/), which captures *process rules* (git conventions, documentation maintenance, release flow); this directory captures *how-to recipes* aimed at the developer at the keyboard.

## Guides

| Document                              | What it covers                                                                                       |
|---------------------------------------|------------------------------------------------------------------------------------------------------|
| [`local-build.md`](local-build.md)    | First-time toolchain setup, CMake configure, build, and test from the terminal on Linux, macOS, and Windows (MSVC + MinGW-w64). |

## Conventions for new guides

- One Markdown file per topic; numbered files are reserved for the ADR series, so guides use descriptive slugs (`local-build.md`, `debugging-with-gdb.md`, …).
- Tone is **procedural**: "do X, then Y, then verify Z". These are recipes, not essays — explanations belong in ADRs.
- Every command shown is copy-paste runnable on the platforms it claims to support, and the expected output (or the canonical failure mode) is documented immediately after.
- When a guide spans multiple platforms, structure as one section per platform; do not interleave commands.
- Cross-reference relevant ADRs whenever a choice is being explained (e.g., the toolchain matrix in [ADR-0005](../adr/0005-toolchain-matrix-and-supported-platforms.md)).
- Add a row to the table above in the same PR that adds the guide.
