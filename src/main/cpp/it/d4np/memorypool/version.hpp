// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

#ifndef IT_D4NP_MEMORYPOOL_VERSION_HPP_
#define IT_D4NP_MEMORYPOOL_VERSION_HPP_

/**
 * @file version.hpp
 * @brief Project version constants.
 *
 * Single source of truth for the project version, per ADR-0004. The
 * top-level `CMakeLists.txt` reads these constants at configure time to
 * populate `project(... VERSION ...)`. Bump these values from the release
 * PR that closes a milestone (see `docs/workflow/release.md`).
 */

namespace it::d4np::memorypool {

/** Major version component (incremented for breaking changes post-1.0). */
inline constexpr unsigned PBR_MEMORY_POOL_VERSION_MAJOR = 0;

/** Minor version component (incremented with each closed milestone pre-1.0). */
inline constexpr unsigned PBR_MEMORY_POOL_VERSION_MINOR = 5;

/** Patch version component (incremented for hotfixes between milestones). */
inline constexpr unsigned PBR_MEMORY_POOL_VERSION_PATCH = 0;

/** Pre-formatted version string, kept in lockstep with the components above. */
inline constexpr const char* PBR_MEMORY_POOL_VERSION_STRING = "0.5.0";

}  // namespace it::d4np::memorypool

#endif  // IT_D4NP_MEMORYPOOL_VERSION_HPP_
