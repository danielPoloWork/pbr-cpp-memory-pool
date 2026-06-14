#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Daniel Polo
"""Agent-runnable consistency lint for pbr-cpp-memory-pool (ADR-0035, ROADMAP §8.6).

A dependency-free (Python 3 standard library only) checker that asserts the
project is internally congruent after any change. It exits non-zero with an
actionable report when a congruence invariant is violated. Run from anywhere:

    python tools/consistency_lint.py

The checks (the "post-release congruence contract"):

  1. version constants are in lockstep across version.hpp / CHANGELOG / the
     README status badge / the latest docs/releases/*.md;
  2. every ADR file is in the index and every indexed ADR exists (bijection),
     with no numbering gap;
  3. every catalogued Adopted pattern cites an existing ADR and an existing
     code location under src/main/cpp/;
  4. the Spec Coverage Map has no dangling row (valid status glyph + non-empty
     roadmap-items cell);
  5. the i18n manifest has no `translated` entry staler than its English source
     (the recorded source commit is the source file's latest commit);
  6. ROADMAP / README milestone-completion state is internally consistent.

Each check is independent; all run, then the report lists every failure.
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

failures = []  # list of (check_name, message)


def fail(check, message):
    failures.append((check, message))


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
        return handle.read()


def git(*args):
    """Run git from the repo root; return stdout (stripped) or None on error."""
    try:
        out = subprocess.run(
            ["git", "-C", ROOT, *args],
            capture_output=True, text=True, check=True,
        )
        return out.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def semver_tuple(text):
    match = re.search(r"(\d+)\.(\d+)\.(\d+)", text)
    return tuple(int(g) for g in match.groups()) if match else None


# --------------------------------------------------------------------------
# 1. Version lockstep
# --------------------------------------------------------------------------
def check_version_lockstep():
    name = "version-lockstep"
    hpp = read("src", "main", "cpp", "it", "d4np", "memorypool", "version.hpp")

    def const(key):
        m = re.search(r"PBR_MEMORY_POOL_VERSION_" + key + r"\s*=\s*(\d+)", hpp)
        return int(m.group(1)) if m else None

    major, minor, patch = const("MAJOR"), const("MINOR"), const("PATCH")
    sm = re.search(r'PBR_MEMORY_POOL_VERSION_STRING\s*=\s*"([^"]+)"', hpp)
    if None in (major, minor, patch) or sm is None:
        fail(name, "could not parse version constants from version.hpp")
        return
    string = sm.group(1)
    expected = f"{major}.{minor}.{patch}"
    if string != expected:
        fail(name, f"version.hpp STRING '{string}' != components '{expected}'")

    # README status badge.
    readme = read("README.md")
    bm = re.search(r"Status-v(\d+\.\d+\.\d+)", readme)
    if bm is None:
        fail(name, "no `Status-vX.Y.Z` badge found in README.md")
    elif bm.group(1) != string:
        fail(name, f"README badge v{bm.group(1)} != version.hpp {string}")

    # Latest dated CHANGELOG block (topmost `## [X.Y.Z] — ...`).
    changelog = read("CHANGELOG.md")
    cm = re.search(r"^##\s*\[(\d+\.\d+\.\d+)\]\s+—", changelog, re.MULTILINE)
    if cm is None:
        fail(name, "no dated `## [X.Y.Z] — ...` block found in CHANGELOG.md")
    elif cm.group(1) != string:
        fail(name, f"latest CHANGELOG [{cm.group(1)}] != version.hpp {string}")

    # Latest release-notes file by semver.
    rel_dir = os.path.join(ROOT, "docs", "releases")
    versions = []
    if os.path.isdir(rel_dir):
        for fn in os.listdir(rel_dir):
            m = re.fullmatch(r"v(\d+\.\d+\.\d+)\.md", fn)
            if m:
                versions.append(m.group(1))
    if not versions:
        fail(name, "no docs/releases/vX.Y.Z.md files found")
    else:
        latest = max(versions, key=semver_tuple)
        if latest != string:
            fail(name, f"latest docs/releases/v{latest}.md != version.hpp {string}")


# --------------------------------------------------------------------------
# 2. ADR index bijection + sequential numbering
# --------------------------------------------------------------------------
def check_adr_index():
    name = "adr-index"
    adr_dir = os.path.join(ROOT, "docs", "adr")
    files = sorted(
        fn for fn in os.listdir(adr_dir) if re.fullmatch(r"\d{4}-.+\.md", fn)
    )
    index = read("docs", "adr", "README.md")
    for fn in files:
        slug = fn[:-3]
        if slug not in index:
            fail(name, f"ADR '{slug}' is not listed in docs/adr/README.md")
    # Sequential numbering, no gaps/dupes.
    nums = [int(fn[:4]) for fn in files]
    for i, n in enumerate(nums, start=1):
        if n != i:
            fail(name, f"ADR numbering gap/dup: expected {i:04d}, found {n:04d}")
            break
    # Every NNNN referenced by an index row links to an existing file.
    for num in re.findall(r"\]\((\d{4}-[a-z0-9-]+\.md)\)", index):
        if not os.path.exists(os.path.join(adr_dir, num)):
            fail(name, f"index links missing ADR file '{num}'")


# --------------------------------------------------------------------------
# 3. Catalogued patterns -> ADR + code location
# --------------------------------------------------------------------------
def check_patterns():
    name = "patterns"
    cat = read("docs", "patterns", "README.md")
    # The Adopted table rows start with `| <n> | <Pattern> | Implemented | ...`.
    rows = re.findall(r"^\|\s*\d+\s*\|.*\|.*$", cat, re.MULTILINE)
    adopted = [r for r in rows if "Implemented" in r or "Planned" in r]
    if len(adopted) < 11:
        fail(name, f"expected >= 11 Adopted pattern rows, found {len(adopted)}")
    for row in adopted:
        pattern = row.split("|")[2].strip()
        adrs = re.findall(r"\((?:\.\./)?adr/(\d{4}-[a-z0-9-]+\.md)\)", row)
        if not adrs:
            fail(name, f"pattern '{pattern}': no ADR link in its row")
        for adr in adrs:
            if not os.path.exists(os.path.join(ROOT, "docs", "adr", adr)):
                fail(name, f"pattern '{pattern}': ADR '{adr}' does not exist")
        code = re.findall(
            r"\((?:\.\./\.\./)?(src/main/cpp/[A-Za-z0-9_./-]+\.(?:h|hpp|cpp))\)", row
        )
        if not code:
            fail(name, f"pattern '{pattern}': no src/main/cpp code location in its row")
        for path in code:
            if not os.path.exists(os.path.join(ROOT, path)):
                fail(name, f"pattern '{pattern}': code location '{path}' does not exist")


# --------------------------------------------------------------------------
# 4. Spec Coverage Map — no dangling row
# --------------------------------------------------------------------------
def check_spec_map():
    name = "spec-map"
    roadmap = read("ROADMAP.md")
    block = re.search(
        r"## Spec Coverage Map.*?(?=\n## |\Z)", roadmap, re.DOTALL
    )
    if block is None:
        fail(name, "no '## Spec Coverage Map' section in ROADMAP.md")
        return
    glyphs = {"⏳", "🚧", "✅", "❎"}
    seen = 0
    for line in block.group(0).splitlines():
        if not line.startswith("| §"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 4:
            fail(name, f"malformed Spec Coverage Map row: {line.strip()}")
            continue
        seen += 1
        section, _req, items, status = cells[0], cells[1], cells[2], cells[3]
        if not items:
            fail(name, f"spec row {section} has an empty 'Roadmap items' cell (dangling)")
        if not any(g in status for g in glyphs):
            fail(name, f"spec row {section} has no recognised status glyph: '{status}'")
    if seen == 0:
        fail(name, "Spec Coverage Map has no '| §...' rows")


# --------------------------------------------------------------------------
# 5. i18n manifest freshness
# --------------------------------------------------------------------------
def check_i18n_freshness():
    name = "i18n-freshness"
    manifest_rel = os.path.join("docs", "i18n", "translation-status.md")
    if not os.path.exists(os.path.join(ROOT, manifest_rel)):
        fail(name, f"missing {manifest_rel}")
        return
    manifest = read("docs", "i18n", "translation-status.md")
    i18n_dir = os.path.join(ROOT, "docs", "i18n")
    rows = 0
    for line in manifest.splitlines():
        # Manifest data rows begin with a link cell `| [...](...)`; this skips
        # the status-vocabulary legend and the header/separator rows.
        if not line.lstrip().startswith("| ["):
            continue
        if "`translated`" not in line:
            continue
        link = re.search(r"\]\(([^)]+)\)", line)
        sha = re.search(r"\|\s*`([0-9a-f]{7,40})`\s*\|", line)
        if link is None or sha is None:
            fail(name, f"could not parse translated manifest row: {line.strip()}")
            continue
        rows += 1
        src = os.path.normpath(os.path.join(i18n_dir, link.group(1)))
        rel = os.path.relpath(src, ROOT).replace(os.sep, "/")
        recorded = sha.group(1)
        newer = git("log", "--oneline", f"{recorded}..HEAD", "--", rel)
        if newer is None:
            fail(name, f"git unavailable or commit {recorded} not in history for {rel} "
                       "(need full clone — fetch-depth: 0)")
        elif newer:
            fail(name, f"i18n translation of {rel} is STALE: source changed since "
                       f"recorded commit {recorded} ({len(newer.splitlines())} commit(s) after)")
    if rows == 0:
        fail(name, "no `translated` rows parsed from the i18n manifest")


# --------------------------------------------------------------------------
# 6. ROADMAP <-> README milestone completion consistency
# --------------------------------------------------------------------------
def check_milestones():
    name = "milestones"
    readme = read("README.md")
    roadmap = read("ROADMAP.md")
    # README milestone table rows: | N | Title | ✅ complete / ⏳ ... |
    readme_status = {}
    for m in re.finditer(r"^\|\s*(\d+)\s*\|[^|]+\|\s*([^|]+?)\s*\|$", readme, re.MULTILINE):
        readme_status[int(m.group(1))] = "✅" in m.group(2)
    if not readme_status:
        fail(name, "no milestone table rows parsed from README.md")
        return
    # ROADMAP milestone sections and their checkbox items.
    sections = re.split(r"^## Milestone (\d+)", roadmap, flags=re.MULTILINE)
    # sections = [pre, '0', body0, '1', body1, ...]
    roadmap_complete = {}
    for i in range(1, len(sections), 2):
        num = int(sections[i])
        body = sections[i + 1].split("\n## ")[0]
        items = re.findall(r"^- \[([ xX])\]\s", body, re.MULTILINE)
        if items:
            roadmap_complete[num] = all(c.lower() == "x" for c in items)
    for num, complete in readme_status.items():
        if complete and num in roadmap_complete and not roadmap_complete[num]:
            fail(name, f"README marks Milestone {num} complete, but ROADMAP has "
                       "unchecked item(s) in that milestone")
    # Malformed checkbox syntax anywhere in ROADMAP.
    for ln in roadmap.splitlines():
        if re.match(r"^- \[", ln) and not re.match(r"^- \[[ xX]\]\s", ln):
            fail(name, f"malformed ROADMAP checkbox: {ln.strip()[:60]}")


CHECKS = [
    check_version_lockstep,
    check_adr_index,
    check_patterns,
    check_spec_map,
    check_i18n_freshness,
    check_milestones,
]


def main():
    for fn in CHECKS:
        try:
            fn()
        except Exception as exc:  # a check crashing is itself a failure
            fail(fn.__name__, f"check crashed: {exc!r}")
    if failures:
        print("Consistency lint: FAIL\n")
        for check, message in failures:
            print(f"  [{check}] {message}")
        print(f"\n{len(failures)} congruence problem(s) found.")
        return 1
    print("Consistency lint: OK — all congruence invariants hold.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
