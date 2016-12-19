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

PROJECT(programs)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define project root directory
SET(ROOT_DIR ../../..)

# Define programs directory, where sources and header files are located
SET(LIBRARY_DIR ${ROOT_DIR}/lib)
SET(PROGRAMS_DIR ${ROOT_DIR}/programs)
INCLUDE_DIRECTORIES(${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${LIBRARY_DIR}/dictBuilder)

IF (ZSTD_LEGACY_SUPPORT)
    SET(PROGRAMS_LEGACY_DIR ${PROGRAMS_DIR}/legacy)
    INCLUDE_DIRECTORIES(${PROGRAMS_LEGACY_DIR} ${LIBRARY_DIR}/legacy)
ENDIF (ZSTD_LEGACY_SUPPORT)

IF (MSVC)
    SET(MSVC_RESOURCE_DIR ${ROOT_DIR}/build/VS2010/zstd)
    SET(PlatformDependResources ${MSVC_RESOURCE_DIR}/zstd.rc)
ENDIF (MSVC)

ADD_EXECUTABLE(zstd ${PROGRAMS_DIR}/zstdcli.c ${PROGRAMS_DIR}/fileio.c ${PROGRAMS_DIR}/bench.c ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/dibio.c ${PlatformDependResources})
TARGET_LINK_LIBRARIES(zstd libzstd_static)

IF (UNIX)
    ADD_EXECUTABLE(zstd-frugal ${PROGRAMS_DIR}/zstdcli.c ${PROGRAMS_DIR}/fileio.c)
    TARGET_LINK_LIBRARIES(zstd-frugal libzstd_static)
    SET_TARGET_PROPERTIES(zstd-frugal PROPERTIES COMPILE_DEFINITIONS "ZSTD_NOBENCH;ZSTD_NODICT")
ENDIF (UNIX)
