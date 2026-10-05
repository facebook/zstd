# Copyright (c) Meta Platforms, Inc. and affiliates.
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.

cmake_minimum_required(VERSION 3.10)
project(zstd_consumer C CXX)

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Build command-line programs")
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "Build zstd's test suite")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." zstd)

enable_testing()
foreach(kind IN ITEMS static shared)
    add_executable(consumer_${kind} main.cpp)
    # Consumers must receive the public headers through the linked target,
    # without inheriting zstd's directory-scoped include paths.
    target_link_libraries(consumer_${kind} PRIVATE libzstd_${kind})
    set_target_properties(consumer_${kind} PROPERTIES CXX_STANDARD 98 CXX_STANDARD_REQUIRED ON)
    add_test(NAME consumer_${kind} COMMAND consumer_${kind})
endforeach()

if(WIN32)
    add_custom_command(TARGET consumer_shared POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:libzstd_shared>" "$<TARGET_FILE_DIR:consumer_shared>"
        VERBATIM)
endif()
