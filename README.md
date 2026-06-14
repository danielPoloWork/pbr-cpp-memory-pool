# High-Performance Memory Pool Manager (C++)

[![ci](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml)
[![docs](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml/badge.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Standard: C++17 / ANSI C](https://img.shields.io/badge/Standard-C%2B%2B17%20%2F%20ANSI%20C-blue.svg)](docs/specs/01_spec_cpp_memory_pool.md)
[![Status: v0.6.0 observability](https://img.shields.io/badge/Status-v0.6.0%20observability-green.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v0.6.0)

> Part of the **Purpose-Built References (PBR)** series — small, didactic, production-quality C/C++ reference implementations of high-performance building blocks.

Many high-performance systems — graphics engines, financial trading servers, databases — suffer from memory fragmentation and the overhead of frequent `malloc`/`free` (or `new`/`delete`) calls. This component provides a **custom Memory Pool** that pre-allocates a contiguous block of memory and delivers **constant-time, fixed-size allocation with zero external fragmentation**.

## At a glance

- **Allocation:** O(1), fixed block size, contiguous backing storage.
- **Free-list strategy:** implicit — free blocks store the next-free pointer in their own first `sizeof(void*)` bytes, so live blocks carry zero metadata overhead.
- **Metadata overhead:** 0 bytes per block (the free-list link reuses unused block storage) + a fixed ~40 bytes per pool — independent of `block_count`. CI-gated at ≤ 128 bytes per [ADR-0015](docs/adr/0015-metadata-overhead-budget-and-introspection.md).
- **Standards:** ANSI C public surface, C++17 internals and wrapper. No external dependencies.
- **Thread safety:** opt-in, configurable at compile time (Milestone 4).
- **Dynamic growth:** optional contiguous overflow chunks (Milestone 5).
- **Observability:** opt-in `InstrumentedPool` Decorator (stats, occupancy) + `PoolObserver` for lifecycle events — zero cost to undecorated pools (Milestone 6).
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

The free-list layout, the `block_size ≥ sizeof(void*)` constraint, and the alignment guarantees are locked down in [ADR-0009](docs/adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md); the surrounding decisions are recorded in [ADR-0002](docs/adr/0002-adopt-cross-language-source-layout.md) and [ADR-0003](docs/adr/0003-design-patterns-policy.md).

## Performance

Headline numbers from the v0.2.0 microbenchmark (M2.9 / spec §6.3) on the maintainer's Windows × MSVC × x64 workstation, 64-byte blocks, 1,000,000 iterations × 10 repeats with the first dropped as warm-up. Full report — host disclosure, raw output, per-region tables, observations — in [`docs/bench/v0.2.0-windows-msvc-x64.md`](docs/bench/v0.2.0-windows-msvc-x64.md). Methodology contract: [ADR-0014](docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md).

| Scenario     | median `malloc` (ns/op) | median `pool` (ns/op) | `malloc` / `pool` |
|--------------|------------------------:|----------------------:|------------------:|
| bulk-alloc   | 75.5                    | 6.9                   | **11.02 ×**       |
| bulk-free    | 44.5                    | 8.3                   | **5.35 ×**        |
| interleaved  | 49.9                    | 11.2                  | **4.45 ×**        |

**Threading (M4.5).** The same binary's `--scenario concurrent` runs `T` threads against a shared pool, built per thread-safety policy. The single-thread fast path is preserved (`NONE` ≈ 9 ns/op interleaved, unchanged); under 4-thread contention `LOCKFREE` (41.8 ns/op) beats `MUTEX` (69.5 ns/op), though a single-shared-head pool does not out-scale `malloc`'s per-thread arenas — full analysis in [`docs/bench/v0.4.0-windows-msvc-x64-threading.md`](docs/bench/v0.4.0-windows-msvc-x64-threading.md).

The bench binary is built off by default; the `bench` preset (Release + benchmarks ON + tests OFF) opts in:

```bash
cmake --preset bench
cmake --build --preset bench
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench
```

Reports for other host × compiler combinations (Linux / GCC, Linux / Clang, macOS / Apple Clang) are welcome — see [`docs/bench/README.md`](docs/bench/README.md) for the contribution recipe.

## Status

`v0.6.0` — observability & decorators. Optional statistics, on-demand logging, and lifecycle-event notification, all **opt-in by type** — a program that uses `Pool` directly is byte-identical to v0.5.0 and pays nothing. The header-only `InstrumentedPool` **Decorator** composes a `Pool` and counts allocation activity (allocations / deallocations / failures / live + the `peak_live_` high-water mark) via relaxed atomics, exposed as a `PoolStats` snapshot plus `write_summary`; a runtime **Observer** (`PoolObserver` via `add_observer`) delivers `exhausted` / `grew` / `destroyed` events from the same interception points. The one library-side addition is a `grow_count_` atomic on `struct memory_pool` (bumped only on the growth slow path) behind the O(1) `memory_pool_growths` accessor, within the ADR-0015 192-byte budget. The new `zero_overhead` test verifies the zero-cost-when-disabled contract structurally. Two new ADRs (0025–0026) bring the total to 26; **Decorator** and **Observer** flip to Implemented. Release notes for `v0.6.0` live in [`docs/releases/v0.6.0.md`](docs/releases/v0.6.0.md).

| Milestone | Title                              | Status      |
|-----------|------------------------------------|-------------|
| 0         | Agent & Workflow Scaffolding       | ✅ complete |
| 1         | Build System & Project Skeleton    | ✅ complete |
| 2         | Core Memory Pool (single-threaded) | ✅ complete |
| 3         | C++ Wrapper & Type Safety          | ✅ complete |
| 4         | Thread-Safe Variant                | ✅ complete |
| 5         | Dynamic Growth Mode                | ✅ complete |
| 6         | Observability & Decorators         | ✅ complete |
| 7         | Release & Polish                   | ⏳ next     |

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

Full quality contract: [`AGENTS.md`](AGENTS.md) §10. The C++ build matrix, sanitizers, `clang-format`, `clang-tidy` diff gate, ANSI C / C99 verification, and zero-external-dependency audit run on every PR via [`ci.yml`](.github/workflows/ci.yml); a docs-only workflow ([`docs.yml`](.github/workflows/docs.yml)) covers markdownlint, internal link checks, and ADR numbering sanity. Both badges above gate `master`.

## Build and test

The canonical three-step workflow, once the toolchain is installed, is the same on every supported platform — only the shell-level chaining differs.

```bash
# Linux, macOS, MinGW/MSYS2, WSL — any POSIX shell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

```powershell
# Windows — Developer PowerShell for VS 2022 (PowerShell 5.1 has no &&)
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Both invocations are exercised end-to-end on every push to `master` by the [CI matrix](.github/workflows/ci.yml): Linux × {GCC, Clang}, Windows × MSVC, macOS × Apple Clang — across `debug`, `release`, `asan`, and `ubsan` presets (sanitizer presets are POSIX-only per [ADR-0005](docs/adr/0005-toolchain-matrix-and-supported-platforms.md) §3). A green `ci` badge above is the canonical "the quickstart works on Windows and Linux" signal.

For first-time setup on a fresh clone — installing CMake, Ninja, and the supported compilers per platform, plus troubleshooting and the full quality-bar workflow — see the **[Local Build Guide](docs/development/local-build.md)**.

## Repository layout

| Path                                          | What lives there                                                                          |
|-----------------------------------------------|-------------------------------------------------------------------------------------------|
| [`AGENTS.md`](AGENTS.md)                      | Cross-tool contract for AI coding agents (Codex, Claude, Gemini).                         |
| [`CLAUDE.md`](CLAUDE.md)                      | Claude Code adapter — defers to `AGENTS.md`.                                              |
| [`GEMINI.md`](GEMINI.md)                      | Gemini Antigravity adapter — defers to `AGENTS.md`.                                       |
| [`ROADMAP.md`](ROADMAP.md)                    | Numbered checkbox plan + Spec Coverage Map.                                               |
| [`CHANGELOG.md`](CHANGELOG.md)                | Keep a Changelog 1.1.0 history; user-visible changes per release (see [ADR-0004](docs/adr/0004-versioning-and-release-policy.md)). |
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
