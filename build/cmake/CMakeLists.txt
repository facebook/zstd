# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

PROJECT(zstd)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.9)
SET(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Ensure Release build even if not invoked via Makefile
SET(CMAKE_BUILD_TYPE "Release")

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
INCLUDE(GNUInstallDirs)

#-----------------------------------------------------------------------------
# Add extra compilation flags
#-----------------------------------------------------------------------------
INCLUDE(AddZstdCompilationFlags)
ADD_ZSTD_COMPILATION_FLAGS()

# Always hide XXHash symbols
ADD_DEFINITIONS(-DXXH_NAMESPACE=ZSTD_)

#-----------------------------------------------------------------------------
# Installation variables
#-----------------------------------------------------------------------------
MESSAGE(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
MESSAGE(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------

# Legacy support
OPTION(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" OFF)

IF (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=4)
ELSE (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT not defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=0)
ENDIF (ZSTD_LEGACY_SUPPORT)

# Multi-threading support
OPTION(ZSTD_MULTITHREAD_SUPPORT "MULTITHREADING SUPPORT" ON)

IF (ZSTD_MULTITHREAD_SUPPORT)
    MESSAGE(STATUS "ZSTD_MULTITHREAD_SUPPORT is enabled")
ELSE (ZSTD_MULTITHREAD_SUPPORT)
    MESSAGE(STATUS "ZSTD_MULTITHREAD_SUPPORT is disabled")
ENDIF (ZSTD_MULTITHREAD_SUPPORT)

OPTION(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" ON)
OPTION(ZSTD_BUILD_CONTRIB "BUILD CONTRIB" OFF)
OPTION(ZSTD_BUILD_TESTS "BUILD TESTS" OFF)
if (MSVC)
    OPTION(ZSTD_USE_STATIC_RUNTIME "LINK TO STATIC RUN-TIME LIBRARIES" OFF)
endif ()

#-----------------------------------------------------------------------------
# External dependencies
#-----------------------------------------------------------------------------
IF (ZSTD_MULTITHREAD_SUPPORT AND UNIX)
    SET(THREADS_PREFER_PTHREAD_FLAG ON)
    FIND_PACKAGE(Threads REQUIRED)
    IF(CMAKE_USE_PTHREADS_INIT)
        SET(THREADS_LIBS "${CMAKE_THREAD_LIBS_INIT}")
    ELSE()
        MESSAGE(SEND_ERROR "ZSTD currently does not support thread libraries other than pthreads")
    ENDIF()
ENDIF (ZSTD_MULTITHREAD_SUPPORT AND UNIX)

#-----------------------------------------------------------------------------
# Add source directories
#-----------------------------------------------------------------------------
ADD_SUBDIRECTORY(lib)

IF (ZSTD_BUILD_PROGRAMS)
    IF (NOT ZSTD_BUILD_STATIC)
        MESSAGE(SEND_ERROR "You need to build static library to build zstd CLI")
    ENDIF (NOT ZSTD_BUILD_STATIC)

    ADD_SUBDIRECTORY(programs)
ENDIF (ZSTD_BUILD_PROGRAMS)

IF (ZSTD_BUILD_TESTS)
    IF (NOT ZSTD_BUILD_STATIC)
        MESSAGE(SEND_ERROR "You need to build static library to build tests")
    ENDIF (NOT ZSTD_BUILD_STATIC)

    ADD_SUBDIRECTORY(tests)
ENDIF (ZSTD_BUILD_TESTS)

IF (ZSTD_BUILD_CONTRIB)
    ADD_SUBDIRECTORY(contrib)
ENDIF (ZSTD_BUILD_CONTRIB)

#-----------------------------------------------------------------------------
# Add clean-all target
#-----------------------------------------------------------------------------
ADD_CUSTOM_TARGET(clean-all
   COMMAND ${CMAKE_BUILD_TOOL} clean
   COMMAND rm -rf ${CMAKE_BINARY_DIR}/
)
