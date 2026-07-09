// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_POOL_HARDENING_HPP_
#define IT_D4NP_MEMORYPOOL_POOL_HARDENING_HPP_

/**
 * @file pool_hardening.hpp
 * @brief Opt-in debug-hardening surface for the free list — ADR-0043.
 *
 * When the library is built with `PBR_MEMORY_POOL_HARDENING` (a compile-time
 * knob, OFF by default — see the CMake option of the same name), the pool's
 * intrusive free list gains three self-contained protections against the
 * classic use-after-free / pointer-corruption primitives an intrusive free
 * list exposes (ADR-0009 §1):
 *
 * 1. **Freed-block poisoning** — a freed block's payload is filled with a
 *    recognizable byte pattern; a write to freed memory is caught on the next
 *    allocation of that block (use-after-free).
 * 2. **Guard word** — a trailing guard word per slot detects a contiguous
 *    write past `block_size` (buffer overflow) and a repeated free of the same
 *    block (double-free), deterministically.
 * 3. **Free-list safe-linking** — the in-band next-pointer is stored XORed with
 *    a per-slot key (glibc's `PROTECT_PTR`/`REVEAL_PTR`), so a leaked or
 *    overwritten next-link is neither directly usable nor silently followed;
 *    corruption surfaces as an alignment fault on reveal.
 *
 * Release builds are byte-for-byte and cycle-for-cycle unchanged — the entire
 * mechanism is compiled out when the knob is off, and this header is then a
 * no-op. A hardened build changes the on-disk free-list encoding **and** the
 * physical slot stride, so it is deliberately **not** memory-layout-compatible
 * with a non-hardened build: never mix the two configurations.
 *
 * This header exposes only the *violation policy* hook. On a detected
 * violation the library calls the installed handler; the default handler
 * prints a diagnostic and calls `std::abort()` (the ADR-0012 "defined,
 * loud failure" stance). Tests install a recording handler so a violation can
 * be asserted without terminating the process.
 */

#include <it/d4np/memorypool/memory_pool.h>

#if PBR_MEMORY_POOL_HARDENING

namespace it::d4np::memorypool {

/**
 * @brief Handler invoked when the hardening layer detects a violation.
 *
 * @param kind  A stable, static string naming the violation — one of the
 *              `HARDENING_*` constants below.
 * @param block Address of the offending block (for the diagnostic).
 *
 * The handler is `noexcept`: it is called from the pool's `noexcept`
 * allocate/deallocate path. The default handler does not return (it aborts);
 * a handler that *does* return lets the operation continue on a best-effort,
 * no-further-corruption path (used by the tests).
 */
using HardeningViolationHandler = void (*)(const char* kind, const void* block) noexcept;

/** A write to a freed block's poisoned payload was detected on allocation. */
inline constexpr const char* HARDENING_USE_AFTER_FREE = "use-after-free";
/** A contiguous write past `block_size` corrupted the trailing guard word. */
inline constexpr const char* HARDENING_OVERFLOW = "buffer-overflow";
/** The same block was freed twice (its guard still read as freed). */
inline constexpr const char* HARDENING_DOUBLE_FREE = "double-free";
/** A free-list slot's guard or next-link was corrupted (integrity check). */
inline constexpr const char* HARDENING_FREELIST_CORRUPTION = "free-list-corruption";

/**
 * Install @p handler as the hardening violation handler and return the
 * previous one. Passing `nullptr` restores the default (diagnostic + abort).
 * Thread-safe. Present only in hardened builds.
 */
HardeningViolationHandler set_hardening_violation_handler(HardeningViolationHandler handler) noexcept;

/** @return The currently installed hardening violation handler. */
[[nodiscard]] HardeningViolationHandler hardening_violation_handler() noexcept;

}  // namespace it::d4np::memorypool

#endif  // PBR_MEMORY_POOL_HARDENING

#endif  // IT_D4NP_MEMORYPOOL_POOL_HARDENING_HPP_
