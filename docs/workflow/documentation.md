# Documentation Maintenance

How `README.md`, `ROADMAP.md`, `docs/specs/`, and `docs/adr/` stay current as the project evolves. The short version is in [`AGENTS.md`](../../AGENTS.md) §6; this document is the operational guide. The choice of *which format for which kind of documentation* is locked in [ADR-0013](../adr/0013-doxygen-for-api-markdown-for-narrative.md) — *Doxygen for the API contract, Markdown for the narrative*; see the *Format* section below for the operational mapping.

## Principle

**Code and docs ship together.** No "docs follow-up later" pull requests. If a change touches a public surface, a workflow, or a non-trivial design decision, the same PR carries the documentation update.

## Per-PR checklist

The PR template includes a *Documentation Impact* section. The agent fills it in honestly:

- [ ] `README.md` updated — if the user-visible surface, build commands, or quickstart changed.
- [ ] `ROADMAP.md` checkbox flipped — for any item the PR completes; new items appended if scope expanded.
- [ ] ADR added / updated — for any non-trivial design choice.
- [ ] Spec updated — if implementation diverges from `docs/specs/`.

If every box is unchecked, the PR body must still keep the section in place, with each line annotated `N/A — <reason>`. Empty checklists are a smell.

## README.md

`README.md` is the project's landing page. Keep it short, current, and pointing outward to the durable docs.

Sections to keep alive:

- **What it is** — one paragraph, current as of the latest spec revision.
- **Status** — a single line: "early scaffolding", "MVP", "stable", etc.
- **Build / Test** — exact commands, kept in sync with the actual build system.
- **Layout** — pointers to `AGENTS.md`, `ROADMAP.md`, `docs/`.
- **License** — pointer to `LICENSE`.

Do not embed long design discussions in the README — those belong in ADRs.

## ROADMAP.md

A numbered, checkbox-driven plan grouped by milestone. Conventions:

- One root-level milestone per major project phase (e.g., *0 — Agent & Workflow Scaffolding*, *1 — Core Memory Pool MVP*, *2 — Thread-Safe Variant*).
- Items are numbered within their milestone: `1.1`, `1.2`, …
- Unchecked: `- [ ] 1.1 …`. Checked: `- [x] 1.1 …`. Do not delete completed items — the list is a record, not a queue.
- When work splits or new items appear, append at the end of the relevant milestone with a fresh number; do not renumber existing items.
- Each PR that completes a roadmap item flips the corresponding checkbox in the same PR.

## Format

Two formats coexist, with strict separation per [ADR-0013](../adr/0013-doxygen-for-api-markdown-for-narrative.md):

| Kind of documentation                                            | Format                                | Where it lives                                                                                       |
|------------------------------------------------------------------|---------------------------------------|------------------------------------------------------------------------------------------------------|
| **API contract** — per-symbol pre/post/throws                    | Doxygen `/** … */`                    | In the public headers: `src/main/cpp/it/d4np/memorypool/*.h` and `*.hpp`, immediately above the declaration |
| **Narrative** — decisions, plans, runbooks, history, onboarding  | Markdown                              | `README.md`, `ROADMAP.md`, `CHANGELOG.md`, `AGENTS.md`, `docs/**/*.md`                                |
| **Implementation comments** — the *why* of a tricky line         | Free-form `//` or `/* */`             | Inside the C/C++ source file, locally next to the code                                               |
| **Test names** — executable contract documentation               | `doctest TEST_CASE("…")` strings      | `src/test/cpp/**`                                                                                    |

The split is mutually exclusive. API-contract content does not appear in Markdown; narrative content does not appear in Doxygen blocks. When a piece of documentation is conceptually both (e.g., the C API contract is described in [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §5 *and* in the Doxygen of `memory_pool.h`), the spec carries the canonical *requirement* and the Doxygen carries the *current implementation contract* with a back-reference to the spec section.

A Doxygen block that drifts into rationale or history is a bug — that content belongs in an ADR. A Markdown file that re-documents the per-symbol API contract is a bug — that content belongs in the Doxygen comment in the header. The build-time and CI-time enforcement is staged: `docs.yml` already validates Markdown (markdownlint + Lychee + ADR sanity); the Doxygen-warn-as-error gate lands in [ROADMAP](../../ROADMAP.md) §7.1 alongside the rendered static-site pipeline.

## ADRs

See [`docs/adr/README.md`](../adr/README.md) for the full lifecycle. In short:

- One Markdown file per decision, numbered sequentially.
- Use [`docs/adr/template.md`](../adr/template.md) as the skeleton.
- Once accepted, the ADR is immutable except for the `Status` line. To change a decision, write a new ADR that supersedes the old one.
- Update the ADR index table in [`docs/adr/README.md`](../adr/README.md) in the same PR.

## Specs

Files under `docs/specs/` are **frozen contracts**. If implementation diverges:

1. **Either** update the spec and document the change in the PR body, with a brief rationale, or
2. **Or** add an ADR that records the deviation and explicitly references the spec section it supersedes.

Never let the code and the spec drift without a paper trail.

The spec is maintained in **English** (the normative source), per [ADR-0033](../adr/0033-english-as-the-spec-normative-language.md) — it was originally authored in Italian and translated to English in place so the whole repository is uniformly English-normative (AGENTS.md §2) and the spec can be localized like the rest of the translatable surface ([ADR-0032](../adr/0032-documentation-i18n-architecture.md)). "Frozen" governs the *requirements*, not the language.

## Language and style

- English only, in every file under `docs/`.
- Active voice. Imperative mood for instructions, declarative for explanations.
- Concrete over abstract: "store the next-free pointer in the first bytes of each free block" beats "use an efficient linkage scheme".
- Link liberally between docs using relative paths.
- No screenshots without alt-text. Diagrams as ASCII or as source-controlled SVG; never as opaque PNGs alone.
