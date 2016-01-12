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

PROJECT(programs)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define project root directory
SET(ROOT_DIR ../../..)

# Define programs directory, where sources and header files are located
SET(PROGRAMS_DIR ${ROOT_DIR}/programs)
INCLUDE_DIRECTORIES(${PROGRAMS_DIR})

IF (WORKAROUND_OUTDATED_CODE_STYLE)
    # Define library directory, where sources and header files are located
    SET(LIBRARY_DIR ${ROOT_DIR}/lib)
    INCLUDE_DIRECTORIES(${LIBRARY_DIR})
ENDIF (WORKAROUND_OUTDATED_CODE_STYLE)

IF (ZSTD_LEGACY_SUPPORT)
    SET(PROGRAMS_LEGACY_DIR ${PROGRAMS_DIR}/legacy)
    INCLUDE_DIRECTORIES(${PROGRAMS_LEGACY_DIR})

    IF (WORKAROUND_OUTDATED_CODE_STYLE)
        INCLUDE_DIRECTORIES(${LIBRARY_DIR}/legacy)
    ENDIF (WORKAROUND_OUTDATED_CODE_STYLE)

    SET(ZSTD_FILEIO_LEGACY ${PROGRAMS_LEGACY_DIR}/fileio_legacy.c)
ENDIF (ZSTD_LEGACY_SUPPORT)

ADD_EXECUTABLE(zstd ${PROGRAMS_DIR}/zstdcli.c ${PROGRAMS_DIR}/fileio.c ${PROGRAMS_DIR}/bench.c ${PROGRAMS_DIR}/xxhash.c ${PROGRAMS_DIR}/datagen.c ${ZSTD_FILEIO_LEGACY})
TARGET_LINK_LIBRARIES(zstd libzstd_static)

ADD_EXECUTABLE(fullbench ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/fullbench.c)
TARGET_LINK_LIBRARIES(fullbench libzstd_static)

ADD_EXECUTABLE(fuzzer ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/xxhash.c ${PROGRAMS_DIR}/fuzzer.c)
TARGET_LINK_LIBRARIES(fuzzer libzstd_static)

IF (UNIX)
    ADD_EXECUTABLE(zstd-noBench ${PROGRAMS_DIR}/zstdcli.c ${PROGRAMS_DIR}/fileio.c ${ZSTD_FILEIO_LEGACY})
    TARGET_LINK_LIBRARIES(zstd-noBench libzstd_static)
    SET_TARGET_PROPERTIES(zstd-noBench PROPERTIES COMPILE_DEFINITIONS "ZSTD_NOBENCH")

    ADD_EXECUTABLE(zbufftest ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/xxhash.c ${PROGRAMS_DIR}/zbufftest.c)
    TARGET_LINK_LIBRARIES(zbufftest libzstd_static)

    ADD_EXECUTABLE(paramgrill ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/xxhash.c ${PROGRAMS_DIR}/paramgrill.c)
    TARGET_LINK_LIBRARIES(paramgrill libzstd_static m) #m is math library

    ADD_EXECUTABLE(datagen ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/datagencli.c)
    TARGET_LINK_LIBRARIES(datagen libzstd_static)
ENDIF (UNIX)
