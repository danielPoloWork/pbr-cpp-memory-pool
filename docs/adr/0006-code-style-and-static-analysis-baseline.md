# ADR-0006: Code style and static-analysis baseline

- **Status:** Accepted
- **Date:** 2026-06-10
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §9 / §10; [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md); ROADMAP §1.4 / §1.5 / §1.8

## Context

`AGENTS.md` §9 carried provisional defaults for naming, line width, indent, formatter, and the `clang-tidy` check-set — explicitly marked "finalised in Milestone 1". The implementation-phase milestones (M2 and later) will produce thousands of lines of code reviewed under those rules, so the rules need to be concrete, repo-local, and tool-enforced from the moment any non-stub code lands.

Two artefacts are required:

- **`.clang-format`** — automatic formatter config. Without it, every IDE and contributor reformats to their own defaults and the diff noise drowns out the actual review surface.
- **`.clang-tidy`** — static-analysis config. Without it, the enterprise quality-bar in `AGENTS.md` §10 ("`clang-tidy` clean on the diff; no broad disables") has no operational meaning.

Both files live at the repo root because that is where `clang-format` and `clang-tidy` look by default (closest-ancestor lookup). Sub-tree overrides are not required at this scale.

The provisional defaults in `AGENTS.md` §9 give us a starting point: LLVM-derived style, 4-space indent, 120-column soft limit; `bugprone-*` / `cert-*` / `cppcoreguidelines-*` / `performance-*` / `readability-*` check set. This ADR turns those bullet-points into shipping configs and records the specific deviations from `clang-tidy`'s defaults.

## Decision

### 1. Formatter — `clang-format`

We adopt **LLVM-derived style** with project-specific overrides codified in [`.clang-format`](../../.clang-format). The full file is normative; the deviations from upstream LLVM defaults are:

| Setting                       | LLVM default | This project | Rationale                                                                 |
|-------------------------------|--------------|--------------|---------------------------------------------------------------------------|
| `ColumnLimit`                 | 80           | **120**      | Modern displays; rule-of-thumb for enterprise C++. Soft limit — `clang-format` may exceed for readability. |
| `IndentWidth`                 | 2            | **4**        | Higher contrast between scopes. Matches the maintainer's pre-existing preference. |
| `AccessModifierOffset`        | -2           | **-4**       | Tracks `IndentWidth` so `public:` / `private:` sit at the class-brace column. |
| `PointerAlignment`            | Right        | **Left**     | `int* p` not `int *p`. Aligns with the "the type owns the asterisk" reading common in modern C++ guides. |
| `AlwaysBreakTemplateDeclarations` | MultiLine | **Yes**    | Templates always break onto their own line — easier to scan, friendlier for diff review. |
| `FixNamespaceComments`        | true         | **true** (explicit) | Re-asserts the LLVM default so editors can't drop it accidentally. |
| `IncludeBlocks` / `IncludeCategories` | Preserve / unset | **Regroup with priorities** | Auto-group and order includes: own headers first (`<it/d4np/memorypool/...>`), then C++ stdlib, then C stdlib, then third-party `.hpp`, then third-party `.h`. Eliminates merge conflicts on include order. |
| `Standard`                    | (detect)     | **c++17**    | Match ADR-0005 §3.                                                        |
| `Language`                    | (detect)     | **Cpp**      | Single language; explicit avoids surprises.                               |
| `UseTab`                      | Never        | **Never** (explicit) | Tabs banned — no mixed indentation, no tab-vs-space wars.            |

`clang-format` is invoked as `clang-format -i <file>` to apply, or `clang-format --dry-run --Werror <file>` in CI to verify.

### 2. Static analysis — `clang-tidy`

The full check-set lives in [`.clang-tidy`](../../.clang-tidy). It enables:

- `bugprone-*` — likely-incorrect patterns the compiler does not warn on.
- `cert-*` — CERT C++ secure-coding rules.
- `clang-analyzer-*` — the static analyser (on by default; re-asserted).
- `cppcoreguidelines-*` — the C++ Core Guidelines as enforced by clang-tidy.
- `modernize-*` — encourages C++17 idioms over C-style or pre-C++11 patterns.
- `performance-*` — flags O(n)-or-worse patterns where O(1) is available.
- `portability-*` — non-portable assumptions (endianness, size_t, …).
- `readability-*` — naming, identifier length, function size, cognitive complexity.

**Deviations** — specific checks disabled, each with a recorded reason. Anything not listed here is on.

| Check disabled                                          | Reason                                                                                                                                          |
|---------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| `cert-err58-cpp`                                        | Bans throwing constructors on static objects. Acceptable in our scope; `std::bad_alloc` from `Pool` construction at namespace scope is in spec. |
| `cppcoreguidelines-avoid-magic-numbers`, `readability-magic-numbers` | Too noisy in test fixtures and microbenchmarks where literal sizes are the entire point. Re-enable per-file with `// NOLINT` if a magic number needs flagging. |
| `cppcoreguidelines-pro-bounds-pointer-arithmetic`       | A pool allocator does pointer arithmetic by definition. Disabling at the check level is honest; per-line `// NOLINT` would be noise.            |
| `modernize-use-trailing-return-type`                    | Stylistic preference; trailing return types are not adopted in this project.                                                                    |
| `modernize-use-using`                                   | The public C-ABI header (`memory_pool.h`) must remain compilable under C89/C99 per ADR-0005 §3; `using` is a C++-only syntax and `typedef` is the only portable form. Disabling at the check level is more honest than scattering `// NOLINT` annotations on every C-compat typedef. |
| `readability-identifier-length`                         | Default lower bound is 1 — clean for general code, noisy for `i`, `j`, `n` in tight loops. Re-enable with a project-wide config-option if needed. |

In addition, the `readability-identifier-naming.MacroDefinitionIgnoredRegexp` option carries an exemption for macros matching `IT_D4NP_.*` — the project's include guards embed `D4NP` as a single word (mirroring the namespace and filesystem path locked by [ADR-0002](0002-adopt-cross-language-source-layout.md)) and end with a trailing underscore, both of which clang-tidy's default tokenizer rejects. The exemption is narrowly scoped to this prefix so generic macros (`PBR_FOO_BAR`) remain enforced.

`WarningsAsErrors` is **not** set in the config — diff-based enforcement happens at the CI layer (M1.8) via the `--warnings-as-errors='*'` flag. The config defines *what* to check; the CI defines *when* a finding blocks a PR.

`HeaderFilterRegex` is scoped to the project tree (`src/main/cpp/it/d4np/memorypool/.*\\.(h|hpp)$`) so third-party headers included transitively are not analysed.

`FormatStyle: file` ties `clang-tidy` fix-suggestions to the same `.clang-format`, keeping the two tools in lockstep.

### 3. Enforcement

- **Local pre-flight.** Contributors run `clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp' '*.h')` and `clang-tidy -p build/debug $(git diff --name-only --diff-filter=AM master...HEAD -- '*.cpp' '*.hpp' '*.h')` before opening a PR. The exact commands are documented in [`docs/development/local-build.md`](../development/local-build.md) §6.
- **CI.** The build-system CI job (ROADMAP §1.8) runs both as gating checks: `clang-format` over all tracked C++ sources, `clang-tidy` over the PR diff with `--warnings-as-errors='*'`. Broad `// NOLINT(...)` comments in source require an inline justification (one-line `// NOLINT: <reason>`).
- **Configuration drift.** The `.clang-format` and `.clang-tidy` files are themselves under review like any other code: changes go through an ADR amendment (or supersede this one) rather than a silent commit.

## Alternatives Considered

- **Google C++ Style** — rejected. Mature and ubiquitous, but its 2-space indent and 80-column limit do not match the maintainer's existing preference, and its naming conventions (CamelCase for functions) clash with the spec's C ABI (`memory_pool_create` is `snake_case` per spec §5).
- **Microsoft style** — rejected. Reasonable on Windows but uses 4-space indent and braces-on-new-line; the latter doubles vertical space in C++ where templates already inflate line counts.
- **Mozilla / Webkit / GNU** — rejected as outside the modern C++ ecosystem the project targets.
- **No formatter, "follow the file"** — rejected. Without a tool-enforced style every PR carries a side-channel discussion about whitespace.
- **`WarningsAsErrors: '*'` in `.clang-tidy` itself, not at CI** — rejected. Would force every contributor with a slightly different clang-tidy version (different default checks across LLVM 14 / 15 / 16 / …) to either match the maintainer's version or get spurious failures. The CI-only enforcement pins the version centrally.
- **Run `clang-tidy` on the entire repo at every PR** — rejected as too slow for the gating signal we want. Full-repo `clang-tidy` runs as a nightly informational job once the CI matrix in M1.8 lands. Diff-based runs are the gate.
- **Add `cppcheck` alongside `clang-tidy`** — deferred. `clang-tidy`'s coverage is comparable for our needs; adding a second tool means a second tuning surface. Reconsider if a `cppcheck` find legitimately misses in `clang-tidy`.

## Consequences

**Positive**

- Every code-bearing PR from M2 onwards is formatted and analysed identically across all four supported compilers (ADR-0005).
- The configs are repo-local — IDEs that read `.clang-format` / `.clang-tidy` (CLion, VS, VS Code, Vim with plugins) inherit the project policy automatically with zero per-user setup.
- The deviation table makes every disabled `clang-tidy` check auditable. Re-enabling a check is a deliberate PR, not a silent vote.

**Negative**

- A contributor on `clang-format` / `clang-tidy` older than LLVM 14 may see slight diff noise (settings introduced after LLVM 13 are unknown to older binaries — they are ignored, not error). The local-build guide pins the floor to LLVM 14.
- The diff-based CI gate is not retroactive — pre-existing code is grandfathered until touched. Net effect at this point in the project is zero (only stubs exist), but a follow-up ADR may want to run full-repo `clang-tidy --fix` once code volume justifies it.

## References

- LLVM Style — <https://llvm.org/docs/CodingStandards.html>
- clang-format options — <https://clang.llvm.org/docs/ClangFormatStyleOptions.html>
- clang-tidy checks — <https://clang.llvm.org/extra/clang-tidy/checks/list.html>
- C++ Core Guidelines — <https://isocpp.github.io/CppCoreGuidelines/>
- CERT C++ Secure Coding — <https://wiki.sei.cmu.edu/confluence/display/cplusplus>
- [`AGENTS.md`](../../AGENTS.md) §9 / §10 — the bar this config implements.
- [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) — the toolchain matrix the configs must be portable across.
