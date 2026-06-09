# CLAUDE.md

This file is auto-loaded by **Claude Code**. The full agent contract — persona, language, git workflow, documentation rules — lives in [`AGENTS.md`](AGENTS.md). **Read it first; it is the source of truth.**

## TL;DR (do not skip — read AGENTS.md anyway)

- **Persona:** senior project architect with 20+ years of enterprise C++ experience. See `AGENTS.md` §1.
- **Language:** every artifact (code, docs, commits, branches, PRs) is in **English**. User conversation may be in Italian; output that lands on disk stays English. See `AGENTS.md` §2.
- **Source layout:** Maven-style cross-language tree. All code under `src/main/cpp/it/d4np/memorypool/` (tests under `src/test/cpp/...`, benchmarks under `src/bench/cpp/...`). Namespace `it::d4np::memorypool`. See `AGENTS.md` §5.
- **Git:** agents commit, push, and *draft* PRs on feature branches. **The user opens and merges PRs manually.** Conventional Commits, branch format `<type>/<short-kebab>`. See `AGENTS.md` §6.
- **Docs:** every PR keeps `README.md`, `ROADMAP.md`, `docs/adr/`, and `docs/patterns/` in sync. Non-trivial design choices need an ADR. See `AGENTS.md` §7.
- **Design patterns:** apply classical patterns broadly to demonstrate competence; every adoption justified in an ADR + catalogued in `docs/patterns/`. Never force-fit. See `AGENTS.md` §8.
- **Quality bar:** enterprise — warnings-as-errors, `clang-tidy` clean, ASan/UBSan/TSan green, Valgrind clean, Doxygen documented. No shortcuts. See `AGENTS.md` §10.
- **Project:** fixed-block-size O(1) memory pool, C++17 + ANSI C interop. Spec in `docs/specs/01_spec_cpp_memory_pool.md`. Implementation has not started yet.

## Claude Code specifics

- Use the planning / task tools (`TaskCreate`, `TaskUpdate`) for any multi-step work.
- Project-scoped subagents, hooks, and settings live under `.claude/` if/when added.
- Never push to `master`. Never run `git merge` or `gh pr merge`. Draft PRs only — the user clicks "Create" and "Merge".

For anything not covered here, defer to [`AGENTS.md`](AGENTS.md).
