# High-Performance Memory Pool Manager (C++)

[![ci](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml)
[![docs](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml/badge.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml)
[![API reference](https://img.shields.io/badge/API%20reference-Doxygen-1f6feb.svg)](https://danielpolowork.github.io/pbr-cpp-memory-pool/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Standard: C++17 / ANSI C](https://img.shields.io/badge/Standard-C%2B%2B17%20%2F%20ANSI%20C-blue.svg)](docs/specs/01_spec_cpp_memory_pool.md)
[![Status: v1.1.1 stable](https://img.shields.io/badge/Status-v1.1.1%20stable-brightgreen.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v1.1.1)

> Part of the **Purpose-Built References (PBR)** series — small, didactic, production-quality C/C++ reference implementations of high-performance building blocks.

**Read this in:** [简体中文](docs/i18n/zh-Hans/README.md) · [日本語](docs/i18n/ja/README.md) — English is the normative source ([ADR-0032](docs/adr/0032-documentation-i18n-architecture.md)).

Many high-performance systems — graphics engines, financial trading servers, databases — suffer from memory fragmentation and the overhead of frequent `malloc`/`free` (or `new`/`delete`) calls. This component provides a **custom Memory Pool** that pre-allocates a contiguous block of memory and delivers **constant-time, fixed-size allocation with zero external fragmentation**. It is a single, focused C/C++ library — a header-backed static library with a four-function C ABI and an idiomatic C++17 wrapper — built to enterprise quality (warnings-as-errors, sanitizers, Valgrind, Doxygen) and documented decision-by-decision in its [ADRs](docs/adr/).

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

The complete, cross-linked **API reference** — every public symbol's parameter / return / throws contract — is generated from the in-header Doxygen comments and published to [GitHub Pages](https://danielpolowork.github.io/pbr-cpp-memory-pool/) on every push to `master` ([ADR-0027](docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)). The same build runs as a warn-as-error gate on every PR.

## Usage

The public include root is `src/main/cpp`, so headers are included as `<it/d4np/memorypool/…>`; link against the static library target `pbr_memory_pool` (see [Build and test](#build-and-test)). Every snippet below is compiled and run as-is by the maintainer before each release. The C++ surface lives in namespace `it::d4np::memorypool` (elided below for brevity).

### C API — the four-function core

```c
#include <it/d4np/memorypool/memory_pool.h>

memory_pool_t* pool = memory_pool_create(64, 1024);  /* 1024 blocks of 64 bytes */
if (pool != NULL) {
    void* block = memory_pool_alloc(pool);           /* O(1); NULL when exhausted */
    if (block != NULL) {
        memory_pool_free(pool, block);               /* O(1); back to the free list */
    }
    memory_pool_destroy(pool);                       /* releases all backing storage */
}
```

### C++ RAII wrapper — `Pool`

`Pool` owns the handle (constructed → created, destroyed → destroyed) and is move-only. The exception policy is dual-verb ([ADR-0016](docs/adr/0016-exception-policy-at-the-c-cpp-boundary.md)): `allocate()` throws `std::bad_alloc` on exhaustion, `try_allocate()` is `noexcept` and returns `nullptr`.

```cpp
#include <it/d4np/memorypool/memory_pool.hpp>
using namespace it::d4np::memorypool;

Pool pool(64, 1024);                       // throws std::bad_alloc on bad config
if (void* block = pool.try_allocate()) {   // non-throwing verb
    pool.deallocate(block);
}

// Failure as a value instead of an exception — Factory Method / Builder (ADR-0011):
if (std::optional<Pool> p = Pool::make(64, 1024)) {
    void* b = p->allocate();               // throwing verb — never nullptr
    p->deallocate(b);
}

auto built = PoolBuilder{}.with_block_size(64).with_block_count(1024).build();
```

### Type-safe pool — `TypedPool<T>`

Derives a spec-conformant `block_size` from `T` at compile time and adds object-lifetime verbs (`construct` placement-news, `destroy` runs the destructor).

```cpp
#include <it/d4np/memorypool/typed_pool.hpp>

TypedPool<Widget> pool(1024);
Widget* w = pool.construct(arg1, arg2);    // allocate + placement-new (strong guarantee)
// ... use w ...
pool.destroy(w);                           // ~Widget() + return the slot to the pool
```

### STL containers — `PoolAllocator<T>`

A *Cpp17Allocator* Adapter ([ADR-0018](docs/adr/0018-stl-allocator-adapter.md)). Node-based containers (`std::list`, `std::map`, `std::set`) run on the O(1) pool fast path; oversized / multi-element requests transparently fall back to `::operator new`. The pool must out-live the container.

```cpp
#include <it/d4np/memorypool/pool_allocator.hpp>
#include <list>

Pool pool(64, 1024);                       // 64-byte blocks fit a list<int> node
std::list<int, PoolAllocator<int>> values{PoolAllocator<int>{pool}};
for (int i = 0; i < 100; ++i) {
    values.push_back(i);                   // each node served from the pool
}
```

### Dynamic growth

Opt-in, per-pool ([ADR-0022](docs/adr/0022-dynamic-growth-policy-and-chunk-linking.md)): on exhaustion the pool acquires a new contiguous chunk and grows geometrically instead of failing. Default pools stay fixed-size.

```cpp
if (std::optional<Pool> pool = Pool::make_dynamic(64, 256, /*growth_factor=*/2)) {
    for (int i = 0; i < 100000; ++i) {
        (void)pool->try_allocate();        // grows 256 → 512 → … instead of returning nullptr
    }
}
```

### Observability — `InstrumentedPool` + `PoolObserver`

An opt-in Decorator ([ADR-0025](docs/adr/0025-decorator-for-instrumented-pool.md)) that counts allocation activity and emits lifecycle events ([ADR-0026](docs/adr/0026-observer-for-pool-lifecycle-events.md)). A program that uses `Pool` directly pays nothing.

```cpp
#include <it/d4np/memorypool/instrumented_pool.hpp>

struct LoggingObserver : PoolObserver {
    void on_pool_event(PoolEvent event, const PoolStats& stats) noexcept override {
        if (event == PoolEvent::exhausted) {
            std::cerr << "pool exhausted at " << stats.live_ << " live blocks\n";
        }
    }
};

if (auto pool = InstrumentedPool::make(64, 1024)) {
    LoggingObserver observer;
    pool->add_observer(observer);          // observer must out-live the pool
    void* b = pool->try_allocate();
    pool->deallocate(b);
    PoolStats s = pool->stats();           // allocations_, deallocations_, live_, peak_live_, …
    pool->write_summary(std::cout);
}
```

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

**Summary.** Against `malloc`/`free`, a pre-sized fixed pool is **4–11× faster** single-threaded; a *growing* pool stays **~2× faster** even while it acquires chunks; under thread contention the lock-free policy beats the mutex but a single-shared-head pool does not out-scale `malloc`'s per-thread arenas. All numbers below are on the maintainer's **Intel i5-6600K (Skylake) × Windows 10 × MSVC 19.51 Release** workstation, 64-byte blocks, 1,000,000 iterations × 10 repeats (first dropped as warm-up). Methodology contract: [ADR-0014](docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md).

**Fixed, single-threaded** (M2.9 / spec §6.3 — full report: [`docs/bench/v0.2.0-windows-msvc-x64.md`](docs/bench/v0.2.0-windows-msvc-x64.md)):

| Scenario     | median `malloc` (ns/op) | median `pool` (ns/op) | `malloc` / `pool` |
|--------------|------------------------:|----------------------:|------------------:|
| bulk-alloc   | 75.5                    | 6.9                   | **11.02 ×**       |
| bulk-free    | 44.5                    | 8.3                   | **5.35 ×**        |
| interleaved  | 49.9                    | 11.2                  | **4.45 ×**        |

**Dynamic growth** (M5.4 — full report: [`docs/bench/v0.5.0-windows-msvc-x64-growth.md`](docs/bench/v0.5.0-windows-msvc-x64-growth.md)): a pool that grows from 256 blocks to 1,000,000 during the run does amortized bulk-alloc at **55 ns/op vs `malloc`'s 108 — 1.96×** faster, the cost of ~12 geometric chunk acquisitions. Size up front when the working set is known (the ~11× path); use growth when it is not.

**Threading** (M4.5 — full report: [`docs/bench/v0.4.0-windows-msvc-x64-threading.md`](docs/bench/v0.4.0-windows-msvc-x64-threading.md)): `--scenario concurrent` runs `T` threads against a shared pool, built per thread-safety policy. The single-thread fast path is preserved (`NONE` ≈ 9 ns/op interleaved, unchanged); under 4-thread contention `LOCKFREE` (41.8 ns/op) beats `MUTEX` (69.5 ns/op).

The bench binary is built off by default; the `bench` preset (Release + benchmarks ON + tests OFF) opts in:

```bash
cmake --preset bench
cmake --build --preset bench
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench
```

Reports for other host × compiler combinations (Linux / GCC, Linux / Clang, macOS / Apple Clang) are welcome — see [`docs/bench/README.md`](docs/bench/README.md) for the contribution recipe.

## Status

`v1.1.1` — **maintenance** (documentation / process / tooling), the first post-`v1.1.0` PATCH. The shipped library is byte-identical to `v1.1.0` — no API/ABI/behaviour change. Adds the in-repo [bug ledger](docs/bugs/) + triage protocol ([ADR-0039](docs/adr/0039-bug-ledger-and-triage-protocol.md)), a [PR-metadata policy](docs/adr/0040-pull-request-metadata-policy.md), a [`SECURITY.md`](SECURITY.md), `packaging-smoke` CI for the vcpkg/Conan recipes, the [session journal](docs/journal/) ([ADR-0036](docs/adr/0036-session-journal-extraction.md)), the new-feature roadmap-placement rule ([ADR-0037](docs/adr/0037-new-feature-roadmap-placement.md)), and the per-release changelog split ([ADR-0038](docs/adr/0038-changelog-version-split.md)). Five new ADRs (0036–0040) bring the total to 40. Release notes: [`docs/releases/v1.1.1.md`](docs/releases/v1.1.1.md). The earlier line:

`v1.1.0` — **internationalization & post-release governance** (Milestone 8), the first post-1.0 MINOR. Purely **additive** — the library binary is unchanged from `v1.0.x`. Documentation now ships in **Simplified Chinese (`zh-Hans`) and Japanese (`ja`)** (English stays normative — [`docs/i18n/`](docs/i18n/), [ADR-0032](docs/adr/0032-documentation-i18n-architecture.md)); the spec is English-normative ([ADR-0033](docs/adr/0033-english-as-the-spec-normative-language.md)); a [post-release maintenance protocol](docs/workflow/maintenance.md) ([ADR-0034](docs/adr/0034-post-release-maintenance-protocol.md)) governs the maintained-product phase; and an agent-runnable [`consistency lint`](tools/consistency_lint.py) ([ADR-0035](docs/adr/0035-agent-runnable-consistency-lint.md)) gates cross-artifact congruence in CI and the agent contract. Four new ADRs (0032–0035) bring the total to 35. Release notes: [`docs/releases/v1.1.0.md`](docs/releases/v1.1.0.md). The earlier line:

`v1.0.1` — **packaging patch** over the frozen `v1.0.0` API: adds the vcpkg port and the Conan 2.x recipe (Phase 2 distribution, ADRs [0030](docs/adr/0030-vcpkg-port.md) / [0031](docs/adr/0031-conan-recipe.md)). The shipped library is byte-identical to `v1.0.0` — a `PATCH`, no API/ABI/behaviour change. Release notes: [`docs/releases/v1.0.1.md`](docs/releases/v1.0.1.md). The stable baseline it patches:

`v1.0.0` — **the first stable release.** The public C ABI (`memory_pool_create` / `_alloc` / `_free` / `_destroy` plus the O(1) introspection accessors) and the C++ surface (`Pool`, `TypedPool<T>`, `PoolAllocator<T>`, `InstrumentedPool`, `PoolObserver`) are frozen under the SemVer 1.0 promise — no breaking change without a `2.0.0`. `v1.0.0` seals the feature set built across Milestones 0–6 — the O(1) implicit-free-list fixed-block pool with zero per-block metadata, the RAII / typed / STL-allocator C++ wrappers, compile-time-configurable thread safety, optional geometric dynamic growth, and opt-in observability — and adds the Milestone 7 polish: the published Doxygen API site (M7.1), the expanded usage / performance / compatibility README (M7.2), `find_package` install + pkg-config packaging (M7.4), and the patterns-catalogue (M7.5) and specification-compliance (M7.6, [ADR-0029](docs/adr/0029-spec-compliance-acceptance-audit.md)) acceptance audits. All fifteen Spec Coverage Map rows are ✅, re-verified end-to-end. Twenty-nine ADRs (0001–0029) record every decision; all eleven adopted design patterns are Implemented. Release notes for `v1.0.0` live in [`docs/releases/v1.0.0.md`](docs/releases/v1.0.0.md).

| Milestone | Title                              | Status      |
|-----------|------------------------------------|-------------|
| 0         | Agent & Workflow Scaffolding       | ✅ complete |
| 1         | Build System & Project Skeleton    | ✅ complete |
| 2         | Core Memory Pool (single-threaded) | ✅ complete |
| 3         | C++ Wrapper & Type Safety          | ✅ complete |
| 4         | Thread-Safe Variant                | ✅ complete |
| 5         | Dynamic Growth Mode                | ✅ complete |
| 6         | Observability & Decorators         | ✅ complete |
| 7         | Release & Polish                   | ✅ complete |
| 8         | i18n & Post-Release Governance     | ✅ complete |

See [`ROADMAP.md`](ROADMAP.md) for the per-task breakdown and the Spec Coverage Map at the bottom (traceability from spec sections to roadmap items).

## Compatibility

The library targets **C++17** (strict, no compiler extensions) for the implementation and **ANSI C (C89) + C99** for the public C header — both verified in CI. There are **no external dependencies**: only the C and C++ standard libraries. The full rationale and floor-version reasoning are in [ADR-0005](docs/adr/0005-toolchain-matrix-and-supported-platforms.md).

**Tier-1 — gated on every PR** (Linux × {GCC, Clang} + Windows × MSVC + macOS × Apple Clang, across Debug / Release and, where applicable, ASan / UBSan / TSan):

| OS      | Arch   | Compilers (minimum)                                        |
|---------|--------|------------------------------------------------------------|
| Linux   | x86_64 | GCC ≥ 11, Clang ≥ 14                                       |
| Windows | x86_64 | MSVC ≥ 19.30 (VS 2022 17.0); clang-cl ≥ 14 optional        |
| macOS   | arm64  | Apple Clang ≥ 14 (Xcode 14)                                |

**Tier-2 — best-effort, not gated:** Linux aarch64, macOS x86_64 (pre-Apple-Silicon), FreeBSD x86_64, MinGW-w64 on Windows.

**Language standards:**

| Surface             | Standard                       | Build flags                                                       |
|---------------------|--------------------------------|-------------------------------------------------------------------|
| C++ implementation  | C++17 (strict)                 | `-std=c++17` / `/std:c++17`, `-Wall -Wextra -Wpedantic -Werror` / `/W4 /WX` |
| C public header     | ANSI C (C89) **and** C99       | dedicated CI jobs: `-std=c89 -pedantic -Werror`, `-std=c99 -pedantic -Werror` |

**Thread safety** is selected at build time via the `PBR_MEMORY_POOL_THREAD_SAFETY` CMake option — `NONE` (default, single-thread fast path), `MUTEX`, or `LOCKFREE` ([ADR-0020](docs/adr/0020-thread-safety-strategy-and-compile-time-knob.md)). Dynamic growth is supported under `NONE` and `MUTEX` (not `LOCKFREE` — [ADR-0024](docs/adr/0024-dynamic-growth-synchronization-and-creation-surface.md) §2).

## Technology stack

The library has **zero runtime/build dependencies** beyond the C and C++ standard libraries (spec §3.3); everything below is either a language standard, a build/test/docs tool, or a packaging integration. The consumer-facing compiler & platform matrix is in [Compatibility](#compatibility) above.

| Layer | Technology | Version |
|-------|------------|---------|
| Language (impl) | C++ | C++17 (strict, no extensions) |
| Language (C public header) | ANSI C | C89 **and** C99 |
| Build system | CMake | ≥ 3.21 |
| Build generator | Ninja (CI & presets) | any recent |
| Compilers (floor) | GCC / Clang / MSVC / Apple Clang | 11 / 14 / 19.30 (VS 2022 17.0) / 14 ([ADR-0005](docs/adr/0005-toolchain-matrix-and-supported-platforms.md)) |
| Unit tests | [doctest](https://github.com/doctest/doctest) (via CMake `FetchContent`, test-only) | v2.4.11 |
| Runtime sanitizers | ASan · UBSan · TSan | compiler-bundled |
| Memory checker | Valgrind | distro (CI: Ubuntu 24.04) |
| Static analysis / style | clang-tidy · clang-format | LLVM 14+ |
| API docs | Doxygen → GitHub Pages | 1.10.x ([ADR-0027](docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)) |
| Project tooling | Python (consistency lint, stdlib-only) | 3.x ([ADR-0035](docs/adr/0035-agent-runnable-consistency-lint.md)) |
| Packaging | CMake `find_package` + pkg-config · vcpkg port · Conan recipe | [ADR-0028](docs/adr/0028-install-and-packaging-layout.md) / [0030](docs/adr/0030-vcpkg-port.md) / [0031](docs/adr/0031-conan-recipe.md) |
| CI | GitHub Actions | — |
| Runtime dependencies | **none** | — |

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

Full quality contract: [`AGENTS.md`](AGENTS.md) §10. The C++ build matrix, sanitizers, `clang-format`, `clang-tidy` diff gate, ANSI C / C99 verification, and zero-external-dependency audit run on every PR via [`ci.yml`](.github/workflows/ci.yml); a docs-only workflow ([`docs.yml`](.github/workflows/docs.yml)) covers markdownlint, internal link checks, and ADR numbering sanity; a docs-site workflow ([`docs-site.yml`](.github/workflows/docs-site.yml)) builds the Doxygen API reference as a warn-as-error gate on every PR and publishes it to GitHub Pages on push to `master` ([ADR-0027](docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)). The badges above gate `master`.

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

## Install and consume

Install to a prefix and consume with CMake's `find_package` ([ADR-0028](docs/adr/0028-install-and-packaging-layout.md) — Phase 1 distribution):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPBR_MEMORY_POOL_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
# In the consumer's CMakeLists.txt — the imported target name is identical
# whether you install the package or vendor it via add_subdirectory / FetchContent.
find_package(pbr_memory_pool CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

The install tree carries every public header (`include/it/d4np/memorypool/`), the static archive, the CMake package config (`SameMajorVersion` compatibility), and a pkg-config `pbr-memory-pool.pc` for non-CMake (Make / autotools / Meson) builds. The same `cmake --install` tree is what each GitHub Release tarball contains. Vendoring without installing also works:

```cmake
add_subdirectory(path/to/pbr-cpp-memory-pool)   # or FetchContent
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

**vcpkg** (Phase 2 — [ADR-0030](docs/adr/0030-vcpkg-port.md)): a port pinned to `v1.0.0` ships in [`ports/`](ports/), consumable today as an overlay port (the same `pbr::memory_pool` target):

```bash
vcpkg install pbr-memory-pool --overlay-ports=ports
```

**Conan** (Phase 2 — [ADR-0031](docs/adr/0031-conan-recipe.md)): a Conan 2.x recipe pinned to `v1.0.0` ships in [`conan/`](conan/), creatable today (same `pbr::memory_pool` target via `CMakeDeps`):

```bash
conan create conan/        # then depend on pbr-memory-pool/1.0.0
```

Registry publication (microsoft/vcpkg, ConanCenter / self-hosted) is deferred — see [`ports/README.md`](ports/README.md) and [`conan/README.md`](conan/README.md).

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
| [`docs/i18n/`](docs/i18n/)                    | Documentation translations (`zh-Hans`, `ja`); English is normative ([ADR-0032](docs/adr/0032-documentation-i18n-architecture.md)). |

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
