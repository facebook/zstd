# ################################################################
# zstd - Makefile
# Copyright (C) Yann Collet 2014-2015
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
#  - zstd source repository : https://github.com/Cyan4973/zstd
#  - Public forum : https://groups.google.com/forum/#!forum/lz4c
# ################################################################

function(ExtractVersion _content _outputVar1 _outputVar2 _outputVar3)
    string(REGEX MATCHALL ".*define ZSTD_VERSION_MAJOR+.* ([0-9]+).*define ZSTD_VERSION_MINOR+.* ([0-9]+).*define ZSTD_VERSION_RELEASE+.* ([0-9]+)" VERSION_REGEX "${_content}")
    SET(${_outputVar1} ${CMAKE_MATCH_1} PARENT_SCOPE)
    SET(${_outputVar2} ${CMAKE_MATCH_2} PARENT_SCOPE)
    SET(${_outputVar3} ${CMAKE_MATCH_3} PARENT_SCOPE)
endfunction()

project(libzstd)
cmake_minimum_required(VERSION 2.8.12)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

file(READ zstd.h HEADER_CONTENT)

ExtractVersion("${HEADER_CONTENT}" LIBVER_MAJOR LIBVER_MINOR LIBVER_RELEASE)
message("ZSTD VERSION ${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE}")

SET(Sources
        fse.c
        huff0.c
        zstd_buffered.c
        zstd_compress.c
        zstd_decompress.c)

SET(Headers
        bitstream.h
        error.h
        fse.h
        fse_static.h
        huff0.h
        huff0_static.h
        mem.h
        zstd_buffered_static.h
        zstd_buffered.h
        zstd_internal.h
        zstd_static.h
        zstd.h)

IF (ZSTD_LEGACY_SUPPORT)
    include_directories(legacy)

    SET(Sources ${Sources}
            legacy/zstd_v01.c
            legacy/zstd_v02.c
            legacy/zstd_v03.c)

    SET(Headers ${Headers}
            legacy/zstd_legacy.h
            legacy/zstd_v01.h
            legacy/zstd_v02.h
            legacy/zstd_v03.h)
ENDIF (ZSTD_LEGACY_SUPPORT)

IF (MSVC)
    SET(PlatformDependResources msvc/resource.h msvc/zstdlib.rc)
ENDIF (MSVC)

add_library(libzstd_static STATIC ${Sources} ${Headers} ${PlatformDependResources})
add_library(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})

IF (MSVC)
    set_target_properties(libzstd_static PROPERTIES COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    set_target_properties(libzstd_shared PROPERTIES COMPILE_DEFINITIONS "ZSTD_DLL_EXPORT=1;ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    set_target_properties(libzstd_shared PROPERTIES COMPILE_DEFINITIONS "-DBUILD_SHARED_LIBS=TRUE")
ENDIF (MSVC)

target_include_directories(libzstd_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(libzstd_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

IF (ZSTD_LEGACY_SUPPORT)
    target_include_directories(libzstd_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/legacy)
    target_include_directories(libzstd_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/legacy)
ENDIF (ZSTD_LEGACY_SUPPORT)

SET_TARGET_PROPERTIES(
        libzstd_shared
        PROPERTIES
        OUTPUT_NAME libzstd_shared.${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE})

SET_TARGET_PROPERTIES(
        libzstd_static
        PROPERTIES
        OUTPUT_NAME libzstd_static.${LIBVER_MAJOR}.${LIBVER_MINOR}.${LIBVER_RELEASE})