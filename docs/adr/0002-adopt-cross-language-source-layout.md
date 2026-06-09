# ADR-0002: Adopt a Maven-style cross-language source layout

- **Status:** Accepted
- **Date:** 2026-06-09
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §5, [`src/README.md`](../../src/README.md)

## Context

`pbr-cpp-memory-pool` is the first concrete reference in the **Purpose-Built References (PBR)** series. The series is explicitly multi-language: future siblings will implement the same kind of high-performance building blocks in Java, Python, Rust, Go, and others. Each reference is consumed primarily for *reading* — newcomers open the repo, navigate to the production sources, and expect to recognise the layout within seconds.

C++ projects historically use heterogeneous layouts (`src/` + `include/`, `source/` + `headers/`, or everything at the repo root). None of those layouts maps cleanly onto Java, Python, or Rust conventions, and none of them carries an explicit *organisational* root segment. As the PBR series grows, a divergent layout per language would force readers to relearn the structure on every new repo and would make automated tooling (search, code metrics, dashboards) harder to share.

We also want the layout itself to communicate the project's organisational provenance — the reverse-domain `it/d4np/` segment makes it obvious that every PBR project belongs to the same family, the same way a Java package coordinates do.

## Decision

We adopt the **Maven Standard Directory Layout**, generalised across languages, as the canonical source-tree shape for every PBR project:

```
src/main/<lang>/it/d4np/<project>/    # production sources
src/test/<lang>/it/d4np/<project>/    # test sources
src/bench/<lang>/it/d4np/<project>/   # benchmarks (where applicable)
src/main/<lang>/resources/            # non-source assets, when needed
```

For this repository specifically:

- `<lang>` = `cpp`
- `<project>` = `memorypool`
- All C and C++ files (public headers, private headers, translation units) live under `src/main/cpp/it/d4np/memorypool/`.
- The C++ namespace mirrors the path 1:1 → **`it::d4np::memorypool`**.
- The build system exposes `src/main/cpp` as the public include root, so includes look like `#include <it/d4np/memorypool/memory_pool.h>`.

Public/private header separation is handled *within* `memorypool/` (private code lives under `detail/` or is suffixed `_internal`), not by splitting into a parallel `include/` tree.

## Alternatives Considered

- **Conventional C++ `src/` + `include/` split.** Rejected because it has no analogue in Java/Python/Rust and would force per-language exceptions to the PBR layout convention. The public/private header concern it solves is real but can be handled by intra-component subfolders.
- **Flat layout (`source files at repo root or under a single `src/<project>/`).** Rejected because it scales poorly past one component and conveys no organisational information.
- **CMake "modern" target layout (one folder per CMake target, headers next to sources).** Rejected as the *outer* convention because it is C++-only; we can still apply target-level organisation *inside* `memorypool/` once the codebase grows.
- **Reverse-domain segment with a different organisational root** (e.g., `com/pbr/`, `org/d4np/`). Rejected to keep alignment with the user's existing `it.d4np` identity and Italian ccTLD; switching would also force a rename across every sibling project.

## Consequences

**Positive**

- One layout, recognisable across the entire PBR series, regardless of language.
- The reverse-domain segment doubles as an organisational signature inside the source tree, matching what a Java Maven artifact would advertise externally.
- Tooling (find/grep across the series, metric dashboards, CI templates) can target identical paths.
- For C++ specifically, the `it::d4np::memorypool` namespace falls out of the path mechanically — no impedance mismatch between filesystem and language.
- Tests and benchmarks are first-class siblings of production code (`src/test/...`, `src/bench/...`), rather than ad-hoc folders.

**Negative**

- Unusual depth for a C++ project (six levels before any source file). Mitigated by the build system handling the include path; consumers see `<it/d4np/memorypool/...>` and never type the full filesystem path by hand.
- Tools that assume `include/` for public headers (some IDEs, some package managers) need explicit configuration. Documented in the build setup (Milestone 1.2).
- Installation/export of public headers requires an explicit selection step in CMake (e.g., `install(DIRECTORY ... FILES_MATCHING)` with header patterns), because they are not isolated in a dedicated tree. A follow-up ADR will lock down the install layout once Milestone 6 is reached.

## References

- Maven Standard Directory Layout — <https://maven.apache.org/guides/introduction/introduction-to-the-standard-directory-layout.html>
- [`AGENTS.md`](../../AGENTS.md) §5 — normative statement of the layout.
- [`src/README.md`](../../src/README.md) — operational notes for working in the tree.
