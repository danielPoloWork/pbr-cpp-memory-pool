# Roadmap

Plan of work for `pbr-cpp-memory-pool`, grouped by milestone. Each item is numbered and checkbox-driven — every PR that completes a roadmap item flips the corresponding checkbox in the same PR. Completed items remain in the list as a permanent record.

> Conventions: items are numbered `<milestone>.<task>`. New work that emerges mid-flight is appended at the end of its milestone with a fresh number; existing numbers are never reused or renumbered.

---

## Milestone 0 — Agent & Workflow Scaffolding

Goal: establish the documentation, agent configuration, source-tree shape, and quality bar before writing a single line of pool code.

- [x] 0.1 Persist the senior-C++-architect persona and English-only working language in the cross-tool agent configuration.
- [x] 0.2 Create `AGENTS.md` as the single source of truth for agent behavior.
- [x] 0.3 Add `CLAUDE.md` and `GEMINI.md` adapters that defer to `AGENTS.md`.
- [x] 0.4 Scaffold `docs/` with `README.md`, `adr/`, `specs/`, and `workflow/`.
- [x] 0.5 Add ADR template, ADR index, and ADR-0001 ("Record architecture decisions").
- [x] 0.6 Document the git workflow (branches, Conventional Commits, PR template) in `docs/workflow/git-workflow.md`.
- [x] 0.7 Document the documentation-maintenance rules in `docs/workflow/documentation.md`.
- [x] 0.8 Create `ROADMAP.md` with numbered, checkbox-driven milestones (this file).
- [x] 0.9 Refresh `README.md` with project description, status, and pointers to `AGENTS.md`, `ROADMAP.md`, `docs/`.
- [x] 0.10 Relocate the initial spec to `docs/specs/01_spec_cpp_memory_pool.md`.
- [x] 0.11 Codify the enterprise quality bar (warnings-as-errors, sanitizers, Valgrind, `clang-tidy`, Doxygen) in `AGENTS.md` §10.
- [x] 0.12 Adopt the Maven-style cross-language source layout under `src/main/cpp/it/d4np/memorypool/` — ADR-0002.
- [x] 0.13 Adopt the design-patterns policy and create the patterns catalogue — ADR-0003 + `docs/patterns/README.md`.
- [x] 0.14 Scaffold the production / test / benchmark source roots with placeholders and `src/README.md`.

## Milestone 1 — Build System & Project Skeleton

Goal: a clean, reproducible build with empty stubs that compile, link, and run a no-op test under CTest — built against the Maven-style source tree.

- [ ] 1.1 ADR: C++17 toolchain matrix (MSVC, GCC, Clang — Debug & Release) and supported platforms.
- [ ] 1.2 Add `CMakeLists.txt` exposing `src/main/cpp` as the public include root; declare `pbr_memory_pool` library target with sources globbed under `src/main/cpp/it/d4np/memorypool/`.
- [ ] 1.3 Add `CMakePresets.json` with `debug`, `release`, `asan`, `ubsan`, `tsan` presets.
- [ ] 1.4 Add `.clang-format` (LLVM derivative, 4-space indent, 120-col soft limit) — ADR for style baseline.
- [ ] 1.5 Add `.clang-tidy` with the baseline check set declared in `AGENTS.md` §9 — ADR if checks deviate from that baseline.
- [ ] 1.6 Add `src/main/cpp/it/d4np/memorypool/memory_pool.h` (public C API skeleton) and `memory_pool.hpp` (C++ wrapper skeleton) — no-op definitions, fully documented.
- [ ] 1.7 Add CTest wiring; create a no-op smoke test under `src/test/cpp/it/d4np/memorypool/`.
- [ ] 1.8 Set up CI workflow: build matrix, `clang-tidy`, ASan + UBSan, CTest — gate `master` on all green.
- [ ] 1.9 README quickstart: build / test commands verified on Windows and Linux.

## Milestone 2 — Core Memory Pool (Single-Threaded MVP)

Goal: a correct, leak-free, O(1) fixed-block pool matching the spec — single-threaded, no dynamic growth yet, with measured demonstrative patterns.

- [ ] 2.1 ADR: implicit free-list layout, `block_size` minimum, and alignment guarantee.
- [ ] 2.2 ADR: introduce the **RAII** wrapper (`Pool`) and the **Pimpl** idiom across the C++/C boundary; update patterns catalogue.
- [ ] 2.3 Implement `memory_pool_create` / `memory_pool_destroy` with contiguous backing allocation.
- [ ] 2.4 Implement `memory_pool_alloc` / `memory_pool_free` against the implicit free list.
- [ ] 2.5 Implement the C++ `it::d4np::memorypool::Pool` RAII wrapper.
- [ ] 2.6 ADR + impl: **Factory Method** / **Builder** for constructing configured pool instances (block size, block count, future growth/threading knobs).
- [ ] 2.7 Correctness tests: full exhaustion, null inputs, double-free detection policy, foreign-pointer policy.
- [ ] 2.8 Valgrind job in CI — gate on `ERROR SUMMARY: 0 errors from 0 contexts`.
- [ ] 2.9 Microbenchmark vs `malloc`/`free` over 1,000,000 iterations under `src/bench/cpp/it/d4np/memorypool/`; numbers committed and summarised in the README.

## Milestone 3 — C++ Wrapper & Type Safety

Goal: an idiomatic C++17 wrapper around the C core, with RAII and allocator-aware ergonomics.

- [ ] 3.1 ADR: exception policy at the C / C++ boundary.
- [ ] 3.2 `it::d4np::memorypool::TypedPool<T>` template with RAII lifetime and `try_allocate` / `allocate` variants.
- [ ] 3.3 ADR + impl: **Adapter** — STL-compatible allocator over the underlying pool; propagation traits specified in the ADR.
- [ ] 3.4 ADR + impl: **Iterator** (read-only) over the free list for diagnostics — disabled in release builds unless explicitly enabled.
- [ ] 3.5 Tests against `std::vector`, `std::list`, and a small custom container.

## Milestone 4 — Thread-Safe Variant

Goal: opt-in thread safety with a measurable single-threaded fast path preserved.

- [ ] 4.1 ADR: thread-safety **Strategy** (lock-free CAS vs. mutex vs. per-thread caches) and configuration knob.
- [ ] 4.2 ADR + impl: **Template Method** allocation skeleton with hook points for the chosen Strategy.
- [ ] 4.3 Implementation behind a compile-time switch; default remains single-threaded.
- [ ] 4.4 Concurrent stress tests; TSan job added to CI.
- [ ] 4.5 Comparative benchmark: single-thread fast path vs. concurrent path.

## Milestone 5 — Dynamic Growth Mode

Goal: optional behavior where the pool acquires additional contiguous chunks when exhausted.

- [ ] 5.1 ADR: growth policy (geometric vs. linear) and chunk-linking strategy.
- [ ] 5.2 ADR + impl: **Composite** chunk-list representation linking the original pool with overflow chunks.
- [ ] 5.3 Implementation behind a runtime / compile-time flag.
- [ ] 5.4 Tests and benchmarks covering exhaustion-and-grow scenarios.

## Milestone 6 — Observability & Decorators

Goal: optional logging / statistics / tracing without touching the hot path of release builds.

- [ ] 6.1 ADR + impl: **Decorator** for an instrumented pool variant (counters, allocation histogram, optional logging).
- [ ] 6.2 ADR + impl: **Observer** for pool-lifecycle events (exhaustion, growth, destruction).
- [ ] 6.3 Tests verifying zero-overhead in release builds when instrumentation is disabled.

## Milestone 7 — Release & Polish

Goal: ship a v1.0.0 reference implementation.

- [ ] 7.1 Doxygen-generated API documentation published as a static site.
- [ ] 7.2 README: full usage example, performance summary, compatibility matrix.
- [ ] 7.3 `CHANGELOG.md` following Keep a Changelog conventions.
- [ ] 7.4 ADR: install / packaging layout (public-header export, pkg-config, CMake config file).
- [ ] 7.5 Patterns catalogue audit — verify every adopted pattern has both an ADR and a code location; refresh statuses.
- [ ] 7.6 Tag `v1.0.0` and draft GitHub release notes.

---

**Status check:** As of 2026-06-09 — Milestone 0 complete; Milestone 1 ready to start.
