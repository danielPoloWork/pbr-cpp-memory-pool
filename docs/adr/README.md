# Architecture Decision Records

This directory captures the architectural decisions made on `pbr-cpp-memory-pool` as lightweight, immutable Markdown records — one decision per file.

## Format

Each ADR follows the [Michael Nygard format](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions) and is created from [`template.md`](template.md). File naming: `NNNN-short-kebab-title.md`, where `NNNN` is a zero-padded sequential number.

## Lifecycle

```
Proposed ─► Accepted ─┬─► Superseded by ADR-XXXX
                     └─► Deprecated
```

- **Proposed** — decision drafted, under discussion. Allowed to evolve.
- **Accepted** — decision is in force. The record is now **immutable**; only the `Status` line may change.
- **Superseded** — a newer ADR replaces this one. Both records remain in the repo; the older one points to the newer one in its `Status` line, the newer one references the older in `Context`.
- **Deprecated** — the decision no longer applies but is preserved for historical context.

## When to write an ADR

Write one when a decision:

- affects the public C or C++ API of the pool;
- affects ABI, alignment, or thread-safety guarantees;
- has two or more reasonable alternatives and the rationale is not obvious from the code;
- supersedes or amends a prior ADR.

Do **not** write one for purely local implementation details, formatting, or trivially reversible choices — those belong in the commit body.

## Index

| Number | Title                                                                                                | Status   |
|--------|------------------------------------------------------------------------------------------------------|----------|
| 0001   | [Record architecture decisions](0001-record-architecture-decisions.md)                               | Accepted |
| 0002   | [Adopt a Maven-style cross-language source layout](0002-adopt-cross-language-source-layout.md)       | Accepted |
| 0003   | [Design Patterns Policy](0003-design-patterns-policy.md)                                             | Accepted |
| 0004   | [Versioning and Release Policy](0004-versioning-and-release-policy.md)                               | Accepted |
| 0005   | [Toolchain Matrix and Supported Platforms](0005-toolchain-matrix-and-supported-platforms.md)         | Accepted |
| 0006   | [Code style and static-analysis baseline](0006-code-style-and-static-analysis-baseline.md)           | Accepted |
| 0007   | [Test framework — doctest](0007-test-framework-doctest.md)                                           | Accepted |
| 0008   | [Delegate tag creation and push to the agent](0008-delegate-tag-creation-and-push-to-the-agent.md)   | Accepted |
| 0009   | [Free-list layout, block_size constraints, and alignment guarantee](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) | Accepted |
| 0010   | [RAII for the Pool wrapper and Pimpl across the C/C++ boundary](0010-raii-pool-wrapper-and-pimpl-across-the-c-cpp-boundary.md) | Accepted |
| 0011   | [Factory Method and Builder for Pool construction](0011-factory-method-and-builder-for-pool-construction.md) | Accepted |
| 0012   | [Foreign-pointer and out-of-range pointer policy](0012-foreign-pointer-and-out-of-range-pointer-policy.md) | Accepted |
| 0013   | [Doxygen for the API contract, Markdown for the narrative](0013-doxygen-for-api-markdown-for-narrative.md) | Accepted |

When adding a new ADR, append a row to this table in the same PR.
