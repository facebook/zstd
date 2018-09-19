# ################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

PROJECT(libzstd)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)
OPTION(ZSTD_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
OPTION(ZSTD_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)

IF(NOT ZSTD_BUILD_SHARED AND NOT ZSTD_BUILD_STATIC)
    MESSAGE(SEND_ERROR "You need to build at least one flavor of libzstd")
ENDIF()

# Define library directory, where sources and header files are located
SET(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
INCLUDE_DIRECTORIES(${LIBRARY_DIR} ${LIBRARY_DIR}/common)

# Parse version
INCLUDE(GetZstdLibraryVersion)
GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h LIBVER_MAJOR LIBVER_MINOR LIBVER_RELEASE)
MESSAGE(STATUS "ZSTD VERSION ${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE}")

SET(Sources
        ${LIBRARY_DIR}/common/entropy_common.c
        ${LIBRARY_DIR}/common/fse_decompress.c
        ${LIBRARY_DIR}/common/threading.c
        ${LIBRARY_DIR}/common/pool.c
        ${LIBRARY_DIR}/common/zstd_common.c
        ${LIBRARY_DIR}/common/error_private.c
        ${LIBRARY_DIR}/common/xxhash.c
        ${LIBRARY_DIR}/compress/hist.c
        ${LIBRARY_DIR}/compress/fse_compress.c
        ${LIBRARY_DIR}/compress/huf_compress.c
        ${LIBRARY_DIR}/compress/zstd_compress.c
        ${LIBRARY_DIR}/compress/zstdmt_compress.c
        ${LIBRARY_DIR}/compress/zstd_fast.c
        ${LIBRARY_DIR}/compress/zstd_double_fast.c
        ${LIBRARY_DIR}/compress/zstd_lazy.c
        ${LIBRARY_DIR}/compress/zstd_opt.c
        ${LIBRARY_DIR}/compress/zstd_ldm.c
        ${LIBRARY_DIR}/decompress/huf_decompress.c
        ${LIBRARY_DIR}/decompress/zstd_decompress.c
        ${LIBRARY_DIR}/dictBuilder/cover.c
        ${LIBRARY_DIR}/dictBuilder/fastcover.c
        ${LIBRARY_DIR}/dictBuilder/divsufsort.c
        ${LIBRARY_DIR}/dictBuilder/zdict.c
        ${LIBRARY_DIR}/deprecated/zbuff_common.c
        ${LIBRARY_DIR}/deprecated/zbuff_compress.c
        ${LIBRARY_DIR}/deprecated/zbuff_decompress.c)

SET(Headers
        ${LIBRARY_DIR}/zstd.h
        ${LIBRARY_DIR}/common/debug.h
        ${LIBRARY_DIR}/common/pool.h
        ${LIBRARY_DIR}/common/threading.h
        ${LIBRARY_DIR}/common/bitstream.h
        ${LIBRARY_DIR}/common/error_private.h
        ${LIBRARY_DIR}/common/zstd_errors.h
        ${LIBRARY_DIR}/common/fse.h
        ${LIBRARY_DIR}/common/huf.h
        ${LIBRARY_DIR}/common/mem.h
        ${LIBRARY_DIR}/common/zstd_internal.h
        ${LIBRARY_DIR}/compress/hist.h
        ${LIBRARY_DIR}/compress/zstd_compress_internal.h
        ${LIBRARY_DIR}/compress/zstd_fast.h
        ${LIBRARY_DIR}/compress/zstd_double_fast.h
        ${LIBRARY_DIR}/compress/zstd_lazy.h
        ${LIBRARY_DIR}/compress/zstd_opt.h
        ${LIBRARY_DIR}/compress/zstd_ldm.h
        ${LIBRARY_DIR}/compress/zstdmt_compress.h
        ${LIBRARY_DIR}/dictBuilder/zdict.h
        ${LIBRARY_DIR}/dictBuilder/cover.h
        ${LIBRARY_DIR}/deprecated/zbuff.h)

IF (ZSTD_LEGACY_SUPPORT)
    SET(LIBRARY_LEGACY_DIR ${LIBRARY_DIR}/legacy)
    INCLUDE_DIRECTORIES(${LIBRARY_LEGACY_DIR})

    SET(Sources ${Sources}
            ${LIBRARY_LEGACY_DIR}/zstd_v01.c
            ${LIBRARY_LEGACY_DIR}/zstd_v02.c
            ${LIBRARY_LEGACY_DIR}/zstd_v03.c
            ${LIBRARY_LEGACY_DIR}/zstd_v04.c
            ${LIBRARY_LEGACY_DIR}/zstd_v05.c
            ${LIBRARY_LEGACY_DIR}/zstd_v06.c
            ${LIBRARY_LEGACY_DIR}/zstd_v07.c)

    SET(Headers ${Headers}
            ${LIBRARY_LEGACY_DIR}/zstd_legacy.h
            ${LIBRARY_LEGACY_DIR}/zstd_v01.h
            ${LIBRARY_LEGACY_DIR}/zstd_v02.h
            ${LIBRARY_LEGACY_DIR}/zstd_v03.h
            ${LIBRARY_LEGACY_DIR}/zstd_v04.h
            ${LIBRARY_LEGACY_DIR}/zstd_v05.h
            ${LIBRARY_LEGACY_DIR}/zstd_v06.h
            ${LIBRARY_LEGACY_DIR}/zstd_v07.h)
ENDIF (ZSTD_LEGACY_SUPPORT)

IF (MSVC)
    SET(MSVC_RESOURCE_DIR ${ZSTD_SOURCE_DIR}/build/VS2010/libzstd-dll)
    SET(PlatformDependResources ${MSVC_RESOURCE_DIR}/libzstd-dll.rc)
ENDIF (MSVC)

# Split project to static and shared libraries build
IF (ZSTD_BUILD_SHARED)
    ADD_LIBRARY(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})
    IF (ZSTD_MULTITHREAD_SUPPORT)
        SET_PROPERTY(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        IF (UNIX)
            TARGET_LINK_LIBRARIES(libzstd_shared ${THREADS_LIBS})
        ENDIF ()
    ENDIF()
ENDIF (ZSTD_BUILD_SHARED)
IF (ZSTD_BUILD_STATIC)
    ADD_LIBRARY(libzstd_static STATIC ${Sources} ${Headers})
    IF (ZSTD_MULTITHREAD_SUPPORT)
        SET_PROPERTY(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        IF (UNIX)
            TARGET_LINK_LIBRARIES(libzstd_static ${THREADS_LIBS})
        ENDIF ()
    ENDIF ()
ENDIF (ZSTD_BUILD_STATIC)

# Add specific compile definitions for MSVC project
IF (MSVC)
    IF (ZSTD_BUILD_SHARED)
        SET_PROPERTY(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_DLL_EXPORT=1;ZSTD_HEAPMODE=0;_CONSOLE;_CRT_SECURE_NO_WARNINGS")
    ENDIF (ZSTD_BUILD_SHARED)
    IF (ZSTD_BUILD_STATIC)
        SET_PROPERTY(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    ENDIF (ZSTD_BUILD_STATIC)
ENDIF (MSVC)

# With MSVC static library needs to be renamed to avoid conflict with import library
IF (MSVC)
    SET(STATIC_LIBRARY_BASE_NAME zstd_static)
ELSE ()
    SET(STATIC_LIBRARY_BASE_NAME zstd)
ENDIF (MSVC)

# Define static and shared library names
IF (ZSTD_BUILD_SHARED)
    SET_TARGET_PROPERTIES(
            libzstd_shared
            PROPERTIES
            OUTPUT_NAME zstd
            SOVERSION ${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE})
ENDIF (ZSTD_BUILD_SHARED)

IF (ZSTD_BUILD_STATIC)
    SET_TARGET_PROPERTIES(
            libzstd_static
            PROPERTIES
            OUTPUT_NAME ${STATIC_LIBRARY_BASE_NAME})
ENDIF (ZSTD_BUILD_STATIC)

IF (UNIX)
    # pkg-config
    SET(PREFIX "${CMAKE_INSTALL_PREFIX}")
    SET(LIBDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    SET(INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")
    SET(VERSION "${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE}")
    ADD_CUSTOM_TARGET(libzstd.pc ALL
            ${CMAKE_COMMAND} -DIN="${LIBRARY_DIR}/libzstd.pc.in" -DOUT="libzstd.pc"
            -DPREFIX="${PREFIX}" -DLIBDIR="${LIBDIR}" -DINCLUDEDIR="${INCLUDEDIR}" -DVERSION="${VERSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig.cmake"
            COMMENT "Creating pkg-config file")

    INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/libzstd.pc" DESTINATION "${LIBDIR}/pkgconfig")
ENDIF (UNIX)

# install target
INSTALL(FILES
    ${LIBRARY_DIR}/zstd.h
    ${LIBRARY_DIR}/deprecated/zbuff.h
    ${LIBRARY_DIR}/dictBuilder/zdict.h
    ${LIBRARY_DIR}/dictBuilder/cover.h
    ${LIBRARY_DIR}/common/zstd_errors.h
    DESTINATION "include")

IF (ZSTD_BUILD_SHARED)
    INSTALL(TARGETS libzstd_shared RUNTIME DESTINATION "bin"
                                   LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                                   ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
ENDIF()
IF (ZSTD_BUILD_STATIC)
    INSTALL(TARGETS libzstd_static ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
ENDIF (ZSTD_BUILD_STATIC)

# uninstall target
CONFIGURE_FILE(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
