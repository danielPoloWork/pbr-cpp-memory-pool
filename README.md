# Purpose-built reference High-Performance Memory Pool Manager (C++)

[![docs](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml/badge.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Standard: C++17 / ANSI C](https://img.shields.io/badge/Standard-C%2B%2B17%20%2F%20ANSI%20C-blue.svg)](docs/specs/01_spec_cpp_memory_pool.md)
[![Status: v0 scaffolding](https://img.shields.io/badge/Status-v0%20scaffolding-orange.svg)](ROADMAP.md)

> Part of the **Purpose-Built References (PBR)** series — small, didactic, production-quality C/C++ reference implementations of high-performance building blocks.

Many high-performance systems — graphics engines, financial trading servers, databases — suffer from memory fragmentation and the overhead of frequent `malloc`/`free` (or `new`/`delete`) calls. This component provides a **custom Memory Pool** that pre-allocates a contiguous block of memory and delivers **constant-time, fixed-size allocation with zero external fragmentation**.

## At a glance

- **Allocation:** O(1), fixed block size, contiguous backing storage.
- **Free-list strategy:** implicit — free blocks store the next-free pointer in their own first `sizeof(void*)` bytes, so live blocks carry zero metadata overhead.
- **Standards:** ANSI C public surface, C++17 internals and wrapper. No external dependencies.
- **Thread safety:** opt-in, configurable at compile time (Milestone 4).
- **Dynamic growth:** optional contiguous overflow chunks (Milestone 5).
- **Quality gates:** `clang-tidy` clean, ASan + UBSan + (when threading lands) TSan green, Valgrind clean, Doxygen-documented public surface.
- **Benchmark target:** measured against `malloc`/`free` over 1,000,000 iterations.

## Public C API

The complete public surface is four functions. The full contract is in the [spec](docs/specs/01_spec_cpp_memory_pool.md):

```c
typedef struct memory_pool memory_pool_t;

memory_pool_t* memory_pool_create(size_t block_size, size_t block_count);
void*          memory_pool_alloc(memory_pool_t* pool);
void           memory_pool_free(memory_pool_t* pool, void* block);
void           memory_pool_destroy(memory_pool_t* pool);
```

A C++17 RAII wrapper (`it::d4np::memorypool::Pool`) and a typed template (`TypedPool<T>`) layer on top of this surface — see Milestones 2.5 and 3.2 in [`ROADMAP.md`](ROADMAP.md).

## Architecture

The pool manages free memory using a **free list embedded inside the free blocks themselves**: when a block is free, its first `sizeof(void*)` bytes hold the address of the next free block. Live blocks carry no metadata at all.

```text
+-------------------------------------------------------------------+
|                           Memory Pool                             |
+-------------------------------------------------------------------+
| [Block 1 (free)]   -> next-free ptr to Block 2                    |
| [Block 2 (in use)] -> user data                                   |
| [Block 3 (free)]   -> next-free ptr to Block 4                    |
| [Block 4 (free)]   -> NULL (end of free list)                     |
+-------------------------------------------------------------------+
```

The free-list layout, the `block_size ≥ sizeof(void*)` constraint, and the alignment guarantees will be locked down in a dedicated ADR landing with Milestone 2.1; meanwhile [ADR-0002](docs/adr/0002-adopt-cross-language-source-layout.md) and [ADR-0003](docs/adr/0003-design-patterns-policy.md) already cover the surrounding decisions (source layout and design-patterns policy).

## Status

`v0` — scaffolding phase. Agent configuration, documentation, source-tree shape, CI for docs, and the design-patterns policy are all in place. The C/C++ implementation has **not started yet**.

| Milestone | Title                              | Status      |
|-----------|------------------------------------|-------------|
| 0         | Agent & Workflow Scaffolding       | ✅ complete |
| 1         | Build System & Project Skeleton    | ⏳ next     |
| 2         | Core Memory Pool (single-threaded) | ⏳ planned  |
| 3         | C++ Wrapper & Type Safety          | ⏳ planned  |
| 4         | Thread-Safe Variant                | ⏳ planned  |
| 5         | Dynamic Growth Mode                | ⏳ planned  |
| 6         | Observability & Decorators         | ⏳ planned  |
| 7         | Release & Polish                   | ⏳ planned  |

See [`ROADMAP.md`](ROADMAP.md) for the per-task breakdown and the Spec Coverage Map at the bottom (traceability from spec sections to roadmap items).

## Verification & Quality Gates

Every PR must clear, at minimum:

| Gate                          | Requirement                                                                  |
|-------------------------------|------------------------------------------------------------------------------|
| Compiler matrix               | MSVC, GCC, Clang — Debug & Release builds                                    |
| Warnings                      | `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang) or `/W4 /WX` (MSVC) — zero    |
| `clang-tidy`                  | Clean on the diff; no broad disables                                         |
| Unit tests                    | Cover new/changed behaviour; pass on every compiler                          |
| Sanitizers                    | ASan + UBSan green; TSan once threading is touched                           |
| Valgrind                      | `ERROR SUMMARY: 0 errors from 0 contexts` on the demonstrative test          |
| Public API docs               | Doxygen-compatible, builds without warnings                                  |
| Performance claims            | Backed by reproducible benchmark under `src/bench/`                          |

Full quality contract: [`AGENTS.md`](AGENTS.md) §10. Until the C++ build lands in Milestone 1.8, a **docs-only CI** runs on every PR (markdownlint + internal link check + ADR numbering sanity) — its status appears in the badge above.

## Build and test

The canonical commands, once the toolchain is installed:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

For first-time setup on a fresh clone — installing CMake, Ninja, and the supported compilers per platform, plus troubleshooting and the full quality-bar workflow — see the **[Local Build Guide](docs/development/local-build.md)**.

## Repository layout

| Path                                          | What lives there                                                                          |
|-----------------------------------------------|-------------------------------------------------------------------------------------------|
| [`AGENTS.md`](AGENTS.md)                      | Cross-tool contract for AI coding agents (Codex, Claude, Gemini).                         |
| [`CLAUDE.md`](CLAUDE.md)                      | Claude Code adapter — defers to `AGENTS.md`.                                              |
| [`GEMINI.md`](GEMINI.md)                      | Gemini Antigravity adapter — defers to `AGENTS.md`.                                       |
| [`ROADMAP.md`](ROADMAP.md)                    | Numbered checkbox plan + Spec Coverage Map.                                               |
| [`src/`](src/)                                | All source code, Maven-style layout — `src/{main,test,bench}/cpp/it/d4np/memorypool/`.    |
| [`docs/specs/`](docs/specs/)                  | Functional and technical specifications.                                                  |
| [`docs/adr/`](docs/adr/)                      | Architecture Decision Records.                                                            |
| [`docs/patterns/`](docs/patterns/)            | Design-patterns catalogue + canonical enterprise taxonomy.                                |
| [`docs/workflow/`](docs/workflow/)            | Git and documentation conventions.                                                        |
| [`docs/development/`](docs/development/)      | Procedural how-to guides for local development (build, debug, profile).                   |

## For human contributors

Start by reading, in this order:

1. [`docs/specs/01_spec_cpp_memory_pool.md`](docs/specs/01_spec_cpp_memory_pool.md) — what we are building.
2. [`docs/development/local-build.md`](docs/development/local-build.md) — how to build and test it on your machine.
3. [`docs/adr/`](docs/adr/) — why the project is structured this way.
4. [`docs/patterns/README.md`](docs/patterns/README.md) — which design patterns we exercise and why.
5. [`docs/workflow/git-workflow.md`](docs/workflow/git-workflow.md) — branch, commit, and PR conventions.
6. [`ROADMAP.md`](ROADMAP.md) — what is done and what is next.

## For AI coding agents

This repository is configured to work with **Claude Code**, **Gemini Antigravity**, and **ChatGPT Codex**. The agent contract lives in [`AGENTS.md`](AGENTS.md) — read it before doing anything. Short version:

- Every artifact (code, docs, commits, branches, PRs) is in **English**.
- You commit, push, and **draft** pull requests on feature branches. The maintainer **opens and merges** PRs manually.
- Non-trivial design decisions are recorded as ADRs in the same PR.
- Every adopted design pattern is justified in an ADR and listed in [`docs/patterns/README.md`](docs/patterns/README.md).
- Enterprise quality bar: warnings-as-errors, `clang-tidy` clean, ASan/UBSan/TSan green, Valgrind clean, Doxygen documented.
- `README.md` and `ROADMAP.md` are kept current in the same PR as the work they describe.

## License

[MIT](LICENSE) © 2026 Daniel Polo.
