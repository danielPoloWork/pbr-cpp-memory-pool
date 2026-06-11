# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Daniel Polo
#
# Spec §6.2 — Valgrind verification target (ROADMAP item 2.8).
#
# The .c source is the literal demonstrative test from spec §6.2 and
# compiles under strict ANSI C89 (overriding the project-wide C99 floor on
# this target only — any future C99 construct slipped into test_pool.c
# causes an immediate compile error here). The executable links against
# pbr::memory_pool which brings in the C++ implementation; the linker is
# CMake's CXX linker so libstdc++ is pulled in automatically.
#
# The literal `gcc -g -O0 test_pool.c memory_pool.c -o test_pool` shell
# command from spec §6.2 is reproduced in the `valgrind` CI job in
# .github/workflows/ci.yml; this CMake target is the local-reproducibility
# parallel path. See ./README.md for the side-by-side mapping.

add_executable(spec_6_2_valgrind_test_pool
    test_pool.c)

# The spec command issues no `-std=` flag and relies on gcc's default
# (gnu17 on every Tier-1 toolchain). We pin C90 here so a regression
# that slips a C99 construct into test_pool.c fails CI loudly rather than
# silently drifting away from the ANSI C contract.
set_target_properties(spec_6_2_valgrind_test_pool PROPERTIES
    C_STANDARD 90
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    # Force the C++ linker so libstdc++ is brought in (the C++17 impl
    # in memory_pool.cpp depends on it). Without this property CMake
    # picks the C linker when every TU directly part of the target is
    # .c, even though the linked dependency is C++.
    LINKER_LANGUAGE CXX)

target_link_libraries(spec_6_2_valgrind_test_pool PRIVATE
    pbr::memory_pool)

add_test(
    NAME    spec_6_2_valgrind
    COMMAND spec_6_2_valgrind_test_pool)

set_tests_properties(spec_6_2_valgrind PROPERTIES
    LABELS "spec;milestone-2;valgrind")
