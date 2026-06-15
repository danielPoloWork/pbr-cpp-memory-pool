# Bug ledger

The durable, in-repo record of **known defects** and the **triage** of incoming
reports for `pbr-cpp-memory-pool`. One Markdown file per defect, reviewed like any
other artifact ([ADR-0039](../adr/0039-bug-ledger-and-triage-protocol.md)).

This ledger is the **source of truth** for defects. A GitHub issue, if one exists, is
*referenced* from a record but is not authoritative — the investigation travels with
the repo, offline, like the ADRs and the [session journal](../journal/).

The ledger holds the **open / in-flight / triaged** side of a defect's life; the
**closing** side — *what shipped in which release* — is recorded in the `CHANGELOG`
`Fixed` category ([ADR-0038](../adr/0038-changelog-version-split.md)) and governed by
[`docs/workflow/maintenance.md`](../workflow/maintenance.md). A `fixed` record and its
`CHANGELOG` line cross-reference each other.

## Format

File naming: `BUG-NNNN-<short-kebab-slug>.md`, stored under a **discovery-date** tree:

```
docs/bugs/<YYYY>/<MM>/BUG-NNNN-<slug>.md
```

- `NNNN` is a **zero-padded, globally monotonic** id — never reused, never renumbered
  (like an ADR number). It is the short stable handle (`BUG-0007`) used to reference a
  defect from a commit, a PR, or a `CHANGELOG` line.
- The `<YYYY>/<MM>` folders are the **discovery** year and month, keeping any one
  directory small — the same idiom as the [session journal](../journal/).

Each record carries structured frontmatter:

| Key | Meaning | Vocabulary |
|-----|---------|------------|
| `id` | the `BUG-NNNN` handle — matches the filename | — |
| `title` | one-line description | — |
| `status` | lifecycle state | `open` · `confirmed` · `fixed` · `wontfix` · `duplicate` · `cannot-reproduce` |
| `severity` | impact level | `low` · `medium` · `high` · `critical` |
| `reporter` | who raised it | `internal` · `third-party` |
| `discovered` | discovery date — matches the `<YYYY>/<MM>` path | `YYYY-MM-DD` |
| `affected-versions` | version range affected | e.g. `">=1.0.0,<1.1.1"` |
| `fixed-in` | release that fixes it (required once `status: fixed`) | e.g. `v1.1.1` |

Start from [`template.md`](template.md).

## Lifecycle

```
open ─► confirmed ─► fixed
  │         │
  └─────────┴─► wontfix | duplicate | cannot-reproduce   (terminal)
```

- **open** — recorded, not yet root-caused/confirmed.
- **confirmed** — reproduced and root-caused; awaiting a fix.
- **fixed** — a release fixes it; `fixed-in` is set and the `CHANGELOG` `Fixed` line links back here.
- **wontfix / duplicate / cannot-reproduce** — terminal; the record documents *why* (a `duplicate` links the canonical `BUG-NNNN`).

## How a record is created

The agent's triage protocol is codified in [`AGENTS.md`](../../AGENTS.md) §7.7. In short:

- **Hunting for bugs** → a record is created only for a **verified, reproducible** defect.
- **A third-party report** → the agent **reproduces and root-causes first**; only then a
  `confirmed` record (`reporter: third-party`, repro as evidence). A report that does
  **not** hold up is still recorded — as `cannot-reproduce` / `rejected` / `duplicate` —
  with the investigation that reached that verdict.

The fix lands through the normal hotfix/PATCH flow ([`docs/workflow/maintenance.md`](../workflow/maintenance.md)),
which flips the record to `fixed`, sets `fixed-in`, and adds the `CHANGELOG` `Fixed` line —
all in the same PR. Integrity (frontmatter, ids, index bijection, date agreement) is
checked by `python tools/consistency_lint.py` (the `bugs` check).

## Index

Newest first, grouped by year and month.

### 2026 — June

| Id | Title | Status | Severity | Discovered |
|----|-------|--------|----------|------------|
| [BUG-0001](2026/06/BUG-0001-instrumented-pool-growth-counter-data-race.md) | Data race on `InstrumentedPool::last_growths_` under concurrent use | `fixed` | high | 2026-06-15 |
| [BUG-0002](2026/06/BUG-0002-instrumented-pool-live-counter-underflow.md) | `InstrumentedPool::deallocate` underflows `live_` on a foreign / double-freed pointer | `fixed` | medium | 2026-06-15 |
| [BUG-0003](2026/06/BUG-0003-instrumented-pool-move-assign-missing-destroyed-event.md) | `InstrumentedPool` move-assignment does not notify `destroyed` for the replaced pool | `fixed` | low | 2026-06-15 |
| [BUG-0004](2026/06/BUG-0004-grow-pool-growth-size-overflow.md) | Unguarded `size_t` overflow in `grow_pool` growth-size computation | `fixed` | low | 2026-06-15 |
