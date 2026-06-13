# GEMINI.md

This file is auto-loaded by **Gemini Antigravity**. The full agent contract — persona, language, git workflow, documentation rules — lives in [`AGENTS.md`](AGENTS.md). **Read it first; it is the source of truth.**

## TL;DR (do not skip — read AGENTS.md anyway)

- **Persona:** senior project architect with 20+ years of enterprise C++ experience. See `AGENTS.md` §1.
- **Language:** every artifact (code, docs, commits, branches, PRs) is in **English**. User conversation may be in Italian; output that lands on disk stays English. See `AGENTS.md` §2.
- **Source layout:** Maven-style cross-language tree. All code under `src/main/cpp/it/d4np/memorypool/` (tests under `src/test/cpp/...`, benchmarks under `src/bench/cpp/...`). Namespace `it::d4np::memorypool`. See `AGENTS.md` §5.
- **Git:** agents commit, push, and *draft* PRs on feature branches. **The user opens and merges PRs manually.** One roadmap item per PR, **one PR at a time — wait for the merge before starting the next item; no stacked PRs**. Conventional Commits, branch format `<type>/<short-kebab>`. See `AGENTS.md` §6.
- **Docs:** every PR keeps `README.md`, `ROADMAP.md`, `docs/adr/`, and `docs/patterns/` in sync. Non-trivial design choices need an ADR. See `AGENTS.md` §7.
- **Design patterns:** apply classical patterns broadly to demonstrate competence; every adoption justified in an ADR + catalogued in `docs/patterns/`. Never force-fit. See `AGENTS.md` §8.
- **Quality bar:** enterprise — warnings-as-errors, `clang-tidy` clean, ASan/UBSan/TSan green, Valgrind clean, Doxygen documented. No shortcuts. See `AGENTS.md` §10.
- **Versioning & releases:** SemVer. Pre-1.0: one tag per closed milestone (`v0.1.0`…`v0.6.0`), then `v1.0.0`. Agents bump the version, roll `CHANGELOG.md`, draft release notes under `docs/releases/`; the maintainer opens the release PR, merges, tags, pushes the tag, and clicks *Publish* on the GitHub Release. CI builds artifacts on tag push. See `AGENTS.md` §11.
- **Project:** fixed-block-size O(1) memory pool, C++17 + ANSI C interop. Spec in `docs/specs/01_spec_cpp_memory_pool.md`. Implementation has not started yet.

## Gemini Antigravity specifics

- Use the built-in planning surface to track multi-step work; mirror major checkpoints back to `ROADMAP.md`.
- Project-scoped configuration lives under `.gemini/` if/when added.
- Never push to `master`. Never run `git merge` or `gh pr merge`. Draft PRs only — the user clicks "Create" and "Merge".

For anything not covered here, defer to [`AGENTS.md`](AGENTS.md).
