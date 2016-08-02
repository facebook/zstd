# ################################################################
# zstd - Makefile
# Copyright (C) Yann Collet 2014-2016
# All rights reserved.
# 
# BSD license
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright notice, this
#   list of conditions and the following disclaimer in the documentation and/or
#   other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# You can contact the author at :
#  - zstd homepage : http://www.zstd.net/
# ################################################################

# Get library version based on information from input content (use regular exp)
function(GetLibraryVersion _content _outputVar1 _outputVar2 _outputVar3)
    string(REGEX MATCHALL ".*define ZSTD_VERSION_MAJOR+.* ([0-9]+).*define ZSTD_VERSION_MINOR+.* ([0-9]+).*define ZSTD_VERSION_RELEASE+.* ([0-9]+)" VERSION_REGEX "${_content}")
    SET(${_outputVar1} ${CMAKE_MATCH_1} PARENT_SCOPE)
    SET(${_outputVar2} ${CMAKE_MATCH_2} PARENT_SCOPE)
    SET(${_outputVar3} ${CMAKE_MATCH_3} PARENT_SCOPE)
endfunction()

PROJECT(libzstd)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define project root directory
SET(ROOT_DIR ../../..)

# Define library directory, where sources and header files are located
SET(LIBRARY_DIR ${ROOT_DIR}/lib)
INCLUDE_DIRECTORIES(${LIBRARY_DIR}/common)

# Read file content
FILE(READ ${LIBRARY_DIR}/common/zstd.h HEADER_CONTENT)

# Parse version
GetLibraryVersion("${HEADER_CONTENT}" LIBVER_MAJOR LIBVER_MINOR LIBVER_RELEASE)
MESSAGE("ZSTD VERSION ${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE}")

SET(Sources
        ${LIBRARY_DIR}/common/entropy_common.c
        ${LIBRARY_DIR}/common/zstd_common.c
        ${LIBRARY_DIR}/common/fse_decompress.c
        ${LIBRARY_DIR}/compress/fse_compress.c
        ${LIBRARY_DIR}/compress/huf_compress.c
        ${LIBRARY_DIR}/compress/zbuff_compress.c
        ${LIBRARY_DIR}/compress/zstd_compress.c
        ${LIBRARY_DIR}/decompress/huf_decompress.c
        ${LIBRARY_DIR}/decompress/zbuff_decompress.c
        ${LIBRARY_DIR}/decompress/zstd_decompress.c
        ${LIBRARY_DIR}/dictBuilder/divsufsort.c
        ${LIBRARY_DIR}/dictBuilder/zdict.c)

SET(Headers
        ${LIBRARY_DIR}/common/bitstream.h
        ${LIBRARY_DIR}/common/error_private.h
        ${LIBRARY_DIR}/common/error_public.h
        ${LIBRARY_DIR}/common/fse.h
        ${LIBRARY_DIR}/common/fse_static.h
        ${LIBRARY_DIR}/common/huf.h
        ${LIBRARY_DIR}/common/huf_static.h
        ${LIBRARY_DIR}/common/mem.h
        ${LIBRARY_DIR}/common/zbuff.h
        ${LIBRARY_DIR}/common/zbuff_static.h
        ${LIBRARY_DIR}/common/zstd_internal.h
        ${LIBRARY_DIR}/common/zstd_static.h
        ${LIBRARY_DIR}/common/zstd.h
        ${LIBRARY_DIR}/dictBuilder/zdict.h
        ${LIBRARY_DIR}/dictBuilder/zdict_static.h)

IF (ZSTD_LEGACY_SUPPORT)
    SET(LIBRARY_LEGACY_DIR ${LIBRARY_DIR}/legacy)
    INCLUDE_DIRECTORIES(${LIBRARY_LEGACY_DIR})

    SET(Sources ${Sources}
            ${LIBRARY_LEGACY_DIR}/zstd_v01.c
            ${LIBRARY_LEGACY_DIR}/zstd_v02.c
            ${LIBRARY_LEGACY_DIR}/zstd_v03.c
            ${LIBRARY_LEGACY_DIR}/zstd_v04.c
            ${LIBRARY_LEGACY_DIR}/zstd_v05.c
            ${LIBRARY_LEGACY_DIR}/zstd_v07.c
            ${LIBRARY_LEGACY_DIR}/zstd_v08.c)

    SET(Headers ${Headers}
            ${LIBRARY_LEGACY_DIR}/zstd_legacy.h
            ${LIBRARY_LEGACY_DIR}/zstd_v01.h
            ${LIBRARY_LEGACY_DIR}/zstd_v02.h
            ${LIBRARY_LEGACY_DIR}/zstd_v03.h
            ${LIBRARY_LEGACY_DIR}/zstd_v04.h
            ${LIBRARY_LEGACY_DIR}/zstd_v05.h
            ${LIBRARY_LEGACY_DIR}/zstd_v07.h
            ${LIBRARY_LEGACY_DIR}/zstd_v08.h)
ENDIF (ZSTD_LEGACY_SUPPORT)

IF (MSVC)
    SET(MSVC_RESOURCE_DIR ${ROOT_DIR}/visual/2013/zstdlib)
    SET(PlatformDependResources ${MSVC_RESOURCE_DIR}/resource.h ${MSVC_RESOURCE_DIR}/zstdlib.rc)
ENDIF (MSVC)

# Split project to static and shared libraries build
ADD_LIBRARY(libzstd_static STATIC ${Sources} ${Headers} ${PlatformDependResources})
ADD_LIBRARY(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})

# Add specific compile definitions for MSVC project
IF (MSVC)
    SET_TARGET_PROPERTIES(libzstd_static PROPERTIES COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    SET_TARGET_PROPERTIES(libzstd_shared PROPERTIES COMPILE_DEFINITIONS "ZSTD_DLL_EXPORT=1;ZSTD_HEAPMODE=0;_CONSOLE;_CRT_SECURE_NO_WARNINGS")
ENDIF (MSVC)

# Define include directories
IF (NOT WORKAROUND_OUTDATED_CODE_STYLE)
    TARGET_INCLUDE_DIRECTORIES(libzstd_static PUBLIC ${LIBRARY_DIR}/common)
    TARGET_INCLUDE_DIRECTORIES(libzstd_shared PUBLIC ${LIBRARY_DIR}/common)
    IF (ZSTD_LEGACY_SUPPORT)
        TARGET_INCLUDE_DIRECTORIES(libzstd_static PUBLIC ${LIBRARY_LEGACY_DIR})
        TARGET_INCLUDE_DIRECTORIES(libzstd_shared PUBLIC ${LIBRARY_LEGACY_DIR})
    ENDIF (ZSTD_LEGACY_SUPPORT)
ENDIF (NOT WORKAROUND_OUTDATED_CODE_STYLE)

# Define library base name
IF (UNIX)
    SET(LIBRARY_BASE_NAME libzstd)
ELSEIF (MSVC)
    SET(LIBRARY_BASE_NAME zstdlib)
ELSE ()
    MESSAGE(FATAL_ERROR "Unsupported build type")
ENDIF (UNIX)

# Define static and shared library names
SET(STATIC_LIBRARY_OUTPUT_NAME ${LIBRARY_BASE_NAME})
SET(SHARED_LIBRARY_OUTPUT_NAME ${LIBRARY_BASE_NAME}.${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE})

IF (MSVC)
    IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET(STATIC_LIBRARY_OUTPUT_NAME ${STATIC_LIBRARY_OUTPUT_NAME}_x64)
        SET(SHARED_LIBRARY_OUTPUT_NAME ${SHARED_LIBRARY_OUTPUT_NAME}_x64)
    ELSE ()
        SET(STATIC_LIBRARY_OUTPUT_NAME ${STATIC_LIBRARY_OUTPUT_NAME}_x86)
        SET(SHARED_LIBRARY_OUTPUT_NAME ${SHARED_LIBRARY_OUTPUT_NAME}_x86)
    ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")
ENDIF (MSVC)

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
    SET(PREFIX /usr/local)
    SET(INSTALL_LIBRARY_DIR ${PREFIX}/lib)
    SET(INSTALL_INCLUDE_DIR ${PREFIX}/include)

    # install target
    INSTALL(FILES ${LIBRARY_DIR}/zstd.h ${LIBRARY_DIR}/zstd_buffered.h ${LIBRARY_DIR}/dictBuilder.h DESTINATION ${INSTALL_INCLUDE_DIR})
    INSTALL(TARGETS libzstd_static DESTINATION ${INSTALL_LIBRARY_DIR})
    INSTALL(TARGETS libzstd_shared LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR})

    # Create symlinks and setup this files
    SET(SHARED_LIBRARY_LINK ${SHARED_LIBRARY_OUTPUT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    SET(SHARED_LIBRARY_SYMLINK1 ${LIBRARY_BASE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    SET(SHARED_LIBRARY_SYMLINK2 ${LIBRARY_BASE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${LIBVER_MAJOR})

    SET(SHARED_LIBRARY_LINK_PATH ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIBRARY_LINK})
    SET(SHARED_LIBRARY_SYMLINK1_PATH ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIBRARY_SYMLINK1})
    SET(SHARED_LIBRARY_SYMLINK2_PATH ${CMAKE_CURRENT_BINARY_DIR}/${SHARED_LIBRARY_SYMLINK2})

    if (EXISTS ${SHARED_LIBRARY_SYMLINK1_PATH})
        FILE(REMOVE ${SHARED_LIBRARY_SYMLINK1_PATH})
    endif (EXISTS ${SHARED_LIBRARY_SYMLINK1_PATH})

    if (EXISTS ${SHARED_LIBRARY_SYMLINK2_PATH})
        FILE(REMOVE ${SHARED_LIBRARY_SYMLINK2_PATH})
    endif (EXISTS ${SHARED_LIBRARY_SYMLINK2_PATH})

    ADD_CUSTOM_COMMAND(TARGET libzstd_shared POST_BUILD
            COMMAND ln -s ${SHARED_LIBRARY_LINK} ${SHARED_LIBRARY_SYMLINK1}
            DEPENDS ${SHARED_LIBRARY_LINK_PATH}
            COMMENT "Generating symbolic link")

    ADD_CUSTOM_COMMAND(TARGET libzstd_shared POST_BUILD
            COMMAND ln -s ${SHARED_LIBRARY_LINK} ${SHARED_LIBRARY_SYMLINK2}
            DEPENDS ${SHARED_LIBRARY_LINK_PATH}
            COMMENT "Generating symbolic link")

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
