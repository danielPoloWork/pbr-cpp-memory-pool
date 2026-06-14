# ADR-0027: Doxygen HTML API site published to GitHub Pages

- **Status:** Accepted
- **Date:** 2026-06-14
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) (Doxygen for the API contract, Markdown for the narrative — this ADR closes the rendering half it deferred), [ROADMAP](../../ROADMAP.md) §7.1 (the item this ADR implements), [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 (zero-external-dependency philosophy), [ADR-0006](0006-code-style-and-static-analysis-baseline.md) §1 (`.clang-format` leaves Doxygen comments untouched), [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) §3.3 (the no-external-dependency contract whose spirit this ADR extends to the docs toolchain).

## Context

[ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) settled the documentation **format** question: Doxygen owns the per-symbol API contract in the public headers, Markdown owns the narrative. It deliberately left the **rendering pipeline** open — §4 names a Doxygen-generated API section optionally wrapped by MkDocs / Sphinx-Breathe for a unified site, but states *"the final pick is M7.1's choice and is not pre-empted here"*; §5 sketches an *expected* warn-as-error Doxyfile *"added in M7.1"*. [ROADMAP](../../ROADMAP.md) §7.1 — the first item of the v1.0 polish milestone — is exactly that work: *"Doxygen-generated API documentation published as a static site."*

So the forces are fixed and the decision space is bounded:

- The format is Doxygen (ADR-0013); only the **build, gate, and hosting** are open.
- The project is a single-maintainer reference implementation with a standing **zero-external-dependency** posture (spec §3.3, ADR-0005 §3) — applied to the build graph, and reasonably extended to the docs toolchain: a heavy Python documentation stack would be the largest dependency in the repository.
- AGENTS.md §10 already promises a *"Doxygen-compatible, builds without warnings"* public surface; M7.1 must turn that promise into a CI gate so it cannot silently rot before v1.0.
- The headers are a small set (one C header plus six C++ headers); the API surface is narrow and stable.

## Decision

We publish a **Doxygen-generated static HTML site** to **GitHub Pages**, built and deployed by the GitHub-native Pages Actions, with **no external documentation toolchain** (no MkDocs, no Sphinx, no SaaS, no Graphviz). A checked-in **partial Doxyfile** at [`docs/doxygen/Doxyfile`](../doxygen/Doxyfile) drives the build; a dedicated mainpage at [`docs/doxygen/mainpage.md`](../doxygen/mainpage.md) is the landing page and links out to the GitHub-rendered narrative. A new workflow, [`.github/workflows/docs-site.yml`](../../.github/workflows/docs-site.yml), builds the site as a **warn-as-error gate on every pull request** and **deploys it to Pages on push to `master`**. The site scope is the **API reference only**; the unified narrative-plus-API site sketched in ADR-0013 §4 is explicitly **deferred to post-v1.0**.

Four sub-decisions make the contract precise:

### 1. Hosting — GitHub Pages via the official Actions

The site is uploaded with `actions/upload-pages-artifact` and published with `actions/deploy-pages` into the `github-pages` environment. No generated HTML is committed to the repository (the output lives under the git-ignored `build/`), so git history carries no binary doc churn. Publishing is GitHub-native — the same platform that already hosts the code, CI, and releases — so there is no new account, secret, or third-party service in the trust boundary.

### 2. Scope — API reference now, unified narrative site later

For v1.0 the site renders the public-header API contract and a single hand-written mainpage. The MkDocs Material / Sphinx-Breathe wrapper that ADR-0013 §4 anticipated — folding the ADRs, spec, and workflow guides into one indexed site — is **not** built here: it would introduce a Python toolchain (the project's heaviest dependency to date) for narrative that GitHub already renders well, and it is a research-and-tune exercise that does not belong on the v1.0 critical path. The mainpage links out to the GitHub-rendered narrative, so nothing is lost; the unified site remains a clean, non-breaking future addition (a post-1.0 candidate alongside the Milestone 8 i18n work).

### 3. The gate validates documentation *correctness*, not *exhaustiveness*

This **refines** the expectation sketched in ADR-0013 §5. The Doxyfile sets `WARN_AS_ERROR = FAIL_ON_WARNINGS` (the modern form of "WARN_AS_ERROR = YES": it runs the full pass, prints every warning, then exits non-zero) together with `WARN_IF_DOC_ERROR = YES`. It deliberately leaves `WARN_IF_UNDOCUMENTED`, `WARN_IF_INCOMPLETE_DOC`, and `WARN_NO_PARAMDOC` **off**. The gate therefore fails on the **doc-rot** class — a malformed command, an unresolved cross-reference, a `@param` whose name no longer matches the signature — but not on the absence of a `@param` stanza for every comparison operator or the absence of a doc comment on every iterator trait typedef. Empirically, enabling the exhaustiveness knobs produced ~40 warnings that were almost entirely ceremonial (forced `@param`/`@return` on `operator==`, `operator++`, the five `LegacyForwardIterator` member typedefs, defaulted special members, and the internal config macros) — noise that would push contributors toward filler comments rather than better ones. "Every public symbol carries a comment" stays an AGENTS.md §9 review obligation, enforced socially; the CI gate enforces that the comments that exist are *correct and resolvable*.

### 4. Version is single-sourced from `version.hpp`

`PROJECT_NUMBER` is left blank in the checked-in Doxyfile and injected at build time from `PBR_MEMORY_POOL_VERSION_STRING` in [`version.hpp`](../../src/main/cpp/it/d4np/memorypool/version.hpp) (`( cat Doxyfile; echo "PROJECT_NUMBER = $ver" ) | doxygen -`). The version constants stay in exactly one place, so a release bump does not have to remember the Doxyfile — and the Milestone 8.6 consistency lint has one fewer copy to police.

The HTML is dependency-free: the built-in Doxygen theme with `GENERATE_TREEVIEW = YES`, no Graphviz (`HAVE_DOT = NO`), and no vendored third-party CSS. `PBR_MEMORY_POOL_DIAGNOSTICS=1` is predefined so the free-list diagnostic surface (ADR-0019) appears in the reference.

## Alternatives Considered

- **MkDocs Material or Sphinx-Breathe unified site now.** The ADR-0013 §4 end-state — one site indexing API reference *and* narrative. Rejected for v1.0: it adds a Python documentation toolchain (the repository's largest dependency), is a tune-and-iterate research task off the v1.0 critical path, and delivers marginal value over GitHub's already-good Markdown rendering. Deferred post-1.0, where it can be evaluated without release pressure.
- **Read the Docs (or another hosted docs SaaS).** Rejected. It is an external service in the trust boundary, inconsistent with the project's zero-external-dependency posture, when GitHub Pages — already part of the platform hosting everything else — does the job with no new account or secret.
- **Commit generated HTML to a `gh-pages` branch or `docs/` folder.** Rejected. It floods git history with regenerated binary/HTML churn on every doc change and invites the committed output to drift from the headers. The Actions artifact-deploy model keeps the repository to source only.
- **`doxygen-awesome-css` vendored theme.** A popular modern Doxygen skin. Rejected for now: it is a third-party asset to vendor and keep current, and the built-in 1.9/1.10 theme with treeview is clean and adequate for a narrow API surface. Revisitable later as pure polish; not worth a maintained dependency at v1.0.
- **Graphviz/`dot` collaboration & inheritance diagrams.** Tempting given the composition-heavy design (Decorator, Adapter, TypedPool all compose `Pool`). Rejected/deferred: it adds a `graphviz` dependency, makes local validation diverge from CI unless every contributor installs `dot`, and the header set is small enough that Doxygen's textual cross-references convey the relationships. A clean future enhancement.
- **Gate on full documentation exhaustiveness (`WARN_IF_UNDOCUMENTED` + `WARN_NO_PARAMDOC`).** The literal reading of ADR-0013 §5's "fails if any public symbol is missing a comment." Rejected as the *enforced* gate for the reasons in Decision §3 — it rewards ceremonial filler over correctness. Kept as a review obligation, not a build gate.
- **Hardcode the version in the Doxyfile.** Rejected — it creates a fourth copy of the version string to keep in lockstep and would drift on the next release bump; build-time injection from `version.hpp` is the single-source-of-truth path.

## Consequences

**Positive**

- The public API contract is now a browsable, cross-linked, versioned static site — the v1.0 reference-implementation deliverable ROADMAP §7.1 asks for, with no service or toolchain to operate beyond GitHub itself.
- The warn-as-error gate runs on every PR that touches a public header or the Doxygen config, so doc rot (renamed parameters, broken refs, malformed commands) fails CI before merge — the technical backstop ADR-0013 §5 and AGENTS.md §10 promised.
- Zero new runtime or build dependencies; the docs toolchain matches the project's spec §3.3 zero-dependency philosophy. Version stays single-sourced.
- The decision is reversible and extensible: the unified MkDocs/Sphinx site, a vendored theme, and `dot` diagrams are all clean future additions that do not invalidate this pipeline.

**Negative / costs**

- **GitHub Pages must be enabled once in repository settings** (Settings → Pages → Source: *GitHub Actions*). Until the maintainer does so, the `deploy` job on `master` fails; the **PR build gate is unaffected** (it never deploys), so contributor workflow is not blocked. This one-time action is called out in the M7.1 PR body.
- CI installs Doxygen from `apt` (Ubuntu 24.04 → Doxygen 1.9.x), which may lag the maintainer's local 1.10.x. The Doxyfile uses only options available since 1.9.5, so the gate behaves identically; a future version-specific divergence would surface as a CI failure, not a silent skip.
- Three public-header comments now write `::%operator new` / `::%operator delete` — the Doxygen `%` escape that suppresses a spurious auto-link to the global operators (which are not documented symbols in this project, so the link cannot resolve). The `%` is invisible in the rendered HTML and is a conventional Doxygen-ism, at the cost of a minor blemish when reading the raw header.
- The narrative (ADRs, spec, guides) is **not yet** unified with the API reference on one site; readers follow the mainpage's outbound links to GitHub. Acceptable for v1.0 and explicitly deferred (Decision §2).

**Documentation updates landing in the same PR as this ADR**

- [`docs/adr/README.md`](README.md) — index row for ADR-0027 (Accepted).
- [ROADMAP](../../ROADMAP.md) §7.1 — checkbox flipped with an implementation note.
- [`README.md`](../../README.md) — a pointer to the published API-reference site and a `docs-site` build badge.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — `Added` entry recording the pipeline and ADR.

## References

- [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) §4–§5 — the rendering pipeline and warn-as-error expectation this ADR decides and refines.
- [ROADMAP](../../ROADMAP.md) §7.1 — the item implemented here.
- [`docs/doxygen/Doxyfile`](../doxygen/Doxyfile), [`docs/doxygen/mainpage.md`](../doxygen/mainpage.md), [`.github/workflows/docs-site.yml`](../../.github/workflows/docs-site.yml) — the artifacts this ADR governs.
- Doxygen manual — [`https://www.doxygen.nl/manual/`](https://www.doxygen.nl/manual/); `WARN_AS_ERROR` / `FAIL_ON_WARNINGS` and the auto-link `%` escape referenced above.
- GitHub Pages with Actions — [`https://github.com/actions/deploy-pages`](https://github.com/actions/deploy-pages).
