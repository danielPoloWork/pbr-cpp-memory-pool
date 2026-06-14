# Minimal vcpkg consumer for the packaging-smoke workflow. Built against the
# overlay port (ports/pbr-memory-pool) with the vcpkg toolchain to prove the
# installed package exposes find_package(pbr_memory_pool) + pbr::memory_pool —
# the same imported target documented in ports/README.md.
cmake_minimum_required(VERSION 3.21)
project(pbr_vcpkg_smoke CXX)

find_package(pbr_memory_pool CONFIG REQUIRED)

add_executable(smoke src/smoke.cpp)
target_link_libraries(smoke PRIVATE pbr::memory_pool)
