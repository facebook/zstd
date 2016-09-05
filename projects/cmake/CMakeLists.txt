# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
# ################################################################

PROJECT(zstd)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.7)

OPTION(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" OFF)

IF (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=1)
ELSE (ZSTD_LEGACY_SUPPORT)
    MESSAGE(STATUS "ZSTD_LEGACY_SUPPORT not defined!")
    ADD_DEFINITIONS(-DZSTD_LEGACY_SUPPORT=0)
ENDIF (ZSTD_LEGACY_SUPPORT)

ADD_SUBDIRECTORY(lib)
ADD_SUBDIRECTORY(programs)
ADD_SUBDIRECTORY(tests)

#-----------------------------------------------------------------------------
# Add extra compilation flags
#-----------------------------------------------------------------------------
INCLUDE(CMakeModules/AddExtraCompilationFlags.cmake)
ADD_EXTRA_COMPILATION_FLAGS()

ADD_CUSTOM_TARGET(clean-all
   COMMAND ${CMAKE_BUILD_TOOL} clean
   COMMAND rm -rf ${CMAKE_BINARY_DIR}/
)
