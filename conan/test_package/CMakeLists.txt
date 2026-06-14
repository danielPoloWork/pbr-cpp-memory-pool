cmake_minimum_required(VERSION 3.21)
project(test_package CXX)

find_package(pbr_memory_pool CONFIG REQUIRED)

add_executable(example src/example.cpp)
target_link_libraries(example PRIVATE pbr::memory_pool)
