# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Daniel Polo
#
# Benchmark targets for pbr-cpp-memory-pool — ROADMAP §2.9 (M2.9, spec §6.3).
# Methodology contract: ADR-0014.
#
# Gated by the PBR_MEMORY_POOL_BUILD_BENCHMARKS option in the top-level
# CMakeLists.txt; only entered when the `bench` preset (or an equivalent
# manual configure with -DPBR_MEMORY_POOL_BUILD_BENCHMARKS=ON) is in use.

add_executable(pool_vs_malloc_bench
    pool_vs_malloc_bench.cpp)

target_link_libraries(pool_vs_malloc_bench PRIVATE
    pbr::memory_pool)

target_compile_features(pool_vs_malloc_bench PRIVATE cxx_std_17)

# Property name used by IDEs that group targets; matches the layout under
# src/bench/cpp/.
set_target_properties(pool_vs_malloc_bench PROPERTIES
    FOLDER "bench")
