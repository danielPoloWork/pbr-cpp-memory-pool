# 2026-06-14 — Packaging patch release (v1.0.1)

> Session checkpoint — part of the [session journal](../../README.md). Checkpoints live outside `ROADMAP.md` per [ADR-0036](../../../adr/0036-session-journal-extraction.md).

- A maintenance **`PATCH`** cut by the maintainer's request, immediately after M7.9 merged: it bundles the M7.8 vcpkg port + M7.9 Conan recipe (the post-1.0 packaging additions) into a tagged release. **Chosen as `1.0.1`, not `1.1.0`,** because the shipped library is byte-identical to `v1.0.0` (only repository-side packaging metadata was added) and to keep the Milestone 8 close targeted at `v1.1.0`. `version.hpp` → `1.0.1`, `pool_smoke` asserts updated, `CHANGELOG.md` `[Unreleased]` rolled into `## [1.0.1] — 2026-06-14`, `docs/releases/v1.0.1.md` added, README badge/status refreshed. Not a numbered roadmap item — an ad-hoc patch release. After merge: agent tags `v1.0.1`, maintainer publishes. Next: Milestone 8 (i18n & governance → `v1.1.0`).
