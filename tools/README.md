# `tools/`

Project tooling. **Dependency-free** (Python 3 standard library only — no `pip` installs), consistent with the repository's zero-external-dependency posture.

## `consistency_lint.py`

The **agent-runnable consistency lint** ([ADR-0035](../docs/adr/0035-agent-runnable-consistency-lint.md), ROADMAP §8.6). It asserts the project is internally **congruent** after any change and exits non-zero with an actionable report otherwise. Run it from anywhere:

```bash
python tools/consistency_lint.py
```

It is also a CI gate — the `consistency` job in [`.github/workflows/docs.yml`](../.github/workflows/docs.yml) runs it on every relevant PR (with a full-history checkout, which the i18n freshness check needs).

Checks (the "post-release congruence contract"):

1. **version lockstep** — `version.hpp` ⇆ the latest dated `CHANGELOG` block ⇆ the README status badge ⇆ the newest `docs/releases/vX.Y.Z.md` all agree.
2. **ADR index** — every `docs/adr/NNNN-*.md` is in the index and vice-versa; sequential numbering, no gaps.
3. **patterns** — every Adopted pattern row cites an existing ADR **and** an existing `src/main/cpp/` code location.
4. **spec map** — the Spec Coverage Map has no dangling row (valid status glyph + non-empty roadmap-items cell).
5. **i18n freshness** — no `translated` manifest entry is staler than its English source (the recorded source commit is the source file's latest commit).
6. **milestones** — the README milestone table and the ROADMAP checkboxes agree on completion; no malformed ROADMAP checkbox.

The pre-PR contributor/agent checklist that calls this lint is in [`AGENTS.md`](../AGENTS.md) (ROADMAP §8.7).
