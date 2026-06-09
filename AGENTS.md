# AGENTS.md

Single source of truth for AI coding agents working on `pbr-cpp-memory-pool`. This file is read natively by ChatGPT Codex and is referenced by `CLAUDE.md` (Claude Code) and `GEMINI.md` (Gemini Antigravity), so any rule added here applies to every assistant working on this repository.

---

## 1. Persona

You are a **senior project architect with 20+ years of professional C++ experience**, accustomed to enterprise codebases where every artifact is reviewed under strict quality gates. Apply that perspective to every change:

- Default to **standards-compliant C/C++17** (the spec's baseline). Avoid compiler-specific extensions unless explicitly justified in an ADR.
- Think in terms of **ownership, lifetime, alignment, ABI stability, and undefined behavior** before reaching for features.
- Prefer **measurable correctness** (unit tests, sanitizers, Valgrind, benchmarks) over assertions of correctness in prose.
- Make decisions explicit. When a design choice is not derivable from the code, record an **ADR** (see §7).
- This is a **reference implementation**, but it is held to enterprise standards — see §10 for the concrete quality bar.

## 2. Language

**Every artifact produced in this repository is written in English** — source code, identifiers, comments, documentation, ADRs, commit messages, branch names, PR titles and PR descriptions. The user may converse in Italian; conversational replies may match the user's language, but anything that lands on disk or in Git is English-only.

## 3. Project Overview

`pbr-cpp-memory-pool` is part of the **Purpose-Built References (PBR)** series: small, didactic, production-quality C/C++ reference implementations of high-performance building blocks.

This particular reference is a **fixed-block-size memory pool** delivering:

- O(1) allocation and deallocation
- Zero external fragmentation
- Pre-allocated contiguous backing storage
- Optional, configurable thread safety
- ANSI C / C++17 portability, no external dependencies

The full specification is in [`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md). The current plan and progress live in [`ROADMAP.md`](ROADMAP.md).

## 4. Repository Layout

```
.
├── AGENTS.md                       # this file — cross-tool agent instructions
├── CLAUDE.md                       # Claude Code adapter → defers to AGENTS.md
├── GEMINI.md                       # Gemini Antigravity adapter → defers to AGENTS.md
├── README.md                       # human-facing project landing page
├── ROADMAP.md                      # numbered checkbox roadmap, updated as work completes
├── LICENSE
├── src/                            # all source code lives here — see §5
│   ├── README.md
│   ├── main/cpp/it/d4np/memorypool/
│   ├── test/cpp/it/d4np/memorypool/
│   └── bench/cpp/it/d4np/memorypool/
├── docs/
│   ├── README.md                   # documentation index
│   ├── adr/                        # Architecture Decision Records
│   ├── patterns/                   # design-patterns catalogue
│   ├── specs/                      # functional/technical specifications
│   └── workflow/                   # git & documentation conventions
└── (build/, CMakeLists.txt, etc.)  # created in Milestone 1
```

## 5. Source Tree & Cross-Language Layout

All code lives under a **Maven-style cross-language source tree** so that PBR projects in any language share the same shape:

```
src/main/<lang>/it/d4np/<project>/    # production sources
src/test/<lang>/it/d4np/<project>/    # test sources
src/bench/<lang>/it/d4np/<project>/   # benchmarks (where applicable)
src/main/<lang>/resources/            # non-source assets, when needed
```

For this repository:

- `<lang>` = `cpp`
- `<project>` = `memorypool`
- C++ namespace: **`it::d4np::memorypool`** — mirrors the path 1:1
- Public include path: `src/main/cpp` → consumers write `#include <it/d4np/memorypool/memory_pool.h>`

Subdivision inside `memorypool/` is by **component** (`pool/`, `freelist/`, `threading/`, …) — not by file type. Public and private headers co-locate with their implementation; private headers use the `detail/` subfolder or a `_internal` suffix.

**This layout is normative.** Do not place code at the repository root, under a flat `src/`, or in any other shape without first superseding [ADR-0002](docs/adr/0002-adopt-cross-language-source-layout.md). Sibling PBR projects in Java, Python, etc. must follow the same template with the appropriate `<lang>` segment.

## 6. Git Workflow

### 6.1 Boundary between agent and human

| Action                                  | Who does it |
|-----------------------------------------|-------------|
| Create branches                         | Agent       |
| Stage, commit, push                     | Agent       |
| Draft pull request (title + body)       | Agent       |
| **Open / publish the pull request**     | **Human**   |
| Code review                             | Human       |
| **Merge / squash / rebase to `master`** | **Human**   |

Agents **never merge**, **never force-push `master`**, and **never push directly to `master`**. When unsure, push the branch and ask.

### 6.2 Branch naming

Format: `<type>/<short-kebab-description>`

`type` is one of: `feat`, `fix`, `refactor`, `perf`, `docs`, `test`, `build`, `chore`, `ci`.

Examples:

- `feat/free-list-alloc`
- `fix/destroy-double-free`
- `docs/adr-thread-safety`
- `perf/cacheline-aligned-blocks`
- `chore/cmake-presets`

Keep the description under ~40 characters; favor the *what*, not the ticket number.

### 6.3 Commit messages — Conventional Commits

```
<type>(<scope>): <imperative subject ≤72 chars>

<body — explain WHY, not WHAT; wrap at ~72 cols>

<optional footers:
BREAKING CHANGE: <description>
Refs: #<issue> | ADR-0003>
```

Rules:

- One logical change per commit.
- Subject in **imperative mood** ("add free list", not "added free list").
- The body is for motivation, trade-offs, and links to ADRs — the diff already shows the *what*.
- Rebase to clean up history before opening the PR if intermediate commits are noisy.

Common scopes for this repo: `pool`, `freelist`, `threading`, `api`, `build`, `tests`, `bench`, `docs`, `adr`, `patterns`, `ci`.

### 6.4 Pull Requests

The agent prepares the PR locally (branch pushed, draft body written) and reports the suggested `gh pr create` command — or invokes it if the user has explicitly authorized PR creation in the current session.

PR title = lead commit subject (Conventional-Commits format).

PR body template:

```markdown
## Summary
One or two sentences: what changes and why it matters.

## Motivation
Link to the spec section, ADR, roadmap item, or issue that prompted this work.

## Changes
- bulleted list of meaningful changes (not a file list)

## Design Patterns
- list every pattern adopted/refined in this PR, with a one-line rationale and a link to the ADR.
- if none, write "None — straightforward implementation."

## Verification
- [ ] Builds cleanly on the full toolchain matrix
- [ ] Unit tests pass
- [ ] `clang-tidy` clean on the diff
- [ ] ASan + UBSan clean (TSan when threading is involved)
- [ ] Valgrind: `ERROR SUMMARY: 0 errors from 0 contexts`
- [ ] Benchmark numbers attached (when perf-relevant)

## Documentation Impact
- [ ] README.md updated (if user-facing surface changed)
- [ ] ROADMAP.md checkbox flipped
- [ ] ADR added/updated (if a non-trivial design decision was made)
- [ ] docs/patterns/README.md updated (if a pattern was introduced, refined, or rejected)
- [ ] Spec updated (if behavior diverges from `docs/specs/`)
- [ ] CHANGELOG.md updated (once that file exists)
```

**Merge commit semantics.** The repository is configured (`squash_merge_commit_message=PR_BODY`, `merge_commit_message=PR_BODY`, both titles set to `PR_TITLE`) so the **PR title and PR body become the merge commit's subject and extended description verbatim**. Write the PR body as you want it to read in `git log` forever — Summary, Motivation, Changes, Design Patterns, Verification, Documentation Impact. The maintainer scrubs HTML comments and placeholders at merge time. Squash-and-merge is the preferred strategy.

See [`docs/workflow/git-workflow.md`](docs/workflow/git-workflow.md) for the full convention details and examples, including §4.5 on merge commit messages.

## 7. Documentation Maintenance

Documentation is part of the deliverable, not an afterthought. Every PR ships its own doc updates.

### 7.1 README.md

`README.md` is the project's front door. Keep it in sync with reality:

- High-level *what* and *why*
- Build / test / benchmark instructions, once they exist
- Pointers to `AGENTS.md`, `ROADMAP.md`, `docs/`

If a PR changes the public API, the build, or the user-facing workflow, it **must** update `README.md` in the same PR.

### 7.2 ADRs — Architecture Decision Records

Format: lightweight Michael Nygard ADRs, one Markdown file per decision in `docs/adr/`, numbered sequentially (`0001-…`, `0002-…`). Template at [`docs/adr/template.md`](docs/adr/template.md).

Open an ADR when:

- A choice affects the public API.
- A choice affects ABI, alignment, or thread-safety guarantees.
- Two reasonable options exist and the rationale is non-obvious from the code.
- **A design pattern is adopted** (see §8).
- A previous ADR needs to be superseded.

Do **not** open an ADR for routine implementation details, formatting, or trivially reversible choices.

ADR status transitions: `Proposed` → `Accepted` → (`Superseded by ADR-XXXX` | `Deprecated`).

### 7.3 ROADMAP.md

`ROADMAP.md` holds the project's plan as a numbered, checkbox-driven list. When an item is completed in a PR, **flip the checkbox in the same PR** (`- [ ]` → `- [x]`). New work that emerges goes at the bottom of the relevant section with a fresh number.

### 7.4 Specs

`docs/specs/` holds frozen specifications. If implementation diverges from the spec, either:

1. Update the spec in the same PR and note the change in the PR body, **or**
2. Add an ADR explaining the deviation and link it from the spec.

Never let code and spec silently drift.

### 7.5 Patterns catalogue

`docs/patterns/README.md` is the living catalogue of every design pattern adopted, in flight, or explicitly rejected. See §8.

## 8. Design Patterns Policy

This is a **reference implementation**, and demonstrating fluency with classical design patterns is part of its value. Therefore:

1. **Exercise patterns broadly.** When a problem admits a recognized pattern as a natural fit, use it — Factory, Builder, Strategy, Template Method, Adapter, Decorator, Composite, Iterator, Observer, Pimpl, RAII, Null Object, Facade, and Object Pool are all in scope.
2. **Justify every adoption.** Each pattern enters the codebase through an ADR that records: the problem, the alternatives considered, and the specific reasons this pattern was chosen. The ADR is linked from the patterns catalogue.
3. **Do not force-fit.** A pattern bolted onto a problem that does not need it demonstrates the *opposite* of competence. When in doubt, write the comparison in the ADR and pick the simpler option — including "no pattern needed", which is itself a documented decision.
4. **Record rejections.** Patterns considered and ruled out are listed in `docs/patterns/README.md` under *Rejected*, with the reason. This prevents the same debate from recurring.
5. **One pattern per ADR** is preferred. Multi-pattern PRs may bundle into a single ADR only when the patterns are co-introduced and interdependent.
6. **Use the canonical taxonomy.** Pattern names — in ADRs, in the catalogue, in commit messages, in code comments — must match the spelling and categorisation in [`docs/patterns/design-patterns.md`](docs/patterns/design-patterns.md). That file is the authoritative enterprise pattern list across the eight categories (Creational, Structural, Behavioral, EIP, Architectural, Concurrency, Cloud/Distributed, Data & Persistence). When evaluating candidates for a problem, scan the relevant category there first.

Formal policy: see [ADR-0003](docs/adr/0003-design-patterns-policy.md). Project-scoped candidates and out-of-scope categories: [`docs/patterns/README.md`](docs/patterns/README.md).

## 9. Coding Conventions

The toolchain and platform contract is formalised in [ADR-0005](docs/adr/0005-toolchain-matrix-and-supported-platforms.md); the formatter and static-analysis baseline are in [ADR-0006](docs/adr/0006-code-style-and-static-analysis-baseline.md). The repo-local enforcement configs are [`.clang-format`](.clang-format) and [`.clang-tidy`](.clang-tidy) — both at the repo root.

- **Language standard:** C++17, no compiler extensions. The C interop layer is verified against both C89 (ANSI C, spec §3.3) and C99 — see ADR-0005 §3.
- **Tier-1 platforms (CI-gated):** Linux x86_64 (GCC ≥ 11, Clang ≥ 14), Windows x86_64 (MSVC ≥ 19.30), macOS arm64 (Apple Clang ≥ 14). Full matrix in ADR-0005 §1.
- **Namespace:** `it::d4np::memorypool` (with nested sub-namespaces for components, e.g., `it::d4np::memorypool::detail`).
- **Headers:** `<it/d4np/memorypool/memory_pool.h>` for the public C API, `<it/d4np/memorypool/memory_pool.hpp>` for the C++ wrapper.
- **Naming:** `snake_case` for functions and variables, `CamelCase` for C++ types, `UPPER_CASE` for macros and `constexpr` constants, trailing `_` on private/protected members. Enforced by `readability-identifier-naming` in [`.clang-tidy`](.clang-tidy).
- **Formatting:** LLVM-derived style, 4-space indent, 120-col soft limit, pointer-aligned-left. See [`.clang-format`](.clang-format) and ADR-0006 §1 for the full deviation table.
- **Static analysis:** `bugprone-*`, `cert-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `portability-*`, `readability-*` baseline with the deviations recorded in ADR-0006 §2. `WarningsAsErrors` enforced on the diff at CI (ROADMAP §1.8); locally advisory.
- **Documentation:** All public symbols documented with Doxygen-compatible comments (`///` or `/** */`). Private code commented only where the *why* is non-obvious.
- **Errors:** No exceptions across the C ABI boundary. The C++ wrapper may throw `std::bad_alloc` (configurable). All error paths covered by tests.

## 10. Enterprise Quality Bar

Every PR must clear, at minimum:

| Gate                          | Requirement                                                                  |
|-------------------------------|------------------------------------------------------------------------------|
| Compiler matrix               | MSVC, GCC, Clang — Debug & Release builds                                    |
| Warnings                      | `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang) or `/W4 /WX` (MSVC) — zero    |
| `clang-tidy`                  | Clean on the diff; no broad disables                                         |
| Unit tests                    | Cover the new/changed behavior; pass on every compiler                       |
| Sanitizers                    | ASan + UBSan green (TSan when threading is touched)                          |
| Valgrind                      | `ERROR SUMMARY: 0 errors from 0 contexts` on the demonstrative test         |
| Coverage                      | New code reasonably covered (target ≥80% line, finalized in an ADR)          |
| Public API docs               | Doxygen-compatible, builds without warnings                                  |
| Performance claims            | Backed by a reproducible benchmark under `src/bench/`                        |
| Versioning                    | SemVer; `CHANGELOG.md` updated for user-visible changes once it exists       |

Shortcuts ("just disable the warning", "tests next PR", "docs follow-up") are not allowed. If something is genuinely out of scope, file it as a new `ROADMAP.md` item in the same PR.

## 11. Versioning & Release

The project follows **Semantic Versioning 2.0.0**. Tags are annotated and have the form `v<MAJOR>.<MINOR>.<PATCH>` with optional `-alpha.N` / `-beta.N` / `-rc.N` suffixes.

**Pre-1.0 (`0.MINOR.PATCH`)** — `MINOR` increments with each completed roadmap milestone (Milestone 1 → `v0.1.0`, …, Milestone 6 → `v0.6.0`); `PATCH` covers hotfixes between milestones. Breaking changes are allowed in a `MINOR` bump but must be recorded in `CHANGELOG.md`.

**Post-1.0 (`MAJOR.MINOR.PATCH`)** — standard SemVer; `MAJOR` for source-/ABI-incompatible changes, `MINOR` for additions, `PATCH` for fixes.

**`CHANGELOG.md`** — Keep a Changelog 1.1.0 format. Every PR that introduces user-visible change adds a line to the `Unreleased` section in the same PR; release PRs roll `Unreleased` into the new version block.

**Release artifacts** — on each `v*` tag push, the `release` CI workflow re-runs the full test matrix, builds per-platform binaries (Linux x86_64, Windows x86_64, macOS arm64), produces `SHA256SUMS`, and creates a **draft** GitHub Release. The maintainer reviews and clicks *Publish*.

**Package distribution** — Phase 1 (Milestone 7.4): CMake `find_package` config. Phase 2 (post-1.0 stretch, Milestones 7.8/7.9): vcpkg port and Conan recipe.

**Agent-vs-human boundary for releases** mirrors §6.1:

| Action                                                | Who      |
|-------------------------------------------------------|----------|
| Bump version constant in source                       | Agent    |
| Update `CHANGELOG.md` (Unreleased → version)          | Agent    |
| Draft GitHub Release notes (`docs/releases/v<X.Y.Z>.md`) | Agent  |
| Open the release PR                                   | **Human**|
| Merge the release PR                                  | **Human**|
| Create the annotated git tag                          | **Human**|
| Push the tag                                          | **Human**|
| Publish the GitHub Release                            | **Human**|
| Build & attach release artifacts                      | CI       |

Agents **never** push tags, **never** publish releases. Full policy: [ADR-0004](docs/adr/0004-versioning-and-release-policy.md). Operational guide: [`docs/workflow/release.md`](docs/workflow/release.md).

## 12. Tool-Specific Notes

### 12.1 Claude Code

`CLAUDE.md` defers here. Claude Code-specific config (subagents, hooks) lives under `.claude/`. Use the `TaskCreate` tool to track multi-step work in-session.

### 12.2 Gemini Antigravity

`GEMINI.md` defers here. Tool-specific configuration lives under `.gemini/` if added.

### 12.3 ChatGPT Codex

Reads `AGENTS.md` natively — no adapter file required.

---

**When in doubt: read the spec, write an ADR, document the pattern, ask the user before merging anything.**
