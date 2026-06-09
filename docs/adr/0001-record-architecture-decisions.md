# ADR-0001: Record architecture decisions

- **Status:** Accepted
- **Date:** 2026-06-09
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §6.2, [`docs/adr/README.md`](README.md)

## Context

`pbr-cpp-memory-pool` is a reference implementation in the Purpose-Built References (PBR) series. Its didactic value depends on being able to explain *why* the code looks the way it does — alignment guarantees, thread-safety policy, free-list layout, dynamic-growth strategy, and similar choices all admit several reasonable answers, and the wrong reader assumption can lead to misuse downstream.

The repository is also worked on by multiple AI coding agents (Claude Code, Gemini Antigravity, ChatGPT Codex) in addition to its human maintainer. Each new agent session starts from a cold context: without a durable record of past decisions, the same trade-offs would be relitigated on every PR, and silent drift between spec and implementation becomes likely.

We need a lightweight, version-controlled mechanism that captures decisions at the moment they are made, survives team and tooling changes, and lives alongside the code so it is reviewed under the same rules as the implementation.

## Decision

We adopt **Architecture Decision Records (ADRs)** in the [Michael Nygard format](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions), stored under `docs/adr/` as Markdown files numbered sequentially (`NNNN-short-kebab-title.md`). The lifecycle is `Proposed → Accepted → (Superseded | Deprecated)`. Accepted ADRs are immutable except for the `Status` line. Every ADR is added or superseded through the same pull-request flow as code changes, and the index in [`docs/adr/README.md`](README.md) is updated in the same PR.

## Alternatives Considered

- **No ADRs, rely on commit messages and PR descriptions.** Rejected because git history is hard to navigate by topic, and decisions tend to fragment across many commits. Commit bodies are good for the *what*; ADRs are needed for the *why* at a coarser granularity.
- **Wiki-hosted decisions (GitHub Wiki / external).** Rejected because external systems decouple decisions from the code they govern, are not visible to AI agents reading the repo, and drift the moment the wiki host changes.
- **Heavier formats (RFC, MADR with full template).** Rejected for this project's scale — Nygard's four sections (Context / Decision / Alternatives / Consequences) give enough rigor without ceremony tax on a single-maintainer reference repo.

## Consequences

**Positive**

- AI agents joining a new session have a canonical place to learn the *why* behind the code.
- Decisions are auditable: rejected alternatives are recorded alongside the accepted one.
- Spec/implementation drift becomes visible — any divergence must either update the spec or land as a new ADR.
- The same review process (PR + maintainer approval) governs architectural decisions and code.

**Negative**

- Every non-trivial design choice carries a small documentation tax. Mitigated by the explicit "do not write ADRs for routine details" guidance in [`docs/adr/README.md`](README.md).
- The ADR index must be maintained by hand. Mitigated by including the index update in the same PR as the ADR itself, enforced by the PR template's *Documentation Impact* checklist.

## References

- Michael Nygard, *Documenting Architecture Decisions* (2011).
- [`AGENTS.md`](../../AGENTS.md) §6.2 — when to open an ADR.
- [`docs/adr/template.md`](template.md) — file skeleton used for every new ADR.
