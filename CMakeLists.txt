cmake_minimum_required(VERSION 3.10)

# Thin wrapper so `cmake -S .` behaves like `cmake -S build/cmake`.
# Policy lives in build/cmake; keep parent project language-less.
project(zstd-superbuild LANGUAGES NONE)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Specify -B <build-dir>.")
endif()

add_subdirectory(build/cmake)
