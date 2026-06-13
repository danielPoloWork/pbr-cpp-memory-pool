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

# ROADMAP item 3.2 — TypedPool<T> type-safe surface (ADR-0017). The
# template is header-only, so this binary is also the compile-time proof
# that typed_pool.hpp instantiates cleanly under the full warning set.
add_executable(typed_pool_test
    typed_pool_test.cpp)

target_link_libraries(typed_pool_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(typed_pool_test PRIVATE cxx_std_17)

add_test(
    NAME    typed_pool
    COMMAND typed_pool_test)

set_tests_properties(typed_pool PROPERTIES
    LABELS "smoke;milestone-3")

# ROADMAP item 3.3 — PoolAllocator<T> STL adapter (ADR-0018). The adapter is
# header-only, so this binary is also the compile-time proof that
# pool_allocator.hpp instantiates cleanly (including through std::list and
# std::vector) under the full warning set.
add_executable(pool_allocator_test
    pool_allocator_test.cpp)

target_link_libraries(pool_allocator_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(pool_allocator_test PRIVATE cxx_std_17)

add_test(
    NAME    pool_allocator
    COMMAND pool_allocator_test)

set_tests_properties(pool_allocator PROPERTIES
    LABELS "smoke;milestone-3")

# ROADMAP item 3.4 — read-only free-list diagnostic Iterator (ADR-0019).
# The iterator is header-only and gated behind PBR_MEMORY_POOL_DIAGNOSTICS;
# the test TU compiles in both states (a placeholder case when the surface
# is gated out), so this binary builds in every configuration.
add_executable(free_list_iterator_test
    free_list_iterator_test.cpp)

target_link_libraries(free_list_iterator_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(free_list_iterator_test PRIVATE cxx_std_17)

add_test(
    NAME    free_list_iterator
    COMMAND free_list_iterator_test)

set_tests_properties(free_list_iterator PROPERTIES
    LABELS "smoke;milestone-3")

# ROADMAP item 2.8 — spec §6.2 Valgrind demonstrative test. The directory
# carries its own CMakeLists.txt because the target consumes a strict ANSI
# C89 .c source and overrides the project-wide C99 floor on its own scope.
add_subdirectory(spec_6_2_valgrind)
