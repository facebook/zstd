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

PROJECT(emscripten)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define project root directory
SET(ROOT_DIR ../../..)

# Define programs directory, where sources and header files are located
SET(EMSCRIPTEN_DIR ${ROOT_DIR}/emscripten)
INCLUDE_DIRECTORIES(${EMSCRIPTEN_DIR})

IF (WORKAROUND_OUTDATED_CODE_STYLE)
    # Define library directory, where sources and header files are located
    SET(LIBRARY_DIR ${ROOT_DIR}/lib)
    INCLUDE_DIRECTORIES(${LIBRARY_DIR})
ENDIF (WORKAROUND_OUTDATED_CODE_STYLE)

#IF (ZSTD_LEGACY_SUPPORT)
#    SET(PROGRAMS_LEGACY_DIR ${PROGRAMS_DIR}/legacy)
#    INCLUDE_DIRECTORIES(${PROGRAMS_LEGACY_DIR})
#
#   IF (WORKAROUND_OUTDATED_CODE_STYLE)
#        INCLUDE_DIRECTORIES(${LIBRARY_DIR}/legacy)
#    ENDIF (WORKAROUND_OUTDATED_CODE_STYLE)
#
#    SET(ZSTD_FILEIO_LEGACY ${PROGRAMS_LEGACY_DIR}/fileio_legacy.c)
#ENDIF (ZSTD_LEGACY_SUPPORT)

if (EMSCRIPTEN)
    set(CMAKE_AR "emcc")
    set(CMAKE_STATIC_LIBRARY_SUFFIX ".bc")
    set(CMAKE_C_CREATE_STATIC_LIBRARY "<CMAKE_AR> -o <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_CREATE_STATIC_LIBRARY "<CMAKE_AR> -o <TARGET> <LINK_FLAGS> <OBJECTS>")
endif()

set(CMAKE_C_FLAGS "--bind -Oz -s EXPORT_NAME=\"'ZStd'\" -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s DISABLE_EXCEPTION_CATCHING=1 --memory-init-file 0 -s EXPORTED_FUNCTIONS=\"['_ZStdCompress','_ZStdDecompress']\"")

ADD_EXECUTABLE(zstd ${EMSCRIPTEN_DIR}/zstd.js.c)
TARGET_LINK_LIBRARIES(zstd libzstd_static)

