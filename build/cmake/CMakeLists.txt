# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

#-----------------------------------------------------------------------------
# Setup CMake environment
#-----------------------------------------------------------------------------
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

# Define project paths
set(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)

#-----------------------------------------------------------------------------
# Configure CMake policies and version
#-----------------------------------------------------------------------------
include(ZstdVersion)

#-----------------------------------------------------------------------------
# Project declaration
#-----------------------------------------------------------------------------
project(zstd
    VERSION "${ZSTD_FULL_VERSION}"
    LANGUAGES C   # Main library is in C and ASM, ASM is enabled conditionally
    HOMEPAGE_URL "${zstd_HOMEPAGE_URL}"
    DESCRIPTION "${zstd_DESCRIPTION}"
)

#-----------------------------------------------------------------------------
# Build type configuration
#-----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

#-----------------------------------------------------------------------------
# Include standard modules
#-----------------------------------------------------------------------------
include(GNUInstallDirs)

#-----------------------------------------------------------------------------
# Display installation information
#-----------------------------------------------------------------------------
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")

#-----------------------------------------------------------------------------
# Configure build options
#-----------------------------------------------------------------------------
include(ZstdOptions)

#-----------------------------------------------------------------------------
# Configure compilation flags
#-----------------------------------------------------------------------------
include(AddZstdCompilationFlags)
ADD_ZSTD_COMPILATION_FLAGS(ON ZSTD_ENABLE_CXX ON)

#-----------------------------------------------------------------------------
# Configure dependencies
#-----------------------------------------------------------------------------
include(ZstdDependencies)

#-----------------------------------------------------------------------------
# Configure build targets
#-----------------------------------------------------------------------------
include(ZstdBuild)

#-----------------------------------------------------------------------------
# Configure package generation
#-----------------------------------------------------------------------------
include(ZstdPackage)
