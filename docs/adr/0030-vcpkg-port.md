# ADR-0030: vcpkg port — Phase 2 distribution (overlay, pinned to v1.0.0)

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0004](0004-versioning-and-release-policy.md) §5 (distribution phasing — this is Phase 2), [ADR-0028](0028-install-and-packaging-layout.md) (the Phase 1 install/export rules the port builds on), [ROADMAP](../../ROADMAP.md) §7.8 (the item) and §7.9 (the sibling Conan recipe), [`ports/`](../../ports/) (the artifacts this ADR governs).

## Context

[ADR-0004](0004-versioning-and-release-policy.md) §5 splits distribution into two phases: **Phase 1** (`find_package` + pkg-config, delivered in M7.4 / [ADR-0028](0028-install-and-packaging-layout.md)) and **Phase 2** — package-registry distribution (vcpkg, Conan), gated behind a stable 1.0 API because registries expect stability and frequent pre-1.0 breakage would burn consumer trust. With `v1.0.0` tagged and the API frozen, Phase 2 is unblocked; ROADMAP §7.8 is the vcpkg half.

The literal roadmap wording is "register `pbr-memory-pool` in microsoft/vcpkg". A direct upstream registration, though, is not a thing this repo can unilaterally *do*: it is a pull request against [microsoft/vcpkg](https://github.com/microsoft/vcpkg), reviewed and merged on the registry maintainers' cadence, and it creates an **ongoing obligation** — every future release needs a follow-up upstream PR with a refreshed `versions/` baseline. The decision is therefore not only *what the port looks like* but *where it lives and who maintains the registration*.

A favourable precondition: the library already installs a complete, relocatable `find_package` package and a pkg-config `.pc` (ADR-0028). A vcpkg port that builds from source can lean entirely on those rules rather than re-describing the install.

## Decision

We ship a **vcpkg port in this repository, under [`ports/pbr-memory-pool/`](../../ports/pbr-memory-pool/)**, consumable immediately as an **overlay port** (`--overlay-ports`), pinned to the `v1.0.0` source tag by SHA512. The port **builds from source and delegates installation to the project's own CMake rules** (ADR-0028); `vcpkg_cmake_config_fixup` and `vcpkg_fixup_pkgconfig` relocate the emitted config and `.pc` into vcpkg's layout. The exported imported target is `pbr::memory_pool` — identical to every other consumption mode. **Upstream submission to microsoft/vcpkg is deferred** (the port is written to upstream conventions so it can be contributed when desired — see [`ports/README.md`](../../ports/README.md)), so this milestone delivers a working, supported vcpkg path without taking on the registry-maintenance treadmill at the moment of the 1.0 cut.

Sub-decisions:

1. **Build from source, reuse ADR-0028's install.** The portfile is a thin wrapper: `vcpkg_from_github` → `vcpkg_cmake_configure` (tests/benchmarks off) → `vcpkg_cmake_install` → fixups. No install logic is duplicated in the port; the single source of truth for the install layout stays the project's `CMakeLists.txt`.
2. **Version single-sourced via `REF "v${VERSION}"`.** `VERSION` comes from `vcpkg.json`, so a release bump touches only the manifest `version` and the `SHA512` — the tag reference derives automatically.
3. **SHA512-pinned to the immutable tag.** The hash is of `github.com/danielPoloWork/pbr-cpp-memory-pool/archive/v1.0.0.tar.gz`, so the port is reproducible and tamper-evident.
4. **Static-only, copyright canonicalised.** The library is STATIC (ADR-0004 §4); the portfile drops `debug/include`, `debug/share`, and the project's `share/doc` copy of `LICENSE`, installing the canonical vcpkg copyright via `vcpkg_install_copyright`.

## Alternatives Considered

- **Upstream-first — open the microsoft/vcpkg PR now instead of an in-repo overlay.** Rejected for this milestone: it is gated on external review, cannot be completed unilaterally, and commits the single maintainer to a per-release upstream-PR obligation at the exact moment (the 1.0 cut) when that overhead is least welcome. The overlay port delivers the *consumable* result immediately; the upstream PR is a documented, ready-to-execute follow-up.
- **No vcpkg port — rely on `find_package` / `FetchContent` only.** Rejected: ROADMAP §7.8 and ADR-0004 §5 Phase 2 explicitly call for registry distribution; vcpkg is the dominant C++ package manager and a reference implementation should model that path.
- **Re-describe the install inside the portfile (manual `file(INSTALL …)`).** Rejected: it would duplicate ADR-0028's layout and drift from it. Building from source through the project's own install rules keeps one source of truth.
- **Pin to a branch / `HEAD` instead of a tagged SHA512.** Rejected: non-reproducible and unacceptable to vcpkg's versioning model; a registry port must pin an immutable, hashed source.
- **A custom (self-hosted) vcpkg registry.** Considered, rejected for now: heavier than an overlay port for no extra reach at this stage; the overlay covers local consumption and the upstream PR covers broad reach. Revisitable if a curated multi-port PBR registry emerges.

## Consequences

**Positive**

- A working vcpkg consumption path **today** (`vcpkg install pbr-memory-pool --overlay-ports=…`), with the same `pbr::memory_pool` target as `find_package` / `FetchContent`.
- Zero install-logic duplication — the port rides on ADR-0028, so the two stay in lockstep by construction.
- The port is upstream-submission-ready; promoting it to microsoft/vcpkg is a mechanical follow-up (`x-add-version` + PR), not a rewrite.
- Reproducible and tamper-evident (SHA512-pinned tag); version single-sourced from `vcpkg.json`.

**Negative / limitations**

- The port was **not built through a live `vcpkg install` locally** (vcpkg is not provisioned on the maintainer's box); it follows the canonical helper pattern and its source-tarball SHA512 was verified by download, but the first real build will happen on a consumer's machine or in a future CI job. A vcpkg-CI smoke job is a candidate follow-up (it would belong with the M8 governance work).
- Until the upstream PR lands, consumers must pass `--overlay-ports` — slightly more friction than a registry-resident port.
- Every release must refresh `vcpkg.json` `version` + the `SHA512` (documented in `ports/README.md`); when the upstream registration exists, also an `x-add-version` PR. This per-release step is the maintenance the deferral keeps optional for now.

**Documentation updates landing in the same PR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0030.
- [ROADMAP](../../ROADMAP.md) §7.8 — checkbox flipped.
- [`README.md`](../../README.md) — a vcpkg consumption note alongside the install section.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry (first post-1.0 change).

## References

- [ADR-0004](0004-versioning-and-release-policy.md) §5 — distribution phasing.
- [ADR-0028](0028-install-and-packaging-layout.md) — the install/export rules the port builds on.
- [`ports/`](../../ports/) — `vcpkg.json`, `portfile.cmake`, and the usage / upstream-submission guide.
- vcpkg packaging docs — [`https://learn.microsoft.com/vcpkg/`](https://learn.microsoft.com/vcpkg/); `vcpkg_from_github`, `vcpkg_cmake_config_fixup`, `vcpkg_fixup_pkgconfig`, `vcpkg_install_copyright`.
