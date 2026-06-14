# ADR-0013: Doxygen for the API Contract, Markdown for the Narrative

- **Status:** Accepted
- **Date:** 2026-06-11
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §9 (the coding conventions that already implicitly require Doxygen comments on every public symbol), [`AGENTS.md`](../../AGENTS.md) §10 (the enterprise quality bar that demands "Doxygen-compatible, builds without warnings"), [`docs/workflow/documentation.md`](../workflow/documentation.md) (the operational guide cross-linked from this ADR), [ROADMAP](../../ROADMAP.md) §7.1 (the future Doxygen-generated static site that closes the rendering half of this decision), [ADR-0006](0006-code-style-and-static-analysis-baseline.md) §1 (the `.clang-format` baseline that intentionally leaves Doxygen comments unmodified through formatting), [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §3.3 (the ANSI C interop contract that drives the in-header form of the API documentation).

## Context

The project has, since the Milestone 0 agent contract was written, mandated that **every public symbol carries a Doxygen-compatible comment** ([`AGENTS.md`](../../AGENTS.md) §9 — *"All public symbols documented with Doxygen-compatible comments (`///` or `/** */`). Private code commented only where the *why* is non-obvious."*) and that the enterprise quality bar treats Doxygen warnings as a build gate ([`AGENTS.md`](../../AGENTS.md) §10 — *"Public API docs | Doxygen-compatible, builds without warnings"*). [ROADMAP](../../ROADMAP.md) §7.1 closes the loop by reserving the v1.0 polish milestone for *"Doxygen-generated API documentation published as a static site"*.

In parallel, every other artifact in the repository is Markdown: [`README.md`](../../README.md), [`ROADMAP.md`](../../ROADMAP.md), [`CHANGELOG.md`](../../CHANGELOG.md), [`AGENTS.md`](../../AGENTS.md), the [ADRs](.) (twelve previous documents plus this one), the [spec](../specs/), the [patterns catalogue](../patterns/), the [workflow guides](../workflow/), the [development guides](../development/), and the per-release notes under [`docs/releases/`](../releases/). Roughly twenty-five Markdown files versus ~four to six Doxygen-commented headers — but the latter carry the *contract* surface that consumers depend on.

The split was made during Milestone 0 and has been operating in production through Milestones 1 and 2 without friction. What it has lacked, until this ADR, is a single document a contributor (or an auditor) can open to understand *why* the two formats coexist and what each is responsible for. The question came up explicitly during the M2.7 session ("wouldn't it be better to use Markdown files and then render them on a web page?") — a legitimate concern that any new contributor or compliance reviewer is likely to repeat. Without a recorded decision, the question reopens every time and the conversation has to be reconstructed.

A reference implementation that markets itself as *enterprise-grade* must also model **enterprise documentation practice**, not just enterprise code practice. The choice of documentation toolchain is a decision in its own right — one that interacts with auditing, tooling, onboarding, and long-term maintenance in ways that are not obvious from the source files alone.

## Decision

We adopt a **strict format split** between two kinds of documentation, each authoritative for its category and neither overlapping the other. The split is normative across the repository.

### 1. Documentation taxonomy

The repository contains exactly four kinds of documentation, each with one canonical format:

| Category                                            | Format       | Lives in                                                                                                | Rendered by               |
|-----------------------------------------------------|--------------|---------------------------------------------------------------------------------------------------------|----------------------------|
| **API contract** (per-symbol pre/post/throws)       | Doxygen      | `src/main/cpp/it/d4np/memorypool/*.h` and `*.hpp`, in-source as `/** … */` blocks immediately preceding the declaration | Doxygen → static HTML site (M7.1)             |
| **Narrative** (decisions, plans, runbooks, history) | Markdown     | `README.md`, `ROADMAP.md`, `CHANGELOG.md`, `AGENTS.md`, `docs/**/*.md`, `docs/releases/*.md`, `LICENSE`  | GitHub renders directly; future MkDocs / Sphinx-Breathe for a unified site post-v1.0 |
| **Implementation comments** (the *why* of a tricky line) | Free-form `//` or `/* */` | C/C++ source files, locally next to the code                                                       | Not rendered — code-reader audience |
| **Test names** (executable contract documentation)  | doctest `TEST_CASE("…")` strings | `src/test/cpp/**`                                                                                       | CI output + `ctest -V` listings |

The split is **mutually exclusive**: API contract content does not appear in Markdown files; narrative content does not appear in Doxygen comments. When a piece of documentation belongs to both (e.g., the C API contract is also described in [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §5), the spec contains the canonical *requirement* and the Doxygen comment contains the *current implementation contract* with a back-reference to the spec section.

### 2. Doxygen — what it carries and what it does not

Every public symbol declared in `memory_pool.h` or `memory_pool.hpp` (the C and C++ public surface) carries a Doxygen block with, at minimum:

- a one-line `@brief` (implicit when the first sentence ends with a period);
- one `@param <name>` per parameter, describing the precondition, the semantic, and any constraints the function imposes;
- one `@return` describing the success and failure paths;
- `@note` / `@warning` clauses for anything a caller must know that the signature does not say;
- back-references to spec sections and to amending ADRs using the form `ADR-XXXX` and `spec §X.Y`.

Doxygen comments **do not** describe rationale, history, alternatives considered, or future evolution — those are narrative concerns. A Doxygen block that drifts into "we used to do X but now we do Y" is a bug; the rationale belongs in the ADR that the comment links to.

### 3. Markdown — what it carries and what it does not

Markdown files carry the full corpus of project narrative:

- **Governance & contract** — `AGENTS.md`, `LICENSE`, [`docs/workflow/`](../workflow/).
- **Plan & history** — `ROADMAP.md`, `CHANGELOG.md`, [`docs/releases/`](../releases/).
- **Decisions** — [`docs/adr/`](.) (this document and its siblings), [`docs/patterns/`](../patterns/).
- **Specifications** — [`docs/specs/`](../specs/) (the *what we must build*, the contract above the implementation).
- **Onboarding** — [`README.md`](../../README.md), [`docs/development/`](../development/).

Markdown files **do not** redocument the per-symbol API contract. A consumer who wants to know what `memory_pool_create` returns on a misaligned `block_size` reads the Doxygen comment in `memory_pool.h`, not a Markdown file. The spec and the ADRs describe *what the contract is*; the Doxygen describes *what the current symbol promises*; the two are linked, but the in-header form is the one the IDE shows and the one the build verifies.

### 4. Rendering pipeline

For the v0.x line the project ships in source form only — both formats are read in raw shape: Markdown on GitHub's renderer, Doxygen comments by the developer's IDE on hover. No web site is published.

[ROADMAP](../../ROADMAP.md) §7.1 reserves the v1.0 polish milestone for the rendered site. The site combines both formats:

- a **Doxygen-generated** section for the API reference (every public symbol, cross-linked, with the parameter / return / throws contracts);
- a **MkDocs / Sphinx-Breathe** wrapper that adds the narrative (the ADRs, the spec, the workflow guides) around the API reference, producing one site indexed across both formats.

Sphinx-Breathe (which reads Doxygen XML output and emits Sphinx-renderable nodes) and MkDocs Material (with the `mkdocs-doxygen` plugin) are the two leading toolchain options for this; the final pick is M7.1's choice and is not pre-empted here.

### 5. Build-time enforcement

The Doxygen comments will be validated at CI time once [ROADMAP](../../ROADMAP.md) §7.1 lands. The expected configuration (a Doxyfile checked in alongside this ADR's text, but added in M7.1):

- `WARN_AS_ERROR = YES` so the CI fails the build if any public symbol is missing a comment or has a mismatched `@param` name;
- `EXTRACT_ALL = NO` and `EXTRACT_PRIVATE = NO` so only public symbols are checked;
- The `clang-tidy` baseline already required by [ADR-0006](0006-code-style-and-static-analysis-baseline.md) §2 enforces the `/** */` form is well-formed C-style.

Markdown is already validated at CI time by `docs.yml`:

- `markdownlint-cli2` against the `.markdownlint.json` baseline;
- Lychee against `**/*.md` for internal-link integrity (offline mode — only project-relative links are checked);
- the ADR-numbering and index-coverage sanity check.

The two pipelines complement each other: code-adjacent contracts are gated by the Doxygen build; narrative is gated by the docs CI.

## Alternatives Considered

- **Markdown-only — store the API contract in `docs/api/<symbol>.md`.** Rejected. Three serious problems: (a) Markdown files in a separate directory drift from the symbol they document because the editor cost is high (open another file, find the right section, edit, save) and the proximity cost is low (no warning that the file is now stale); (b) IDE tooltips on `memory_pool_create` would not pick up the Markdown — every C/C++ IDE (CLion, Visual Studio, Eclipse-CDT, VSCode + clangd) extracts Doxygen comments from the header on hover, not arbitrary Markdown from a sibling directory; (c) the typed `@param` / `@return` / `@throws` structure that audit-grade documentation requires has no Markdown equivalent the build can mechanically verify.
- **Doxygen-only — write narrative inside `/** \mainpage … */` blocks at the top of one or two big header files.** Rejected. Doxygen does support narrative through `\page` / `\mainpage` / `\subpage`, but the editor experience for the narrative is poor (writing ADRs in a `/* */` block is friction-positive), the diff experience in pull requests is poor (a narrative paragraph change is visually intermixed with code-comment churn), and non-engineer stakeholders (PMs, compliance, support) cannot read Doxygen HTML the way they read Markdown on GitHub. The narrative volume of this project (~25 Markdown files) does not fit comfortably in any header.
- **AsciiDoc / reStructuredText (RST) as the single format for both API and narrative.** Considered. AsciiDoc has the most expressive native syntax (tables, admonitions, includes, callouts, multi-page documents) of any of the lightweight markup formats, and Sphinx-RST is the LLVM / CPython / Linux Kernel choice. Rejected for two reasons specific to this project: (a) the C/C++ ecosystem expects Doxygen on header files, not RST — an RST-only project that asks contributors to also document headers in RST imposes an unfamiliar toolchain; (b) the wider engineering audience (PR reviewers, stakeholders, future contributors) is more fluent in Markdown than in RST, and the lower-friction format is the right pick for the narrative that does not require RST's heavier features.
- **Sphinx with autodoc-style extractors from C++ source (no Doxygen).** Rejected. The C/C++ autodoc bridges (Breathe, Exhale) all need Doxygen XML as input — they do not parse C/C++ directly. Removing Doxygen would mean either (a) writing API documentation twice (once in source, once in RST) or (b) losing the IDE-tooltip / build-time-check benefits of in-header docs. Neither is acceptable.
- **Custom in-house format with a custom extractor.** Rejected. The cost of building and maintaining a custom toolchain is real (the maintainer is one person; the project is a reference implementation, not a documentation experiment), the onboarding cost for contributors is high (no existing tooling recognises the format), and the audit value is zero (compliance reviewers expect Doxygen on C/C++ APIs — a custom format that does the same job is harder to justify).
- **No formal documentation; rely on code being self-explanatory.** Hard-rejected. [`AGENTS.md`](../../AGENTS.md) §10 makes documentation a quality-bar item, [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §3 implies a contract surface that must be specified, and the *reference implementation* framing of the project (a teaching artifact) requires documentation to be a deliverable. "Self-explanatory code" is the response of a maintainer who has never had to audit, onboard, or hand over a codebase.

## Consequences

**Positive**

- The documentation question — "which format for what?" — has one answer in one place, discoverable from the ADR index. Future contributors and compliance reviewers do not need to reconstruct the rationale by reading a year of git history.
- The two formats reinforce each other rather than competing. The Doxygen contract evolves with the symbol it describes (forced by co-location); the Markdown narrative evolves with the project's understanding of itself (forced by `Documentation Impact` checkboxes in every PR template — [`AGENTS.md`](../../AGENTS.md) §6.4). Neither format carries content the other is responsible for, so no drift between two parallel sources of the same truth.
- IDE tooltips, audit-grade contract structure, and tooling integration (SWIG bindings, ABI checkers, doc-portal exports) all come for free with the Doxygen choice — these are enterprise concerns the project was implicitly relying on without recording the dependency.
- The Markdown narrative is platform-portable: GitHub renders it today, Confluence / Notion / SharePoint / Backstage TechDocs all consume Markdown directly when the project (or a derivative) is mirrored into an enterprise documentation portal. No format conversion at integration time.
- The v1.0 site (M7.1) becomes a planning exercise rather than a research exercise — the formats are decided, the tooling shortlist is short (Doxygen + MkDocs Material or Doxygen + Sphinx-Breathe), and the choice is bounded.

**Negative**

- Two formats means two CI pipelines (the `docs.yml` markdownlint + Lychee path that exists today, and the Doxygen-warn-as-error path that lands in M7.1). The build-time cost is small but real, and each pipeline has its own quirks that contributors must learn (the eight gotchas the agent's project-scoped memory records for the clang-tidy / clang-format side have a small but real Doxygen analogue waiting to be discovered when M7.1 lands — `@param` name typos, missing `@return` on non-void functions, etc.).
- In-source Doxygen blocks are visually noisy when reading the header file directly — `/** … */` blocks of 20–30 lines for a 1-line declaration are the norm for fully-specified public functions. The cost is borne by the C/C++ engineer reading the source; the corresponding benefit is the IDE tooltip the same engineer sees when they hover over the symbol from a consumer file. The trade-off is asymmetric in time spent: source-reading is rare, hover-checking is constant.
- The "code and docs ship together" rule from [`docs/workflow/documentation.md`](../workflow/documentation.md) requires every API-changing PR to also touch the Doxygen comments. This is enforced today by the PR template's *Documentation Impact* checkbox — a process control, not a technical one. The risk of a contributor skipping the checkbox is real until M7.1's Doxygen-warn-as-error CI gate adds the technical backstop.

**Required documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0013 (Accepted).
- [`AGENTS.md`](../../AGENTS.md) §9 — the existing clause *"All public symbols documented with Doxygen-compatible comments…"* gets a one-sentence cross-link to this ADR so the *why* is one hop away. The wording does not change; only the cross-reference is added.
- [`docs/workflow/documentation.md`](../workflow/documentation.md) — the existing operational guide gains a *Format* section that points at this ADR for the decision and at the four-row taxonomy table above for the operational mapping.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry recording the ADR.

[ROADMAP](../../ROADMAP.md) item 7.1 is **not** edited here. It already names "Doxygen-generated API documentation published as a static site" and that name remains correct; this ADR explains *why* Doxygen is the format and lets M7.1 focus on the rendering pipeline rather than the format choice.

## References

- [`AGENTS.md`](../../AGENTS.md) §9 — the existing "Doxygen-compatible comments" clause this ADR formalises.
- [`AGENTS.md`](../../AGENTS.md) §10 — the enterprise quality bar that already requires Doxygen-clean builds.
- [`docs/workflow/documentation.md`](../workflow/documentation.md) — the operational guide updated in lockstep with this ADR.
- [ROADMAP](../../ROADMAP.md) §7.1 — the v1.0 rendering milestone this ADR sets the format constraint for.
- [ADR-0006](0006-code-style-and-static-analysis-baseline.md) §1 — `.clang-format` deliberately leaves Doxygen comments untouched through formatting, so the in-header form is stable across format passes.
- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §3.3 — the ANSI C interop contract that constrains the in-header documentation to ANSI-C-friendly `/* */` blocks (the M1.10 verification job compiles `memory_pool.h` under `-std=c89 -pedantic -Werror`).
- Doxygen manual — [`https://www.doxygen.nl/manual/`](https://www.doxygen.nl/manual/). The `@param` / `@return` / `@throws` commands referenced in §2 above.
- Keep a Changelog 1.1.0 — [`https://keepachangelog.com/en/1.1.0/`](https://keepachangelog.com/en/1.1.0/). The Markdown format `CHANGELOG.md` adheres to per [ADR-0004](0004-versioning-and-release-policy.md) §3.
- Sphinx-Breathe — [`https://breathe.readthedocs.io/`](https://breathe.readthedocs.io/). The candidate bridge for combining Doxygen XML and Sphinx-rendered narrative under one site in M7.1.
