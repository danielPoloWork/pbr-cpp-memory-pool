# ADR-0028: Install and packaging layout — Phase 1 distribution

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0004](0004-versioning-and-release-policy.md) §5 (the distribution phasing this ADR implements Phase 1 of), [ADR-0002](0002-adopt-cross-language-source-layout.md) (the `it/d4np/memorypool/` include tree the install preserves), [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) (the Tier-1 platforms the release artifacts target), [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) §2 (the LOCKFREE `atomic` link the exported target may carry), [ROADMAP](../../ROADMAP.md) §7.4 (the item), [`.github/workflows/release.yml`](../../.github/workflows/release.yml) (the release packaging that consumes these rules).

## Context

The library has been buildable and vendorable since Milestone 1, and its CMake target already declared `target_include_directories(... $<INSTALL_INTERFACE:include>)` in anticipation of an install. But there were **no `install()` rules**: a consumer could only use the pool by adding the source tree via `add_subdirectory` / `FetchContent` and linking the in-build alias `pbr::memory_pool`. There was no way to install the library to a prefix and consume it with `find_package`, and no pkg-config for non-CMake build systems.

[ADR-0004](0004-versioning-and-release-policy.md) §5 fixes the distribution roadmap in two phases: **Phase 1 (this item, M7.4)** is CMake `find_package` support — *"consumers can `find_package(pbr_memory_pool CONFIG REQUIRED)` after vendoring via `FetchContent` or installing the artifacts from a GitHub Release"* — plus the public-header export and a pkg-config file named in ROADMAP §7.4; **Phase 2 (post-1.0)** is the vcpkg port and Conan recipe, deferred behind a stable 1.0 API.

A latent gap reinforced the need: the release workflow ([`release.yml`](../../.github/workflows/release.yml)) hand-copied only **three of the seven** public headers (`memory_pool.h`, `memory_pool.hpp`, `version.hpp`) into its tarball — `typed_pool.hpp`, `pool_allocator.hpp`, `instrumented_pool.hpp`, and `free_list_iterator.hpp` were silently missing — and shipped neither a CMake config nor a `.pc`, so the ADR-0004 §5 "install the artifacts from a Release" claim was not actually true.

## Decision

We add standard CMake **install + export** rules and a **pkg-config** file, gated behind a `PBR_MEMORY_POOL_INSTALL` option, and switch the release packaging to drive them via `cmake --install`. A consumer uses the library identically whether vendored or installed:

```cmake
find_package(pbr_memory_pool CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

Six sub-decisions make the layout precise:

### 1. GNUInstallDirs layout

Install destinations come from `GNUInstallDirs` — the archive to `${CMAKE_INSTALL_LIBDIR}`, the public-header tree to `${CMAKE_INSTALL_INCLUDEDIR}`, the package config to `${CMAKE_INSTALL_LIBDIR}/cmake/pbr_memory_pool`, the `.pc` to `${CMAKE_INSTALL_LIBDIR}/pkgconfig`, and `LICENSE` to `${CMAKE_INSTALL_DOCDIR}`. This is the convention every distro packager, Homebrew formula, and downstream build expects, so the artifacts drop into a standard prefix with no surprises.

### 2. One exported target name: `pbr::memory_pool`

The internal CMake target is `pbr_memory_pool`, exported under `NAMESPACE pbr::`. Left alone that would yield `pbr::pbr_memory_pool` — *different* from the in-build `ALIAS pbr::memory_pool` that `add_subdirectory` / `FetchContent` consumers already use. We set `EXPORT_NAME memory_pool` on the target so the **installed** imported target is `pbr::memory_pool` too. One link line works for every consumption mode; switching from vendoring to an installed package requires no edit. (Verified: the installed `pbr_memory_poolTargets.cmake` declares `add_library(pbr::memory_pool STATIC IMPORTED)`.)

### 3. Relocatable CMake package config, `SameMajorVersion` compatibility

The config is generated with `configure_package_config_file` (so `PACKAGE_PREFIX_DIR` is computed relative to the installed config file — the package is **relocatable**, works wherever it is extracted). The version file uses `write_basic_package_version_file(... COMPATIBILITY SameMajorVersion)`: under SemVer (ADR-0004) any `1.x` satisfies `find_package(pbr_memory_pool 1.0)`, and a future `2.0` does not — the version-file contract mirrors the API-stability contract.

### 4. Header tree preserved

`install(DIRECTORY src/main/cpp/it ... FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")` installs the whole `it/d4np/memorypool/` subtree under `include/`, so the canonical `#include <it/d4np/memorypool/memory_pool.h>` form resolves against the install tree exactly as it does in-build. All seven public headers ship — no hand-maintained subset to fall out of date.

### 5. `PBR_MEMORY_POOL_INSTALL` defaults to `PROJECT_IS_TOP_LEVEL`

When the project is the top-level build, install rules are ON. When it is pulled in via `add_subdirectory` / `FetchContent`, they default OFF, so the embedding parent's own `cmake --install` is not polluted with our config/headers. A consumer who *does* want the embedded copy installed can force the option ON.

### 6. pkg-config for non-CMake consumers

A `pbr-memory-pool.pc` is generated from a template for autotools / Make / Meson consumers. **Known limitation:** `configure_file` bakes `CMAKE_INSTALL_PREFIX` at configure time, so the `.pc`'s `prefix=` reflects the configure-time prefix, not a later `cmake --install --prefix` or the extract location of a relocated tarball. The relocatable CMake config (§3) is the primary, fully-relocatable mechanism; the `.pc` is best-effort and correct when the package is configured with its final prefix or extracted to that prefix. This is an inherent pkg-config trait, not specific to this project.

The release workflow's `build-artifacts` job now runs `cmake --install` into its staging tree instead of hand-copying, so every release tarball is a complete `find_package`-ready install (full headers + archive + config + `.pc`), with top-level `LICENSE` / `README` / `CHANGELOG` added for distribution convenience.

## Alternatives Considered

- **No export — keep vendoring-only (`add_subdirectory` / `FetchContent`).** Rejected: ADR-0004 §5 Phase 1 explicitly requires `find_package` support and installable Release artifacts; vendoring-only leaves non-CMake consumers and package managers unserved.
- **Export the default `pbr::pbr_memory_pool` name (no `EXPORT_NAME`).** Rejected: it diverges from the in-build `pbr::memory_pool` alias, so a consumer migrating between vendoring and an installed package would have to edit their `target_link_libraries`. One name for both modes is worth the one-line `EXPORT_NAME`.
- **`COMPATIBILITY ExactVersion` or `AnyNewerVersion`.** Rejected: `ExactVersion` would force consumers to pin a patch release; `AnyNewerVersion` would falsely claim a future `2.0` satisfies a `1.x` request. `SameMajorVersion` is the SemVer-correct choice.
- **Skip pkg-config — CMake config only.** Rejected: ROADMAP §7.4 names pkg-config explicitly, and Make / autotools / Meson consumers rely on it. Its prefix limitation (§6) is documented rather than used as a reason to omit it.
- **Make the `.pc` fully relocatable via a wrapper / `$ORIGIN`-style logic.** Rejected for now: pkg-config has no portable relocation primitive; the relocatable CMake config already covers the relocate-anywhere case, so the extra machinery is not worth its complexity at Phase 1.
- **Adopt CPack / generate distro packages (`.deb`, `.rpm`) now.** Rejected/deferred: out of scope for Phase 1; `release.yml` already ships per-platform `tar.gz`, and CPack/registry packaging is the Phase 2 (vcpkg/Conan) and beyond concern.
- **Install a shared library too (`BUILD_SHARED_LIBS`).** Out of scope: the library is STATIC by ADR-0004 §4 until a shared-library milestone; the install rules already handle `LIBRARY` / `RUNTIME` destinations so they need no change when that lands.

## Consequences

**Positive**

- `find_package(pbr_memory_pool CONFIG REQUIRED)` + `target_link_libraries(... pbr::memory_pool)` now works against an installed package — Phase 1 of ADR-0004 §5 is delivered and was verified end-to-end (install to a staging prefix, then a separate consumer project configured, built, and ran against it).
- The release tarballs become complete, `find_package`-ready install trees, **fixing the latent bug** where four of the seven public headers and the package config were missing.
- One target name (`pbr::memory_pool`), one include form, one compatibility rule across vendoring, installed packages, and Release tarballs.
- A good citizen when embedded: `PROJECT_IS_TOP_LEVEL`-gated install rules do not leak into a parent build.

**Negative / costs**

- The pkg-config `.pc` carries the configure-time prefix (§6) — a documented best-effort artifact; pkg-config consumers of a relocated tarball must adjust `prefix=` or prefer the relocatable CMake config.
- Two new template files (`cmake/*.in`) and an install block to maintain; the `EXPORT_NAME` indirection is a small subtlety a future contributor must understand (documented inline).
- The `release.yml` packaging change cannot be exercised until the next tag push (M7.7); the `cmake --install` path was validated locally instead, and the local install produced exactly the expected tree (lib, full headers, config + targets + version, `.pc`, LICENSE).

**Documentation updates landing in the same PR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0028.
- [ROADMAP](../../ROADMAP.md) §7.4 — checkbox flipped.
- [`README.md`](../../README.md) — an "Install / consume" snippet (find_package + pkg-config).
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` (install/export, pkg-config) + `Changed` (release packaging) entries.

## References

- [ADR-0004](0004-versioning-and-release-policy.md) §5 — the distribution phasing.
- [ROADMAP](../../ROADMAP.md) §7.4 — the item implemented here.
- [`CMakeLists.txt`](../../CMakeLists.txt), [`cmake/pbr_memory_poolConfig.cmake.in`](../../cmake/pbr_memory_poolConfig.cmake.in), [`cmake/pbr-memory-pool.pc.in`](../../cmake/pbr-memory-pool.pc.in) — the artifacts this ADR governs.
- CMake `install(EXPORT)` / `configure_package_config_file` / `write_basic_package_version_file` — [`https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html`](https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html).
- GNUInstallDirs — [`https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html`](https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html).
