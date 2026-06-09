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
    SOVERSION ${PROJECT_VERSION_MAJOR})

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
    FetchContent_MakeAvailable(doctest)

    add_subdirectory(src/test/cpp/it/d4np/memorypool)
endif()

option(PBR_MEMORY_POOL_BUILD_BENCHMARKS
    "Build the benchmark targets under src/bench/cpp/" OFF)
if(PBR_MEMORY_POOL_BUILD_BENCHMARKS)
    # add_subdirectory(src/bench/cpp/it/d4np/memorypool)  # enabled in Milestone 2.9
endif()

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
message(STATUS "")
