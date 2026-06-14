# ADR-0031: Conan recipe — Phase 2 distribution (in-repo, pinned to v1.0.0)

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0004](0004-versioning-and-release-policy.md) §5 (distribution phasing — this is Phase 2), [ADR-0028](0028-install-and-packaging-layout.md) (the install/export rules the recipe builds on), [ADR-0030](0030-vcpkg-port.md) (the sibling vcpkg port, same shape), [ROADMAP](../../ROADMAP.md) §7.9 (the item), [`conan/`](../../conan/) (the artifacts this ADR governs).

## Context

ADR-0004 §5 Phase 2 is package-registry distribution; ROADMAP §7.8 delivered the vcpkg half ([ADR-0030](0030-vcpkg-port.md)) and §7.9 is the Conan half. The same reasoning applies: "publish to ConanCenter" is a pull request against [conan-io/conan-center-index](https://github.com/conan-io/conan-center-index), reviewed and merged on the index maintainers' cadence, with a per-release follow-up obligation. And the same favourable precondition holds — the library already installs a complete `find_package` package (ADR-0028), so a build-from-source recipe is a thin wrapper.

This ADR deliberately mirrors ADR-0030 so the two Phase-2 paths stay symmetric and easy to maintain together.

## Decision

We ship a **Conan 2.x recipe in this repository, under [`conan/`](../../conan/)** (`conanfile.py` + a `test_package/`), pinned to the `v1.0.0` source tag by **SHA256**. The recipe **builds from source and delegates the build/install to the project's CMake rules** (ADR-0028). Because Conan generates its own consumer-side config via `CMakeDeps`, the recipe **drops the upstream-bundled CMake config and pkg-config** from the package and re-exposes the target through `package_info` — `cmake_file_name = pbr_memory_pool`, `cmake_target_name = pbr::memory_pool` — so a Conan consumer writes the identical `find_package(pbr_memory_pool CONFIG REQUIRED)` + `pbr::memory_pool`. **Registry publication (ConanCenter or self-hosted) is deferred**; the recipe is written to ConanCenter conventions and is creatable locally today (`conan create conan/`).

Sub-decisions:

1. **Build from source, reuse ADR-0028.** `CMakeToolchain` + `CMake` configure (tests/benchmarks off) → build → `cmake.install()`. No build/install logic is duplicated.
2. **Conan owns the consumer config; drop the bundled one.** After `cmake.install()` the recipe removes `lib/cmake`, `lib/pkgconfig`, and `share` from the package, so there is exactly one config a Conan consumer sees — Conan's generated one — avoiding two competing `pbr_memory_poolConfig.cmake` on the consumer's `CMAKE_PREFIX_PATH`. The target name is preserved via `package_info`.
3. **Version single-sourced; SHA256-pinned.** The source URL is derived from `version`, so a bump touches only `version` + the `sha256` (Conan's `get()` verifies SHA256, vs vcpkg's SHA512 — the only material difference from ADR-0030).
4. **`test_package` is part of the deliverable.** A minimal `find_package` + link + run consumer that `conan create` builds, so the recipe is self-verifying once Conan is available — and the package's `static-library` / `fPIC` / `cppstd>=17` contract is declared (`package_type`, `options`, `check_min_cppstd`).

## Alternatives Considered

- **Publish to ConanCenter now instead of an in-repo recipe.** Rejected for this milestone, same reasoning as ADR-0030: gated on external review, not unilaterally completable, and a per-release obligation taken on at the worst moment. The in-repo recipe is creatable/consumable today; the index PR is a documented follow-up.
- **Keep the upstream-bundled CMake config in the Conan package (don't drop `lib/cmake`).** Rejected: it would leave two `pbr_memory_poolConfig.cmake` visible to a CMakeDeps consumer (the bundled one and Conan's generated one) — a confusing, order-dependent setup. Conan's idiom is for the recipe to own the consumer config via `package_info`.
- **Header-describe the package manually (skip `cmake.install()`).** Rejected: duplicates ADR-0028's layout and drifts from it; building through the project's install keeps one source of truth (same call as ADR-0030).
- **Conan 1.x recipe (or a 1.x/2.x dual recipe).** Rejected: Conan 2.x is the current line and ConanCenter's baseline; a 1.x recipe would be legacy maintenance for shrinking benefit.
- **Pin to a branch / unhashed source.** Rejected: non-reproducible; a recipe must pin an immutable, hashed source (`sha256`).

## Consequences

**Positive**

- A working Conan consumption path **today** (`conan create conan/`), with the same `pbr::memory_pool` target as `find_package` / `FetchContent` / vcpkg.
- Symmetric with the vcpkg port (ADR-0030): both build from source through ADR-0028, both pin `v1.0.0`, both defer registry publication — one mental model, two managers.
- ConanCenter-submission-ready (recipe + `test_package`); promotion is a mechanical follow-up.
- Reproducible (SHA256-pinned tag); version single-sourced.

**Negative / limitations**

- The recipe was **not built through a live `conan create` locally** (Conan is not provisioned on the maintainer's box); its Python syntax is validated and the source-tarball SHA256 was verified by download, but the first real build will happen on a consumer's machine or in a future CI job. A Conan-CI smoke job is a candidate follow-up (with the M8 governance work, alongside the vcpkg one).
- Until publication, consumers create the package from this recipe rather than pulling it from a remote.
- Per-release maintenance: bump `version` + `sha256` (documented in [`conan/README.md`](../../conan/README.md)); plus a ConanCenter PR once the recipe is upstreamed.

**Documentation updates landing in the same PR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0031.
- [ROADMAP](../../ROADMAP.md) §7.9 — checkbox flipped (closes Milestone 7's stretch items).
- [`README.md`](../../README.md) — a Conan consumption note alongside vcpkg.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry.

## References

- [ADR-0004](0004-versioning-and-release-policy.md) §5; [ADR-0028](0028-install-and-packaging-layout.md); [ADR-0030](0030-vcpkg-port.md) — the sibling vcpkg decision.
- [`conan/`](../../conan/) — `conanfile.py`, `test_package/`, and the usage / publication guide.
- Conan 2.x docs — [`https://docs.conan.io/2/`](https://docs.conan.io/2/); `CMakeToolchain`, `CMakeDeps`, `cmake_layout`, `package_info` properties.
