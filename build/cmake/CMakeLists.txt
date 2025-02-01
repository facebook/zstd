# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# As of 2018-12-26 ZSTD has been validated to build with cmake version 3.13.2 new policies.
# Set and use the newest cmake policies that are validated to work
set(ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION "3")
set(ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION "13") #Policies never changed at PATCH level
if("${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}" EQUAL "${CMAKE_MAJOR_VERSION}" AND
       "${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}" GREATER "${CMAKE_MINOR_VERSION}")
    set(ZSTD_CMAKE_POLICY_VERSION "${CMAKE_VERSION}")
else()
    set(ZSTD_CMAKE_POLICY_VERSION "${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}.${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}.0")
endif()
cmake_policy(VERSION ${ZSTD_CMAKE_POLICY_VERSION})

set(CMAKE_BUILD_WITH_INSTALL_RPATH on)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(ZSTD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
# Parse version
include(GetZstdLibraryVersion)
GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h zstd_VERSION_MAJOR zstd_VERSION_MINOR zstd_VERSION_PATCH)

set(ZSTD_SHORT_VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}")
set(ZSTD_FULL_VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")

project(zstd
  VERSION "${ZSTD_FULL_VERSION}"
  LANGUAGES C   # Main library is in C
            ASM # And ASM
            CXX # Testing contributed code also utilizes CXX
  )

message(STATUS "ZSTD VERSION: ${zstd_VERSION}")
set(zstd_HOMEPAGE_URL "https://facebook.github.io/zstd")
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
option(ZSTD_LEGACY_SUPPORT "LEGACY SUPPORT" ON)

if (ZSTD_LEGACY_SUPPORT)
    message(STATUS "ZSTD_LEGACY_SUPPORT defined!")
    set(ZSTD_LEGACY_LEVEL 5 CACHE STRING "")
    add_definitions(-DZSTD_LEGACY_SUPPORT=${ZSTD_LEGACY_LEVEL})
else ()
    message(STATUS "ZSTD_LEGACY_SUPPORT not defined!")
    add_definitions(-DZSTD_LEGACY_SUPPORT=0)
endif ()

if (APPLE)
    option(ZSTD_FRAMEWORK "Build as Apple Frameworks" OFF)
endif ()

if (ANDROID)
    set(ZSTD_MULTITHREAD_SUPPORT_DEFAULT OFF)
    # Old versions of bionic libc don't have fseeko/ftello
    if ((NOT ${ANDROID_PLATFORM_LEVEL}) OR ${ANDROID_PLATFORM_LEVEL} VERSION_LESS 24)
        message(STATUS "Setting compile definitions for old Android API")
        add_compile_definitions(LIBC_NO_FSEEKO)
    endif ()
else()
    set(ZSTD_MULTITHREAD_SUPPORT_DEFAULT ON)
endif()

# Multi-threading support
option(ZSTD_MULTITHREAD_SUPPORT "MULTITHREADING SUPPORT" ${ZSTD_MULTITHREAD_SUPPORT_DEFAULT})

if (ZSTD_MULTITHREAD_SUPPORT)
    message(STATUS "ZSTD_MULTITHREAD_SUPPORT is enabled")
else ()
    message(STATUS "ZSTD_MULTITHREAD_SUPPORT is disabled")
endif ()

option(ZSTD_BUILD_PROGRAMS "BUILD PROGRAMS" ON)
option(ZSTD_BUILD_CONTRIB "BUILD CONTRIB" OFF)

# Respect the conventional CMake option for enabling tests if it was specified on the first configure
if (BUILD_TESTING)
    set(ZSTD_BUILD_TESTS_default ON)
else()
    set(ZSTD_BUILD_TESTS_default OFF)
endif()
option(ZSTD_BUILD_TESTS "BUILD TESTS" ${ZSTD_BUILD_TESTS_default})
if (MSVC)
    option(ZSTD_USE_STATIC_RUNTIME "LINK TO STATIC RUN-TIME LIBRARIES" OFF)
endif ()

#-----------------------------------------------------------------------------
# External dependencies
#-----------------------------------------------------------------------------
# Define a function to handle special thread settings for HP-UX
# See https://github.com/facebook/zstd/pull/3862 for details.
function(setup_hpux_threads)
    find_package(Threads)
    if (NOT Threads_FOUND)
        set(CMAKE_USE_PTHREADS_INIT 1 PARENT_SCOPE)
        set(CMAKE_THREAD_LIBS_INIT -lpthread PARENT_SCOPE)
        set(CMAKE_HAVE_THREADS_LIBRARY 1 PARENT_SCOPE)
        set(Threads_FOUND TRUE PARENT_SCOPE)
    endif()
endfunction()

if (ZSTD_MULTITHREAD_SUPPORT AND UNIX)
    if (CMAKE_SYSTEM_NAME MATCHES "HP-UX")
        setup_hpux_threads()
    else()
        set(THREADS_PREFER_PTHREAD_FLAG ON)
        find_package(Threads REQUIRED)
    endif()
    if (CMAKE_USE_PTHREADS_INIT)
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
    enable_testing()
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

#-----------------------------------------------------------------------------
# Generate Package Config files
#
# This section is based on the boiler plate code from:
# https://cmake.org/cmake/help/latest/manual/cmake-packages.7.html#creating-packages
#-----------------------------------------------------------------------------
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfigVersion.cmake"
    VERSION ${zstd_VERSION}
    COMPATIBILITY SameMajorVersion
    )

# A Package Config file that works from the build directory
export(EXPORT zstdExports
    FILE "${CMAKE_CURRENT_BINARY_DIR}/zstdTargets.cmake"
    NAMESPACE zstd::
    )

# A Package Config file that works from the installation directory
set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/zstd)
install(EXPORT zstdExports
    FILE zstdTargets.cmake
    NAMESPACE zstd::
    DESTINATION ${ConfigPackageLocation}
    )
configure_package_config_file(
    zstdConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfig.cmake"
    INSTALL_DESTINATION ${ConfigPackageLocation}
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfigVersion.cmake"
    DESTINATION ${ConfigPackageLocation}
    )
