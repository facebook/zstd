# ################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

project(libzstd)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
option(ZSTD_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
option(ZSTD_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)

if(NOT ZSTD_BUILD_SHARED AND NOT ZSTD_BUILD_STATIC)
    message(SEND_ERROR "You need to build at least one flavor of libzstd")
endif()

# Define library directory, where sources and header files are located
include_directories(${LIBRARY_DIR} ${LIBRARY_DIR}/common)

file(GLOB CommonSources ${LIBRARY_DIR}/common/*.c)
file(GLOB CompressSources ${LIBRARY_DIR}/compress/*.c)
file(GLOB DecompressSources ${LIBRARY_DIR}/decompress/*.c)
file(GLOB DictBuilderSources ${LIBRARY_DIR}/dictBuilder/*.c)
file(GLOB DeprecatedSources ${LIBRARY_DIR}/deprecated/*.c)

set(Sources
        ${CommonSources}
        ${CompressSources}
        ${DecompressSources}
        ${DictBuilderSources}
        ${DeprecatedSources})

file(GLOB CommonHeaders ${LIBRARY_DIR}/common/*.h)
file(GLOB CompressHeaders ${LIBRARY_DIR}/compress/*.h)
file(GLOB DecompressHeaders ${LIBRARY_DIR}/decompress/*.h)
file(GLOB DictBuilderHeaders ${LIBRARY_DIR}/dictBuilder/*.h)
file(GLOB DeprecatedHeaders ${LIBRARY_DIR}/deprecated/*.h)

set(Headers
        ${LIBRARY_DIR}/zstd.h
        ${CommonHeaders}
        ${CompressHeaders}
        ${DecompressHeaders}
        ${DictBuilderHeaders}
        ${DeprecatedHeaders})

if (ZSTD_LEGACY_SUPPORT)
    set(LIBRARY_LEGACY_DIR ${LIBRARY_DIR}/legacy)
    include_directories(${LIBRARY_LEGACY_DIR})

    set(Sources ${Sources}
            ${LIBRARY_LEGACY_DIR}/zstd_v01.c
            ${LIBRARY_LEGACY_DIR}/zstd_v02.c
            ${LIBRARY_LEGACY_DIR}/zstd_v03.c
            ${LIBRARY_LEGACY_DIR}/zstd_v04.c
            ${LIBRARY_LEGACY_DIR}/zstd_v05.c
            ${LIBRARY_LEGACY_DIR}/zstd_v06.c
            ${LIBRARY_LEGACY_DIR}/zstd_v07.c)

    set(Headers ${Headers}
            ${LIBRARY_LEGACY_DIR}/zstd_legacy.h
            ${LIBRARY_LEGACY_DIR}/zstd_v01.h
            ${LIBRARY_LEGACY_DIR}/zstd_v02.h
            ${LIBRARY_LEGACY_DIR}/zstd_v03.h
            ${LIBRARY_LEGACY_DIR}/zstd_v04.h
            ${LIBRARY_LEGACY_DIR}/zstd_v05.h
            ${LIBRARY_LEGACY_DIR}/zstd_v06.h
            ${LIBRARY_LEGACY_DIR}/zstd_v07.h)
endif ()

if (MSVC)
    set(MSVC_RESOURCE_DIR ${ZSTD_SOURCE_DIR}/build/VS2010/libzstd-dll)
    set(PlatformDependResources ${MSVC_RESOURCE_DIR}/libzstd-dll.rc)
endif ()

# Split project to static and shared libraries build
if (ZSTD_BUILD_SHARED)
    add_library(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})
    if (ZSTD_MULTITHREAD_SUPPORT)
        set_property(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        if (UNIX)
            target_link_libraries(libzstd_shared ${THREADS_LIBS})
        endif ()
    endif()
endif ()
if (ZSTD_BUILD_STATIC)
    add_library(libzstd_static STATIC ${Sources} ${Headers})
    if (ZSTD_MULTITHREAD_SUPPORT)
        set_property(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        if (UNIX)
            target_link_libraries(libzstd_static ${THREADS_LIBS})
        endif ()
    endif ()
endif ()

# Add specific compile definitions for MSVC project
if (MSVC)
    if (ZSTD_BUILD_SHARED)
        set_property(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_DLL_EXPORT=1;ZSTD_HEAPMODE=0;_CONSOLE;_CRT_SECURE_NO_WARNINGS")
    endif ()
    if (ZSTD_BUILD_STATIC)
        set_property(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    endif ()
endif ()

# With MSVC static library needs to be renamed to avoid conflict with import library
if (MSVC)
    set(STATIC_LIBRARY_BASE_NAME zstd_static)
else ()
    set(STATIC_LIBRARY_BASE_NAME zstd)
endif ()

# Define static and shared library names
if (ZSTD_BUILD_SHARED)
    set_target_properties(
            libzstd_shared
            PROPERTIES
            OUTPUT_NAME zstd
            VERSION ${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}
            SOVERSION ${zstd_VERSION_MAJOR})
endif ()

if (ZSTD_BUILD_STATIC)
    set_target_properties(
            libzstd_static
            PROPERTIES
            OUTPUT_NAME ${STATIC_LIBRARY_BASE_NAME})
endif ()

if (UNIX)
    # pkg-config
    set(PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(LIBDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    set(INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")
    set(VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")
    add_custom_target(libzstd.pc ALL
            ${CMAKE_COMMAND} -DIN="${LIBRARY_DIR}/libzstd.pc.in" -DOUT="libzstd.pc"
            -DPREFIX="${PREFIX}" -DLIBDIR="${LIBDIR}" -DINCLUDEDIR="${INCLUDEDIR}" -DVERSION="${VERSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig.cmake"
            COMMENT "Creating pkg-config file")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libzstd.pc" DESTINATION "${LIBDIR}/pkgconfig")
endif ()

# install target
install(FILES
    ${LIBRARY_DIR}/zstd.h
    ${LIBRARY_DIR}/deprecated/zbuff.h
    ${LIBRARY_DIR}/dictBuilder/zdict.h
    ${LIBRARY_DIR}/dictBuilder/cover.h
    ${LIBRARY_DIR}/common/zstd_errors.h
    DESTINATION "include")

if (ZSTD_BUILD_SHARED)
    install(TARGETS libzstd_shared RUNTIME DESTINATION "bin"
                                   LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                                   ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()
if (ZSTD_BUILD_STATIC)
    install(TARGETS libzstd_static ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif ()

# uninstall target
if (NOT TARGET uninstall)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)

    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif ()
