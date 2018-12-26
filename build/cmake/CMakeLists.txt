# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

cmake_minimum_required(VERSION 2.8.9)

project(zstd)
set(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, defaulting to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
include(GNUInstallDirs)

#-----------------------------------------------------------------------------
# Add extra compilation flags
#-----------------------------------------------------------------------------
include(AddZstdCompilationFlags)
ADD_ZSTD_COMPILATION_FLAGS()

# Always hide XXHash symbols
add_definitions(-DXXH_NAMESPACE=ZSTD_)

#-----------------------------------------------------------------------------
# Installation variables
#-----------------------------------------------------------------------------
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------

# Legacy support
option(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" OFF)

if (ZSTD_LEGACY_SUPPORT)
    message(STATUS "ZSTD_LEGACY_SUPPORT defined!")
    add_definitions(-DZSTD_LEGACY_SUPPORT=5)
else ()
    message(STATUS "ZSTD_LEGACY_SUPPORT not defined!")
    add_definitions(-DZSTD_LEGACY_SUPPORT=0)
endif ()

# Multi-threading support
option(ZSTD_MULTITHREAD_SUPPORT "MULTITHREADING SUPPORT" ON)

if (ZSTD_MULTITHREAD_SUPPORT)
    message(STATUS "ZSTD_MULTITHREAD_SUPPORT is enabled")
else ()
    message(STATUS "ZSTD_MULTITHREAD_SUPPORT is disabled")
endif ()

option(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" ON)
option(ZSTD_BUILD_CONTRIB "BUILD CONTRIB" OFF)
option(ZSTD_BUILD_TESTS "BUILD TESTS" OFF)
if (MSVC)
    option(ZSTD_USE_STATIC_RUNTIME "LINK TO STATIC RUN-TIME LIBRARIES" OFF)
endif ()

#-----------------------------------------------------------------------------
# External dependencies
#-----------------------------------------------------------------------------
if (ZSTD_MULTITHREAD_SUPPORT AND UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    if(CMAKE_USE_PTHREADS_INIT)
        set(THREADS_LIBS "${CMAKE_THREAD_LIBS_INIT}")
    else()
        message(SEND_ERROR "ZSTD currently does not support thread libraries other than pthreads")
    endif()
endif ()

#-----------------------------------------------------------------------------
# Add source directories
#-----------------------------------------------------------------------------
add_subdirectory(lib)

if (ZSTD_BUILD_PROGRAMS)
    if (NOT ZSTD_BUILD_STATIC)
        message(SEND_ERROR "You need to build static library to build zstd CLI")
    endif ()

    add_subdirectory(programs)
endif ()

if (ZSTD_BUILD_TESTS)
    if (NOT ZSTD_BUILD_STATIC)
        message(SEND_ERROR "You need to build static library to build tests")
    endif ()

    add_subdirectory(tests)
endif ()

if (ZSTD_BUILD_CONTRIB)
    add_subdirectory(contrib)
endif ()

#-----------------------------------------------------------------------------
# Add clean-all target
#-----------------------------------------------------------------------------
add_custom_target(clean-all
   COMMAND ${CMAKE_BUILD_TOOL} clean
   COMMAND rm -rf ${CMAKE_BINARY_DIR}/
)
