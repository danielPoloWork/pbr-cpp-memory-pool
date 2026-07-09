# Security Policy

`pbr-cpp-memory-pool` is a small, self-contained C/C++ library with **no external runtime or build dependencies** (only the C and C++ standard libraries), so its attack surface is its own public API and allocation logic. This policy explains which versions receive security fixes and how to report a vulnerability. It is the public face of the security-fix path in the [post-release maintenance protocol](docs/workflow/maintenance.md).

## Supported versions

Security fixes are issued for the **latest released minor line**. Older lines are not patched — upgrade to the latest `1.x`.

| Version | Supported |
|---------|-----------|
| `1.1.x` (latest) | ✅ |
| `1.0.x` | ❌ — upgrade to the latest `1.x` |
| `< 1.0` (pre-release `0.x`) | ❌ |

Because the public C ABI and C++ surface are frozen under SemVer 1.0, upgrading within the `1.x` line is source- and binary-compatible (see the [maintenance protocol](docs/workflow/maintenance.md)).

## Reporting a vulnerability

**Please do not open a public issue or pull request for a suspected vulnerability.** Report it privately through GitHub's **[private vulnerability reporting](https://docs.github.com/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)**:

> Repository **Security** tab → **Report a vulnerability**.

This opens a private security advisory visible only to you and the maintainer.

A useful report includes:

- the affected version(s) and platform / compiler / thread-safety mode (`NONE` / `MUTEX` / `LOCKFREE`);
- a minimal reproduction (ideally a small program against the public API);
- the impact you observed (memory corruption, leak, crash, UB under a sanitizer, etc.);
- any suggested fix, if you have one.

## What to expect

This is a single-maintainer reference project, so timelines are **best-effort**, handled per the maintenance protocol's security path:

1. **Acknowledgement** of the report, typically within a few days.
2. **Triage** under embargo on a private branch / draft advisory, and assessment of the SemVer level (a fix is usually a `PATCH`; a fix that must change public behaviour may be a `MINOR`/`MAJOR`).
3. **Coordinated disclosure** — the fix is released and *then* the advisory is published.
4. The fix is recorded in [`CHANGELOG.md`](CHANGELOG.md) under a **`Security`** category (Keep a Changelog) with the advisory reference, and backported to every supported line.
5. **Credit** to the reporter in the advisory and changelog, unless you prefer to remain anonymous.

## Scope

In scope: memory-safety and correctness defects in the library's own code reachable through its public API or documented build options — e.g. out-of-bounds access, use-after-free, double-free, leaks, data races in the `MUTEX` / `LOCKFREE` policies, or integer-overflow in size computations.

For defense-in-depth and bug-finding, the library ships an **opt-in debug-hardening build** (compile-time knob `PBR_MEMORY_POOL_HARDENING`, OFF by default; [ADR-0043](docs/adr/0043-opt-in-debug-hardening.md)) that turns use-after-free, buffer-overflow-past-`block_size`, and double-free into deterministic, loud failures via freed-block poisoning, a per-slot guard word, and glibc-style free-list safe-linking. It complements the sanitizer matrix (and works where ASan does not, e.g. MSVC); it is a debugging aid, not a substitute for the sanitizers, and a hardened build is intentionally not layout-compatible with a default one.

Out of scope: misuse that the documentation explicitly calls undefined behaviour (e.g. pairing storage and object-lifetime verbs incorrectly, or a moved-from wrapper used after move), issues in a consumer's own code, and vulnerabilities in third-party toolchains. The default single-threaded build is intentionally not thread-safe (spec §2.4) — concurrent use of a `NONE`-policy pool is not a vulnerability.

## See also

- [`docs/workflow/maintenance.md`](docs/workflow/maintenance.md) — the full maintenance protocol, including the security-fix path and the patch/minor/major decision tree.
- [`docs/adr/0004-versioning-and-release-policy.md`](docs/adr/0004-versioning-and-release-policy.md) — versioning & release policy.
