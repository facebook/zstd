# ################################################################
# * Copyright (c) 2014-present, Yann Collet, Facebook, Inc.
# * All rights reserved.
# *
# * This source code is licensed under the BSD-style license found in the
# * LICENSE file in the root directory of this source tree. An additional grant
# * of patent rights can be found in the PATENTS file in the same directory.
#
# You can contact the author at :
#  - zstd homepage : http://www.zstd.net/
# ################################################################

PROJECT(libzstd)
INCLUDE(${CMAKE_SOURCE_DIR}/CMakeModules/GetLibraryVersion.cmake)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define library directory, where sources and header files are located
SET(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
INCLUDE_DIRECTORIES(${LIBRARY_DIR} ${LIBRARY_DIR}/common)

# Parse version
GetLibraryVersion(${LIBRARY_DIR}/zstd.h LIBVER_MAJOR LIBVER_MINOR LIBVER_RELEASE)
MESSAGE("ZSTD VERSION ${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE}")

SET(Sources
        ${LIBRARY_DIR}/common/entropy_common.c
        ${LIBRARY_DIR}/common/fse_decompress.c
        ${LIBRARY_DIR}/common/threading.c
        ${LIBRARY_DIR}/common/pool.c
        ${LIBRARY_DIR}/common/zstd_common.c
        ${LIBRARY_DIR}/common/error_private.c
        ${LIBRARY_DIR}/common/xxhash.c
        ${LIBRARY_DIR}/compress/fse_compress.c
        ${LIBRARY_DIR}/compress/huf_compress.c
        ${LIBRARY_DIR}/compress/zstd_compress.c
        ${LIBRARY_DIR}/compress/zstdmt_compress.c
        ${LIBRARY_DIR}/decompress/huf_decompress.c
        ${LIBRARY_DIR}/decompress/zstd_decompress.c
        ${LIBRARY_DIR}/dictBuilder/cover.c
        ${LIBRARY_DIR}/dictBuilder/divsufsort.c
        ${LIBRARY_DIR}/dictBuilder/zdict.c
        ${LIBRARY_DIR}/deprecated/zbuff_common.c
        ${LIBRARY_DIR}/deprecated/zbuff_compress.c
        ${LIBRARY_DIR}/deprecated/zbuff_decompress.c)

SET(Headers
        ${LIBRARY_DIR}/zstd.h
        ${LIBRARY_DIR}/common/pool.h
        ${LIBRARY_DIR}/common/threading.h
        ${LIBRARY_DIR}/common/bitstream.h
        ${LIBRARY_DIR}/common/error_private.h
        ${LIBRARY_DIR}/common/zstd_errors.h
        ${LIBRARY_DIR}/common/fse.h
        ${LIBRARY_DIR}/common/huf.h
        ${LIBRARY_DIR}/common/mem.h
        ${LIBRARY_DIR}/common/zstd_internal.h
        ${LIBRARY_DIR}/compress/zstdmt_compress.h
        ${LIBRARY_DIR}/dictBuilder/zdict.h
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
ADD_LIBRARY(libzstd_static STATIC ${Sources} ${Headers})
ADD_LIBRARY(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})

# Add specific compile definitions for MSVC project
IF (MSVC)
    SET_TARGET_PROPERTIES(libzstd_static PROPERTIES COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    SET_TARGET_PROPERTIES(libzstd_shared PROPERTIES COMPILE_DEFINITIONS "ZSTD_DLL_EXPORT=1;ZSTD_HEAPMODE=0;_CONSOLE;_CRT_SECURE_NO_WARNINGS")
ENDIF (MSVC)

# Define library base name
IF (MSVC)
    SET(LIBRARY_BASE_NAME zstdlib)
ELSE ()
    SET(LIBRARY_BASE_NAME libzstd)
ENDIF (MSVC)

IF (MSVC)
    IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET(LIBRARY_ARCH_SUFFIX "_x64")
    ELSE ()
        SET(LIBRARY_ARCH_SUFFIX "_x86")
    ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")
ELSE ()
    SET(LIBRARY_ARCH_SUFFIX "")
ENDIF (MSVC)

# Define static and shared library names
SET(STATIC_LIBRARY_OUTPUT_NAME ${LIBRARY_BASE_NAME}${LIBRARY_ARCH_SUFFIX} CACHE STRING "Static library output name")
SET(SHARED_LIBRARY_OUTPUT_NAME ${LIBRARY_BASE_NAME}.${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE}${LIBRARY_ARCH_SUFFIX} CACHE STRING "Shared library output name")

SET_TARGET_PROPERTIES(
        libzstd_static
        PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${STATIC_LIBRARY_OUTPUT_NAME})

SET_TARGET_PROPERTIES(
        libzstd_shared
        PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${SHARED_LIBRARY_OUTPUT_NAME})

IF (UNIX)
    IF ("${PREFIX}" STREQUAL "")
        SET(PREFIX /usr/local)
    ENDIF()
    MESSAGE("the variable PREFIX=${PREFIX}")
    SET(INSTALL_LIBRARY_DIR ${PREFIX}/lib)
    SET(INSTALL_INCLUDE_DIR ${PREFIX}/include)

    # install target
    INSTALL(FILES ${LIBRARY_DIR}/zstd.h ${LIBRARY_DIR}/deprecated/zbuff.h ${LIBRARY_DIR}/dictBuilder/zdict.h DESTINATION ${INSTALL_INCLUDE_DIR})
    INSTALL(TARGETS libzstd_static DESTINATION ${INSTALL_LIBRARY_DIR})
    INSTALL(TARGETS libzstd_shared LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR})

    # Create symlinks and setup this files
    SET(SHARED_LIBRARY_LINK ${SHARED_LIBRARY_OUTPUT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    SET(SHARED_LIBRARY_SYMLINK1 ${LIBRARY_BASE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    SET(SHARED_LIBRARY_SYMLINK2 ${LIBRARY_BASE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${LIBVER_MAJOR})

    SET(SHARED_LIBRARY_LINK_PATH ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIBRARY_LINK})
    SET(SHARED_LIBRARY_SYMLINK1_PATH ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIBRARY_SYMLINK1})
    SET(SHARED_LIBRARY_SYMLINK2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIBRARY_SYMLINK2})

    ADD_CUSTOM_COMMAND(TARGET libzstd_shared POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${SHARED_LIBRARY_LINK} ${SHARED_LIBRARY_SYMLINK1}
            DEPENDS  ${SHARED_LIBRARY_LINK_PATH}
            COMMENT "Generating symbolic link ${SHARED_LIBRARY_LINK} -> ${SHARED_LIBRARY_SYMLINK1}")

    ADD_CUSTOM_COMMAND(TARGET libzstd_shared POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${SHARED_LIBRARY_LINK} ${SHARED_LIBRARY_SYMLINK2}
            DEPENDS  ${SHARED_LIBRARY_LINK_PATH}
            COMMENT "Generating symbolic link ${SHARED_LIBRARY_LINK} -> ${SHARED_LIBRARY_SYMLINK2}")

    SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${SHARED_LIBRARY_SYMLINK1};${SHARED_LIBRARY_SYMLINK2}")

    INSTALL(FILES ${SHARED_LIBRARY_SYMLINK1_PATH} DESTINATION ${INSTALL_LIBRARY_DIR})
    INSTALL(FILES ${SHARED_LIBRARY_SYMLINK2_PATH} DESTINATION ${INSTALL_LIBRARY_DIR})

    # uninstall target
    CONFIGURE_FILE(
            "${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in"
            "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)

    ADD_CUSTOM_TARGET(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake)
ENDIF (UNIX)
