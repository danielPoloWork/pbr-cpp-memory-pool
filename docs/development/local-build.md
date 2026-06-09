# Local Build Guide

End-to-end terminal workflow for building, testing, and running `pbr-cpp-memory-pool` from a freshly cloned repository. The toolchain and language standards are fixed by [ADR-0005](../adr/0005-toolchain-matrix-and-supported-platforms.md); this guide turns that policy into reproducible commands per platform.

> **Audience.** Engineers who prefer the terminal over an IDE, or who need to reproduce a CI failure on their workstation. IDE-driven workflows (CLion, Visual Studio, VS Code) sit on top of the same CMake project and presets — they call the same commands under the hood.

## 1. Prerequisites

| Tool        | Minimum version | Why                                                              |
|-------------|-----------------|------------------------------------------------------------------|
| Git         | 2.30+           | Cloning, branch-protected workflow (see [git-workflow.md](../workflow/git-workflow.md)). |
| CMake       | 3.25            | Required by our `CMakePresets.json` schema v6.                   |
| Ninja       | 1.10+           | The generator used by every preset in this project.              |
| C++17 toolchain | One of: GCC ≥ 11, Clang ≥ 14, MSVC ≥ 19.30 (VS 2022 17.0), Apple Clang ≥ 14 (Xcode 14) | Floor versions from ADR-0005 §1. |

Optional but recommended for the full quality-bar workflow (matches the CI gates documented in [`AGENTS.md`](../../AGENTS.md) §10):

| Tool        | Used for                                                          |
|-------------|-------------------------------------------------------------------|
| `clang-tidy` | Static analysis — clean diff required on every PR.               |
| `clang-format` | Source formatting — run before opening a PR.                  |
| Valgrind     | Leak-check demonstrative test (POSIX only, spec §6.2).           |
| ASan / UBSan / TSan runtimes | Bundled with GCC and Clang; no separate install. |

## 2. Installing the toolchain

### 2.1 Linux

#### Debian / Ubuntu (22.04+ / Debian 12+)

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    clang-tidy \
    clang-format \
    valgrind
```

If your distribution ships a CMake older than 3.25 (e.g. Ubuntu 20.04), fetch the Kitware APT repository following the instructions at <https://apt.kitware.com> — never rely on an out-of-tree backport for a project file that depends on the schema version.

#### Fedora / RHEL 9+

```bash
sudo dnf install -y \
    gcc gcc-c++ \
    cmake \
    ninja-build \
    clang clang-tools-extra \
    valgrind
```

#### Arch / Manjaro

```bash
sudo pacman -S --needed \
    base-devel cmake ninja clang valgrind
```

### 2.2 macOS

The Apple Clang shipped with Xcode 14 or later satisfies the floor; `cmake` and `ninja` come from Homebrew. Valgrind is no longer maintained on macOS — use ASan / UBSan instead, which are bundled with Apple Clang.

```bash
xcode-select --install                # if Apple's Command Line Tools are not yet present
brew install cmake ninja llvm
```

`llvm` from Homebrew is optional; it provides a parallel `clang-tidy` / `clang-format` when the Apple-bundled versions lag behind the LLVM mainline.

### 2.3 Windows — MSVC path (Tier-1)

The canonical Windows compiler under ADR-0005 §1 is MSVC. The simplest setup:

1. **Install Visual Studio Build Tools 2022 or later** from <https://visualstudio.microsoft.com/downloads/> → "Tools for Visual Studio" → "Build Tools for Visual Studio". In the installer, select the workload **"Desktop development with C++"** and ensure a **Windows 10 or Windows 11 SDK** is checked (do not pick the Windows 8.1 SDK — it is deprecated).
2. **Install CMake and Ninja standalone** so they are reachable from any shell:

   ```powershell
   winget install --id Kitware.CMake -e
   winget install --id Ninja-build.Ninja -e
   ```

   `winget` is bundled with modern Windows 10/11. If your machine is locked down, `scoop install cmake ninja` is an unprivileged alternative.

3. **Refresh `PATH`** and verify the new tools are reachable.

   `winget` writes the installer's `PATH` additions into the Windows registry, but **an already-open PowerShell session does not pick them up** — `cmake` / `ninja` will fail with *"Termine ... non riconosciuto"* / *"is not recognized"*. Two fixes, in increasing order of inconvenience:

   - **Reload `PATH` in the current session** (no restart needed):

     ```powershell
     $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
     ```

   - **Close and reopen the terminal.** Every new PowerShell / `cmd.exe` / Developer PowerShell session inherits the updated `PATH` automatically.

   Either way, verify before continuing:

   ```powershell
   cmake --version    # expect 3.25 or newer
   ninja --version    # any recent ninja
   ```

#### Loading the MSVC environment

`cl.exe` is **not** in `PATH` from a plain PowerShell or `cmd.exe`. You must use a shell that has loaded the Visual Studio environment variables. Two equivalent paths:

- **Recommended.** From the Start menu, open **"Developer PowerShell for VS 2022"** (or the equivalent entry for your installed Visual Studio version). The prompt should announce the loaded environment.
- **From a plain PowerShell**, import the environment manually:

  ```powershell
  & "C:\Program Files (x86)\Microsoft Visual Studio\<version>\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
  ```

  Replace `<version>` with your installed major (e.g., `17` for VS 2022, `18` for the next major). After this call, `cl.exe`, `link.exe`, and the rest of the MSVC toolchain are on `PATH` for the lifetime of the shell session.

Verify the environment with `cl /?` — the help text confirms the compiler is reachable.

### 2.4 Windows — MinGW-w64 path (Tier-2)

For environments where MSVC cannot be installed. ADR-0005 §2 documents MinGW-w64 as best-effort; the project builds cleanly under it but it is not gated in CI.

```powershell
# MSYS2 is the canonical MinGW-w64 distribution.
winget install --id MSYS2.MSYS2 -e
```

After install, open an **"MSYS2 MinGW 64-bit"** shell and:

```bash
pacman -Syu          # refresh package db, may require restart
pacman -S --needed mingw-w64-x86_64-{gcc,cmake,ninja,clang}
```

The MSYS2 MinGW64 shell already has the right `PATH`; from a regular Windows shell, prepend `C:\msys64\mingw64\bin` to `PATH`.

## 3. Clone and configure

From any shell with the toolchain loaded:

```bash
git clone https://github.com/danielPoloWork/pbr-cpp-memory-pool.git
cd pbr-cpp-memory-pool
cmake --preset debug
```

A successful configure ends with a project summary printed by the top-level `CMakeLists.txt`:

```text
--   pbr-cpp-memory-pool 0.1.0
--   --------------------------------------
--   C++ compiler        : <compiler-id> <version>
--   C++ standard        : C++17 (extensions OFF)
--   Build type          : Debug
--   Tests               : ON
--   Benchmarks          : OFF
--
-- Configuring done
-- Generating done
-- Build files have been written to: .../pbr-cpp-memory-pool/build/debug
```

If `cmake --preset debug` fails:

- *"Unsupported CMake version"* — your CMake is older than 3.25; install a newer one (see §1).
- *"Preset 'debug' not found"* — your CMake is older than the v6 preset schema; the floor is enforced inside `CMakePresets.json`.
- *"Could not find compiler"* on Windows — the MSVC environment was not loaded; see §2.3.

## 4. Build

```bash
cmake --build --preset debug
```

The build produces `pbr_memory_pool.lib` (MSVC) or `libpbr_memory_pool.a` (GCC / Clang / MinGW) under `build/debug/`. With the Milestone 1 stub implementations, expect 2 Ninja steps and a wall-clock time of a few seconds.

Other build presets:

| Preset    | Build type | Sanitizer        | Notes                                                                            |
|-----------|------------|------------------|----------------------------------------------------------------------------------|
| `debug`   | Debug      | none             | Default development build.                                                       |
| `release` | Release    | none             | Optimised; used for benchmark numbers.                                           |
| `asan`    | Debug      | AddressSanitizer | POSIX-only (ADR-0005 §3); hidden on Windows.                                     |
| `ubsan`   | Debug      | UBSan            | POSIX-only.                                                                      |
| `tsan`    | Debug      | ThreadSanitizer  | POSIX-only; used from Milestone 4 onwards when the thread-safe variant lands.    |

Switch presets by repeating §3 and §4 with the new name — each preset uses its own out-of-tree build directory (`build/<preset>/`), so they coexist without interference.

## 5. Run tests

```bash
ctest --preset debug
```

Tests are wired in starting at Milestone 1.7. Until then the command prints `No tests were found!!!` — that is the expected output during early Milestone 1 and confirms only that the CTest wiring exists. Once tests land, expect lines such as `Test #1: pool_smoke ... Passed`.

## 6. Verifying the full quality-bar locally

The CI gates listed in [`AGENTS.md`](../../AGENTS.md) §10 can each be reproduced from the terminal:

```bash
# Build matrix (cycle through configs)
for preset in debug release asan ubsan; do
    cmake --preset "$preset" && cmake --build --preset "$preset" && ctest --preset "$preset"
done

# clang-tidy on the diff against master (once .clang-tidy lands — Milestone 1.5)
clang-tidy -p build/debug $(git diff --name-only --diff-filter=AM master...HEAD -- '*.cpp' '*.hpp' '*.h')

# clang-format check (once .clang-format lands — Milestone 1.4)
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp' '*.h')

# Valgrind on the demonstrative test (once the test lands — Milestone 2.8)
valgrind --leak-check=full --show-leak-kinds=all ./build/debug/...
```

Run all of these before opening a PR. CI runs the same things, but local pre-flight is cheaper and faster than a CI round-trip.

## 7. Troubleshooting

| Symptom                                                          | Most likely cause                                                                 | Fix                                                                                                  |
|------------------------------------------------------------------|-----------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------|
| `cmake: command not found`                                       | CMake not in `PATH`.                                                              | Install per §2 and restart the shell.                                                                |
| `cmake` / `ninja` not recognised on Windows immediately after `winget install`. | `winget` updated `PATH` in the registry but the open shell did not reload it. | Either run `$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")` in the current session, or close and reopen the terminal. See §2.3 step 3. |
| `cl is not recognized` on Windows.                               | MSVC environment not loaded.                                                      | Open Developer PowerShell for VS, or run `Launch-VsDevShell.ps1` (see §2.3).                         |
| `CMake Error: Could not find generator 'Ninja'`.                 | Ninja not in `PATH`.                                                              | Install per §2.                                                                                      |
| Preset is "hidden" or "skipped" with reason `condition`.         | A sanitizer preset on Windows.                                                    | Expected behaviour — sanitizer presets are POSIX-only (ADR-0005 §3). Use `debug` or `release` instead. |
| `error C2440` / `error C2664` only on MSVC.                      | Code uses a GCC/Clang extension MSVC rejects.                                     | Fix the code; `/permissive-` is intentional. Do not relax warnings.                                  |
| `undefined reference to memory_pool_*` when linking a consumer.  | Building against the M1 stubs from before M2; or `pbr_memory_pool` not linked.    | Either link `pbr::memory_pool` in your consumer target, or wait for the M2 real implementation.      |
| Re-running configure picks up stale cache.                       | The build directory has stale CMake state.                                        | Delete the per-preset build directory (`rm -rf build/debug`) and re-run `cmake --preset debug`.      |

## 8. Quick reference — one-liner per platform

```bash
# Linux / macOS / MinGW / WSL — any POSIX shell
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

```powershell
# Windows MSVC, Developer PowerShell for VS — PowerShell 5.1 has no &&
cmake --preset debug; if ($?) { cmake --build --preset debug; if ($?) { ctest --preset debug } }
```

## 9. References

- [ADR-0005](../adr/0005-toolchain-matrix-and-supported-platforms.md) — toolchain matrix and supported platforms.
- [`AGENTS.md`](../../AGENTS.md) §9 / §10 — coding conventions and the enterprise quality bar.
- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §3.3 — the standards-compatibility contract this guide implements.
- [`CMakeLists.txt`](../../CMakeLists.txt) — the project file these commands drive.
- [`CMakePresets.json`](../../CMakePresets.json) — the preset definitions referenced by `--preset`.
- Upstream documentation:
  - CMake — <https://cmake.org/cmake/help/latest/>
  - CMake Presets — <https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html>
  - Visual Studio command-line build — <https://learn.microsoft.com/cpp/build/building-on-the-command-line>
  - MSYS2 / MinGW-w64 — <https://www.msys2.org>
