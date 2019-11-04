# ################################################################
# Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

cmake_minimum_required(VERSION 2.8.9 FATAL_ERROR)
  
# As of 2018-12-26 ZSTD has been validated to build with cmake version 3.13.2 new policies. 
# Set and use the newest cmake policies that are validated to work 
set(ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION "3") 
set(ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION "13") #Policies never changed at PATCH level
if("${CMAKE_MAJOR_VERSION}" LESS 3)
  set(ZSTD_CMAKE_POLICY_VERSION "${CMAKE_VERSION}") 
elseif( "${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}" EQUAL "${CMAKE_MAJOR_VERSION}" AND 
        "${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}" GREATER "${CMAKE_MINOR_VERSION}")
    set(ZSTD_CMAKE_POLICY_VERSION "${CMAKE_VERSION}") 
else() 
    set(ZSTD_CMAKE_POLICY_VERSION "${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}.${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}.0") 
endif()
cmake_policy(VERSION ${ZSTD_CMAKE_POLICY_VERSION})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
# Parse version
include(GetZstdLibraryVersion)
GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h zstd_VERSION_MAJOR zstd_VERSION_MINOR zstd_VERSION_PATCH)

if( CMAKE_MAJOR_VERSION LESS 3 )
  ## Provide cmake 3+ behavior for older versions of cmake
  project(zstd)
  set(PROJECT_VERSION_MAJOR ${zstd_VERSION_MAJOR})
  set(PROJECT_VERSION_MINOR ${zstd_VERSION_MINOR})
  set(PROJECT_VERSION_PATCH ${zstd_VERSION_PATCH})
  set(PROJECT_VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")
  enable_language(C)   # Main library is in C
  enable_language(CXX) # Testing contributed code also utilizes CXX
else()
  project(zstd
    VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}"
    LANGUAGES C   # Main library is in C
              CXX # Testing contributed code also utilizes CXX
    )
endif()
message(STATUS "ZSTD VERSION: ${zstd_VERSION}")
set(zstd_HOMEPAGE_URL "http://www.zstd.net")
set(zstd_DESCRIPTION  "Zstandard is a real-time compression algorithm, providing high compression ratios.")

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

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

option(ZSTD_PROGRAMS_LINK_SHARED "PROGRAMS LINK SHARED" OFF)

if (ZSTD_BUILD_PROGRAMS)
    if (NOT ZSTD_BUILD_STATIC AND NOT ZSTD_PROGRAMS_LINK_SHARED)
        message(SEND_ERROR "You need to build static library to build zstd CLI")
    elseif(NOT ZSTD_BUILD_SHARED AND ZSTD_PROGRAMS_LINK_SHARED)
        message(SEND_ERROR "You need to build shared library to build zstd CLI")
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
