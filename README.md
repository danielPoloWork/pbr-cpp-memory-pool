# pbr-cpp-memory-pool

> Part of the **Purpose-Built References (PBR)** series — small, didactic, production-quality C/C++ reference implementations of high-performance building blocks.

Many high-performance systems — graphics engines, financial trading servers, databases — suffer from memory fragmentation and the overhead of frequent `malloc`/`free` (or `new`/`delete`) calls. This component provides a **custom Memory Pool** that pre-allocates a contiguous block of memory and delivers **constant-time, fixed-size allocation with zero external fragmentation**.

## Status

`v0` — scaffolding phase. Agent configuration, documentation, and workflow are in place; the C/C++ implementation has **not started yet**. See [`ROADMAP.md`](ROADMAP.md) for the current milestone.

## At a glance

- **Language:** C++17 with an ANSI C-compatible public interface.
- **Allocator:** fixed block size, O(1) alloc and free, contiguous backing storage.
- **Thread safety:** opt-in, configurable at compile time (planned in Milestone 4).
- **Dependencies:** none beyond the standard library.
- **Verification target:** Valgrind clean (`ERROR SUMMARY: 0 errors from 0 contexts`) and benchmarked against `malloc`/`free` over 1,000,000 iterations.

## Repository layout

| Path                                          | What lives there                                                                          |
|-----------------------------------------------|-------------------------------------------------------------------------------------------|
| [`AGENTS.md`](AGENTS.md)                      | Cross-tool contract for AI coding agents (Codex, Claude, Gemini).                         |
| [`CLAUDE.md`](CLAUDE.md)                      | Claude Code adapter — defers to `AGENTS.md`.                                              |
| [`GEMINI.md`](GEMINI.md)                      | Gemini Antigravity adapter — defers to `AGENTS.md`.                                       |
| [`ROADMAP.md`](ROADMAP.md)                    | Numbered checkbox plan, milestone-by-milestone.                                           |
| [`src/`](src/)                                | All source code, Maven-style layout — `src/{main,test,bench}/cpp/it/d4np/memorypool/`.    |
| [`docs/specs/`](docs/specs/)                  | Functional and technical specifications.                                                  |
| [`docs/adr/`](docs/adr/)                      | Architecture Decision Records.                                                            |
| [`docs/patterns/`](docs/patterns/)            | Design-patterns catalogue.                                                                |
| [`docs/workflow/`](docs/workflow/)            | Git and documentation conventions.                                                        |

## For human contributors

Start by reading, in this order:

1. [`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md) — what we are building.
2. [`docs/adr/`](docs/adr/) — why the project is structured this way.
3. [`docs/patterns/README.md`](docs/patterns/README.md) — which design patterns we exercise and why.
4. [`docs/workflow/git-workflow.md`](docs/workflow/git-workflow.md) — branch, commit, and PR conventions.
5. [`ROADMAP.md`](ROADMAP.md) — what is done and what is next.

## For AI coding agents

This repository is configured to work with **Claude Code**, **Gemini Antigravity**, and **ChatGPT Codex**. The agent contract lives in [`AGENTS.md`](AGENTS.md) — read it before doing anything. Short version:

- You are a **senior project architect with 20+ years of enterprise C++ experience**.
- Every artifact (code, docs, commits, branches, PRs) is in **English**.
- All code lives under `src/main/cpp/it/d4np/memorypool/` (Maven-style layout). C++ namespace: `it::d4np::memorypool`.
- You commit, push, and **draft** pull requests on feature branches. The maintainer **opens and merges** PRs manually.
- Non-trivial design decisions are recorded as ADRs in the same PR.
- Every adopted design pattern is justified in an ADR and listed in [`docs/patterns/README.md`](docs/patterns/README.md).
- Enterprise quality bar: warnings-as-errors, `clang-tidy` clean, ASan/UBSan/TSan green, Valgrind clean, Doxygen documented.
- `README.md` and `ROADMAP.md` are kept current in the same PR as the work they describe.

## Build and test

The build system arrives in Milestone 1 ([`ROADMAP.md`](ROADMAP.md) §1). Once available, the canonical commands will be:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## License

See [`LICENSE`](LICENSE).
