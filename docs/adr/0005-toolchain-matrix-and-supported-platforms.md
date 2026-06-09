# ADR-0005: Toolchain Matrix and Supported Platforms

- **Status:** Accepted
- **Date:** 2026-06-09
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §9, §10; spec §3.3; ROADMAP item 1.1; future ADR on `clang-format` / `clang-tidy` (Milestones 1.4 / 1.5)

## Context

Milestone 1 begins the implementation phase. Before any line of CMake or C/C++ code lands we need to pin down which compilers, operating systems, architectures, and language standards `pbr-cpp-memory-pool` is contractually built and tested against. Without this contract:

- The CI matrix in [ROADMAP 1.8](../../ROADMAP.md) has no shape — we cannot decide which jobs gate `master`.
- The compatibility claim in spec §3.3 ("ANSI C o C++17 standard senza dipendenze esterne") cannot be objectively asserted; "ANSI C" admits at least four published standards (C89, C99, C11, C17) and "C++17" can be exercised at very different conformance levels across compilers.
- The future install / packaging layout (Milestone 7.4) and the package-registry recipes (Milestones 7.8 / 7.9) cannot be authored without a target list.

Two project-shape decisions drive the answer:

1. This is a **reference implementation** — its didactic value depends on being usable as a starting point on the platforms a learner is most likely to be on. Linux is non-negotiable; Windows is the maintainer's primary OS and a large share of downstream consumers; macOS rounds out the desktop trio.
2. The spec's "ANSI C" clause is most defensibly interpreted as C89 (ISO/IEC 9899:1990), but consumers writing modern code overwhelmingly target C99 or later. Verifying both is cheap insurance.

## Decision

We adopt the following toolchain matrix, normative for the project and binding on CI from Milestone 1.8 onwards.

### 1. Tier-1 platforms (gated in CI)

All three platforms below must build and test green on every PR before `master` accepts the merge.

| Platform           | Architecture | Primary compiler           | Secondary compiler          |
|--------------------|--------------|----------------------------|-----------------------------|
| Linux              | x86_64       | GCC ≥ 11                   | Clang ≥ 14                  |
| Windows            | x86_64       | MSVC ≥ 19.30 (VS 2022 17.0)| clang-cl ≥ 14 (optional)    |
| macOS              | arm64        | Apple Clang ≥ 14 (Xcode 14)| —                           |

**Rationale for the floor versions** — GCC 11 and Clang 14 both ship full C++17, are present on Ubuntu 22.04 LTS and on every actively maintained rolling distribution. MSVC 19.30 corresponds to Visual Studio 2022 17.0 (November 2021), which is the first release where `/std:c++17` and the C++17 standard library are simultaneously stable and complete. Apple Clang 14 ships with Xcode 14 (September 2022) and supports arm64 cleanly without legacy Intel ceremony.

### 2. Tier-2 platforms (best-effort, not gated)

Documented as expected to work, exercised by community contributors, but not run on every PR:

- Linux aarch64 (GCC ≥ 11, Clang ≥ 14)
- macOS x86_64 (Apple Clang ≥ 14) — pre-Apple-Silicon Macs
- FreeBSD x86_64 (Clang ≥ 14)
- MinGW-w64 on Windows (GCC ≥ 11) — for users who cannot run MSVC

Tier-2 platforms may be promoted to Tier-1 by a future ADR if usage and capacity justify it.

### 3. Language standards

| Component            | Standard                            | Enforcement                                                                 |
|----------------------|-------------------------------------|-----------------------------------------------------------------------------|
| C++ implementation   | C++17 (strict, no extensions)       | `-std=c++17` / `/std:c++17`; `-Wall -Wextra -Wpedantic -Werror` / `/W4 /WX` |
| C public header      | ANSI C (C89) **and** C99            | Dedicated CI job per standard with `-std=c89 -pedantic -Werror` and `-std=c99 -pedantic -Werror` compiling a minimal C translation unit that `#include`s the public header (ROADMAP 1.10) |
| Build configurations | Debug + Release                     | Mandatory presets; ASan / UBSan presets gated on every PR; TSan preset added when threading lands in Milestone 4 |

C11 and C17 verification are explicitly **not** required by this ADR. The spec calls for ANSI C compatibility; C99 is added as a pragmatic real-world baseline. If a downstream consumer reports a C11/C17 issue, a follow-up ADR may extend the matrix.

### 4. CI matrix shape

Each PR must clear the following cells (platform × compiler × configuration) before merge:

```
Linux x86_64   × GCC 11+        × {Debug, Release, ASan, UBSan}
Linux x86_64   × Clang 14+      × {Debug, Release, ASan, UBSan}
Windows x86_64 × MSVC 19.30+    × {Debug, Release}
macOS arm64    × Apple Clang 14+× {Debug, Release, ASan, UBSan}
```

Plus the two C-standard verification jobs (`-std=c89` and `-std=c99`) on Linux GCC, and the zero-external-dependency audit (ROADMAP 1.11). TSan is added to the Linux Clang column when threading lands.

That is a minimum of **15 gating jobs** per PR. Parallelism keeps wall-clock time bounded — each job is small (a single library + tests).

## Alternatives Considered

- **Linux only.** Rejected — most C++ portability bugs (MSVC's stricter template parsing, macOS's libc++ vs Linux's libstdc++) hide outside Linux. A reference that only builds on Linux is a partial reference.
- **Linux + Windows only.** Rejected — would miss libc++ entirely (Linux Clang typically defaults to libstdc++; macOS Apple Clang uses libc++). Allocator / containers code is sensitive to STL implementation differences.
- **Latest compilers only (GCC 14, Clang 18, MSVC 19.40).** Rejected — would exclude users on LTS distributions (Ubuntu 22.04 ships GCC 11, RHEL 9 ships GCC 11.5). LTS-friendly floors maximise the reachable consumer base.
- **Test every published C standard (C89, C99, C11, C17).** Rejected per maintainer decision — C89 + C99 covers the spec's literal ANSI clause plus the realistic consumer floor without C11/C17 ceremony.
- **Drop ASan/UBSan/TSan in favour of just Valgrind.** Rejected — sanitisers and Valgrind catch different bug classes (TSan finds data races Valgrind misses; ASan is cheaper to run on every PR than Valgrind). Both stay.
- **Allow compiler extensions when convenient (e.g., `__attribute__`, `__declspec` shorthands).** Rejected — `-Wpedantic -Werror` forbids them. The implementation may use them only behind a portable macro defined for each compiler, and only when justified in a per-feature ADR.

## Consequences

**Positive**

- Downstream consumers receive a clear, testable compatibility contract from the very first tag.
- Portability bugs are caught at PR time, not after release.
- The CI matrix is fully defined before any CMake is written, so M1.2 / M1.3 can be authored against a known target list rather than retrofitted later.
- The spec's "ANSI C" clause is interpreted explicitly and durably (C89 + C99), avoiding endless re-litigation.

**Negative**

- ~15 gating CI cells per PR (vs. ~3 if we picked Linux-only). GitHub Actions free-tier minutes are not infinite; cost discipline becomes a real concern. Mitigated by:
  - Each job is small (single library, small test suite).
  - Skip the matrix on docs-only PRs (`.github/workflows/docs.yml` already does this).
  - Aggressive use of `actions/cache` once the build lands.
- Floor versions are not the absolute lowest possible — users on truly ancient toolchains (GCC 9, MSVC 19.20) are excluded. Acceptable: those toolchains predate widespread C++17 stability.

## References

- ISO/IEC 14882:2017 — *Programming languages — C++* (the C++17 standard).
- ISO/IEC 9899:1990 — *Programming languages — C* (ANSI C / C89).
- ISO/IEC 9899:1999 — *Programming languages — C* (C99).
- Compiler C++17 support matrices: <https://en.cppreference.com/w/cpp/compiler_support/17>.
- spec §3.3 — [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md).
- [ADR-0004](0004-versioning-and-release-policy.md) §4 — release artefacts target the same Tier-1 platforms.
