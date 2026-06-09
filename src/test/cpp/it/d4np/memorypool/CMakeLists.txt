# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Daniel Polo
#
# Test targets for pbr-cpp-memory-pool. Framework choice and CTest integration
# governed by ADR-0007. The list of test executables grows as milestones land;
# each one self-registers with CTest below.

add_executable(pool_smoke_test
    pool_smoke_test.cpp)

target_link_libraries(pool_smoke_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(pool_smoke_test PRIVATE cxx_std_17)

add_test(
    NAME    pool_smoke
    COMMAND pool_smoke_test)

set_tests_properties(pool_smoke PROPERTIES
    LABELS "smoke;milestone-1")
