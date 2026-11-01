cmake_minimum_required(VERSION 3.10)

# Determine if zstd is built as a subproject (using add_subdirectory) or if it
# is the master project.
if (NOT DEFINED ZSTD_MASTER_PROJECT)
  set(ZSTD_MASTER_PROJECT OFF)
  # Checking project name is more reliable than checking source directories.
  if (NOT DEFINED PROJECT_NAME)
    set(ZSTD_MASTER_PROJECT ON)
  endif ()
endif ()

# Thin wrapper so `cmake -S .` behaves like `cmake -S build/cmake`.
# Policy lives in build/cmake; keep parent project language-less.
project(zstd-superbuild LANGUAGES NONE)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Specify -B <build-dir>.")
endif()

add_subdirectory(build/cmake)
