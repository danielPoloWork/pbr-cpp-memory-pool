# Documentation

This directory holds the durable, versioned documentation for `pbr-cpp-memory-pool`. Conversational context, scratch notes, and chat transcripts do **not** live here — everything in `docs/` is a first-class repository artifact and follows the same review process as source code.

## Layout

| Path                  | Purpose                                                                                  |
|-----------------------|------------------------------------------------------------------------------------------|
| `docs/specs/`         | Functional and technical specifications. Frozen contracts — diverging requires an ADR.   |
| `docs/adr/`           | Architecture Decision Records — one numbered Markdown file per decision.                 |
| `docs/patterns/`      | Living catalogue of design patterns adopted, rejected, or under consideration.           |
| `docs/workflow/`      | Repository workflow conventions (git, documentation maintenance).                        |

## Reading order for newcomers

1. [`/README.md`](../README.md) — what this project is.
2. [`/AGENTS.md`](../AGENTS.md) — how AI agents are expected to work in this repo.
3. [`docs/specs/01_spec_cpp_memory_pool.md`](specs/01_spec_cpp_memory_pool.md) — what we are building.
4. [`docs/adr/`](adr/) — why we built it that way.
5. [`docs/patterns/`](patterns/) — which design patterns we exercise and why.
6. [`/ROADMAP.md`](../ROADMAP.md) — what is done and what is next.

## Conventions

- **English only.** All documentation, identifiers, and diagrams are in English.
- **Same-PR updates.** When code changes its public surface or a non-trivial design choice, the docs change in the *same* pull request — never as a follow-up.
- **No silent drift.** If the implementation diverges from the spec, either update the spec or open an ADR that explicitly supersedes the relevant section.
