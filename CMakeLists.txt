# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Daniel Polo
#
# Top-level CMake build for pbr-cpp-memory-pool.
# Toolchain matrix and language standards are fixed by ADR-0005.

cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------
# Version: read the canonical constants from version.hpp (ADR-0004 §2).
# ---------------------------------------------------------------------------
set(PBR_VERSION_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/it/d4np/memorypool/version.hpp")
file(READ "${PBR_VERSION_HEADER}" _pbr_version_contents)
string(REGEX MATCH "PBR_MEMORY_POOL_VERSION_MAJOR = ([0-9]+)" _ "${_pbr_version_contents}")
set(_pbr_ver_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "PBR_MEMORY_POOL_VERSION_MINOR = ([0-9]+)" _ "${_pbr_version_contents}")
set(_pbr_ver_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "PBR_MEMORY_POOL_VERSION_PATCH = ([0-9]+)" _ "${_pbr_version_contents}")
set(_pbr_ver_patch "${CMAKE_MATCH_1}")
# Use STREQUAL "" (not `if(NOT var)`) — CMake's truthiness rules treat the
# string "0" as false, which would misfire when a version component is
# legitimately zero (e.g. v0.1.0's MAJOR or PATCH).
if("${_pbr_ver_major}" STREQUAL ""
        OR "${_pbr_ver_minor}" STREQUAL ""
        OR "${_pbr_ver_patch}" STREQUAL "")
    message(FATAL_ERROR
        "Could not parse version constants from ${PBR_VERSION_HEADER}. "
        "The regex contract expected by CMake must stay in sync with the "
        "constants declared in that header (ADR-0004 §2). "
        "Parsed values: MAJOR='${_pbr_ver_major}', MINOR='${_pbr_ver_minor}', "
        "PATCH='${_pbr_ver_patch}'.")
endif()

project(pbr_memory_pool
    VERSION ${_pbr_ver_major}.${_pbr_ver_minor}.${_pbr_ver_patch}
    DESCRIPTION "Purpose-built reference high-performance memory pool"
    HOMEPAGE_URL "https://github.com/danielPoloWork/pbr-cpp-memory-pool"
    LANGUAGES CXX C)

# ---------------------------------------------------------------------------
# Language standards (ADR-0005 §3).
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The C language is only used by the verification jobs in ROADMAP §1.10 to
# compile the public header under -std=c89 and -std=c99. The library itself
# carries no .c translation units, so the project-wide C floor is the
# pragmatic baseline (C99); the per-standard verification jobs override it.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Always emit compile_commands.json for clang-tidy and IDE integrations.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config generators: default to Release if the user did not specify.
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# ---------------------------------------------------------------------------
# Strict warnings, applied to every target in this build (ADR-0005 §3).
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ---------------------------------------------------------------------------
# The library target. STATIC from day 1 — Milestone 1 ships stub
# implementations in memory_pool.cpp so the library is linkable; Milestone 2
# replaces the stubs with the real algorithms without touching this section.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PBR_POOL_SOURCES CONFIGURE_DEPENDS
    "src/main/cpp/it/d4np/memorypool/*.cpp"
    "src/main/cpp/it/d4np/memorypool/*.c")
file(GLOB_RECURSE PBR_POOL_HEADERS CONFIGURE_DEPENDS
    "src/main/cpp/it/d4np/memorypool/*.h"
    "src/main/cpp/it/d4np/memorypool/*.hpp")

add_library(pbr_memory_pool STATIC ${PBR_POOL_SOURCES} ${PBR_POOL_HEADERS})
add_library(pbr::memory_pool ALIAS pbr_memory_pool)

target_include_directories(pbr_memory_pool PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp>
    $<INSTALL_INTERFACE:include>)

target_compile_features(pbr_memory_pool PUBLIC cxx_std_17)

set_target_properties(pbr_memory_pool PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    # Export under the short name so the installed imported target is
    # `pbr::memory_pool` — identical to the in-build ALIAS above, so consumers
    # write the same `target_link_libraries(... pbr::memory_pool)` whether they
    # vendor via add_subdirectory / FetchContent or link an installed package
    # (ADR-0028). Without this, NAMESPACE pbr:: + target `pbr_memory_pool` would
    # export the mismatched `pbr::pbr_memory_pool`.
    EXPORT_NAME memory_pool)

# ---------------------------------------------------------------------------
# Free-list diagnostics gate (ADR-0019). The diagnostic surface (the
# memory_pool_debug_* C accessors and the C++ free_list_iterator.hpp) is
# driven by the PBR_MEMORY_POOL_DIAGNOSTICS macro, which defaults to ON in
# debug builds and OFF in release builds (NDEBUG). This option is the
# documented "explicitly enable" channel: when ON it forces the macro to 1
# PUBLIC-ly on the library target, so the library and every consumer that
# links it agree regardless of build type.
# ---------------------------------------------------------------------------
option(PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS
    "Force the free-list diagnostics surface on regardless of build type (ADR-0019)" OFF)
if(PBR_MEMORY_POOL_ENABLE_DIAGNOSTICS)
    target_compile_definitions(pbr_memory_pool PUBLIC PBR_MEMORY_POOL_DIAGNOSTICS=1)
endif()

# ---------------------------------------------------------------------------
# Opt-in debug hardening (ADR-0043). When ON, the free list gains freed-block
# poisoning, a per-slot guard word (buffer-overflow + double-free detection),
# and next-pointer safe-linking (see pool_hardening.hpp). OFF by default — a
# release build is byte-for-byte unchanged. Defined PUBLIC because a hardened
# build changes the on-disk free-list encoding AND the physical slot stride, so
# the library and every consumer / test linking it must agree; never mix a
# hardened and a non-hardened build.
# ---------------------------------------------------------------------------
option(PBR_MEMORY_POOL_HARDENING
    "Enable opt-in debug hardening: poisoning, guard word, free-list safe-linking (ADR-0043)" OFF)
if(PBR_MEMORY_POOL_HARDENING)
    target_compile_definitions(pbr_memory_pool PUBLIC PBR_MEMORY_POOL_HARDENING=1)
endif()

# ---------------------------------------------------------------------------
# Thread-safety policy (ADR-0020). Selects how the implementation
# synchronizes the free-list head, fixed library-wide at build time. The
# default NONE preserves the single-thread fast path (spec §2.4). The macro
# is defined PRIVATE — it changes only memory_pool.cpp's internal policy, not
# the public ABI or any public declaration, so consumers need not see it.
# ---------------------------------------------------------------------------
set(PBR_MEMORY_POOL_THREAD_SAFETY "NONE" CACHE STRING
    "Thread-safety policy: NONE (default, single-threaded) | MUTEX | LOCKFREE (ADR-0020)")
set_property(CACHE PBR_MEMORY_POOL_THREAD_SAFETY PROPERTY STRINGS NONE MUTEX LOCKFREE)
if(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "MUTEX")
    target_compile_definitions(pbr_memory_pool PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_MUTEX)
elseif(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "LOCKFREE")
    target_compile_definitions(pbr_memory_pool PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE)
    # The ABA-tagged head is a 16-byte (double-width) std::atomic. GCC/Clang
    # lower its compare_exchange to libatomic calls (__atomic_*_16) unless
    # -mcx16 is set, so the library and its consumers must link libatomic.
    # MSVC lowers it to cmpxchg16b intrinsically and Apple Clang provides the
    # builtins, so neither needs (or ships) libatomic — exclude them.
    if(NOT MSVC AND NOT APPLE)
        target_link_libraries(pbr_memory_pool PUBLIC atomic)
    endif()
elseif(NOT PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "NONE")
    message(FATAL_ERROR
        "PBR_MEMORY_POOL_THREAD_SAFETY must be NONE, MUTEX, or LOCKFREE "
        "(got '${PBR_MEMORY_POOL_THREAD_SAFETY}').")
endif()
# NONE leaves the macro at the memory_pool.h default — no compile definition.

# ---------------------------------------------------------------------------
# Install / packaging — Phase 1 distribution (ADR-0028, ROADMAP §7.4 /
# ADR-0004 §5). Exports a namespaced imported target so consumers can
# `find_package(pbr_memory_pool CONFIG REQUIRED)` + link `pbr::memory_pool`,
# installs the public-header tree, and emits a pkg-config `.pc`. Gated so a
# parent project that pulls this in via add_subdirectory / FetchContent does
# not leak our install rules into its own `cmake --install`.
# ---------------------------------------------------------------------------
option(PBR_MEMORY_POOL_INSTALL
    "Generate install and packaging rules (config file, headers, pkg-config)"
    ${PROJECT_IS_TOP_LEVEL})
if(PBR_MEMORY_POOL_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(PBR_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/pbr_memory_pool"
        CACHE STRING "Install location for the CMake package config files")

    # The library archive + the install-tree include directory.
    install(TARGETS pbr_memory_pool
        EXPORT pbr_memory_poolTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    # Public headers, preserving the it/d4np/memorypool/ tree so the
    # `#include <it/d4np/memorypool/...>` form resolves against <prefix>/include.
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/it"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
            PATTERN "*.h"
            PATTERN "*.hpp")

    # The exported targets file — defines the imported pbr::memory_pool target.
    install(EXPORT pbr_memory_poolTargets
        FILE pbr_memory_poolTargets.cmake
        NAMESPACE pbr::
        DESTINATION "${PBR_INSTALL_CMAKEDIR}")

    # The package config + version files.
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pbr_memory_poolConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/pbr_memory_poolConfig.cmake"
        INSTALL_DESTINATION "${PBR_INSTALL_CMAKEDIR}")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/pbr_memory_poolConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/pbr_memory_poolConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/pbr_memory_poolConfigVersion.cmake"
        DESTINATION "${PBR_INSTALL_CMAKEDIR}")

    # pkg-config for non-CMake (autotools / Make / Meson) consumers.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pbr-memory-pool.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/pbr-memory-pool.pc"
        @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pbr-memory-pool.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
endif()

# ---------------------------------------------------------------------------
# Optional sub-trees, wired in by later milestones.
# ---------------------------------------------------------------------------
option(PBR_MEMORY_POOL_BUILD_TESTS
    "Build the test targets under src/test/cpp/" OFF)
if(PBR_MEMORY_POOL_BUILD_TESTS)
    enable_testing()

    # doctest is our test framework (ADR-0007). Pinned to a stable tag,
    # cloned shallow at configure time. The first configure on a fresh
    # build tree pays a one-time network cost (a few seconds); subsequent
    # configures hit the cache under build/_deps/.
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE
    )
    # CMake 4.x removed support for `cmake_minimum_required(VERSION < 3.5)`,
    # and doctest v2.4.11's internal CMakeLists.txt declares a lower minimum.
    # Without this override the configure fails on any runner with CMake >= 4
    # ("Compatibility with CMake < 3.5 has been removed"). Scope the policy
    # floor to doctest only — our own project keeps its modern policy
    # posture by unsetting the variable immediately after. Remove this block
    # when doctest publishes a release whose own CMakeLists.txt asks for
    # >= 3.5.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_MakeAvailable(doctest)
    unset(CMAKE_POLICY_VERSION_MINIMUM)

    add_subdirectory(src/test/cpp/it/d4np/memorypool)
endif()

option(PBR_MEMORY_POOL_BUILD_BENCHMARKS
    "Build the benchmark targets under src/bench/cpp/" OFF)
if(PBR_MEMORY_POOL_BUILD_BENCHMARKS)
    # Methodology fixed by ADR-0014. The benchmark binary depends only on
    # pbr::memory_pool and the C/C++ standard library — spec §3.3's zero-
    # external-dependency posture is preserved into the bench tree.
    add_subdirectory(src/bench/cpp/it/d4np/memorypool)
endif()

# ---------------------------------------------------------------------------
# Coverage-guided fuzzing (ADR-0044). When ON, the test tree adds the
# libFuzzer target `pool_fuzz` (Clang only — libFuzzer is a Clang runtime),
# built with -fsanitize=fuzzer,address,undefined and run by a dedicated CI
# job. OFF by default so it never perturbs the release build or the ADR-0014
# benchmark methodology. The portable `pool_fuzz_replay` target (which replays
# the seed corpus as a regression gate) is built with the normal test suite
# regardless of this option; only the coverage-guided engine is gated here.
# Requires PBR_MEMORY_POOL_BUILD_TESTS, since the target lives in the test tree.
# ---------------------------------------------------------------------------
option(PBR_MEMORY_POOL_BUILD_FUZZERS
    "Build the libFuzzer coverage-guided fuzz target (Clang only, ADR-0044)" OFF)

# ---------------------------------------------------------------------------
# Configuration summary (debug aid; not output in -DCMAKE_MESSAGE_LOG_LEVEL=ERROR).
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "  pbr-cpp-memory-pool ${PROJECT_VERSION}")
message(STATUS "  --------------------------------------")
message(STATUS "  C++ compiler        : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ standard        : C++${CMAKE_CXX_STANDARD} (extensions ${CMAKE_CXX_EXTENSIONS})")
message(STATUS "  Build type          : ${CMAKE_BUILD_TYPE}${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "  Tests               : ${PBR_MEMORY_POOL_BUILD_TESTS}")
message(STATUS "  Benchmarks          : ${PBR_MEMORY_POOL_BUILD_BENCHMARKS}")
message(STATUS "  Fuzzers             : ${PBR_MEMORY_POOL_BUILD_FUZZERS}")
message(STATUS "  Install rules        : ${PBR_MEMORY_POOL_INSTALL}")
message(STATUS "")
