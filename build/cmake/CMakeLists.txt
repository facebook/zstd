# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

PROJECT(zstd)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.9)
SET(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

#-----------------------------------------------------------------------------
# Add extra compilation flags
#-----------------------------------------------------------------------------
INCLUDE(AddZstdCompilationFlags)
ADD_ZSTD_COMPILATION_FLAGS()

# Always hide XXHash symbols
ADD_DEFINITIONS(-DXXH_NAMESPACE=ZSTD_)

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------
OPTION(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" OFF)
IF (UNIX)
    OPTION(ZSTD_MULTITHREAD_SUPPORT "MULTITHREADING SUPPORT" ON)
ELSE (UNIX)
    OPTION(ZSTD_MULTITHREAD_SUPPORT "MULTITHREADING SUPPORT" OFF)
ENDIF (UNIX)
OPTION(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" ON)
OPTION(ZSTD_BUILD_CONTRIB "BUILD CONTRIB" OFF)
OPTION(ZSTD_BUILD_TESTS "BUILD TESTS" OFF)
if (MSVC)
    OPTION(ZSTD_USE_STATIC_RUNTIME "LINK TO STATIC RUN-TIME LIBRARIES" OFF)
endif ()

IF (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=4)
ELSE (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT not defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=0)
ENDIF (ZSTD_LEGACY_SUPPORT)

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
