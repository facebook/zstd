# ################################################################
# zstd - Makefile
# Copyright (C) Yann Collet 2014-present
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

PROJECT(tests)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
SET(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
SET(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
SET(TESTS_DIR ${ZSTD_SOURCE_DIR}/tests)
INCLUDE_DIRECTORIES(${TESTS_DIR} ${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${LIBRARY_DIR}/compress ${LIBRARY_DIR}/dictBuilder)

ADD_EXECUTABLE(datagen ${PROGRAMS_DIR}/datagen.c ${TESTS_DIR}/datagencli.c)
TARGET_LINK_LIBRARIES(datagen libzstd_static)

ADD_EXECUTABLE(fullbench ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/bench.c ${TESTS_DIR}/fullbench.c)
TARGET_LINK_LIBRARIES(fullbench libzstd_static)

ADD_EXECUTABLE(fuzzer ${PROGRAMS_DIR}/datagen.c ${TESTS_DIR}/fuzzer.c)
TARGET_LINK_LIBRARIES(fuzzer libzstd_static)

IF (UNIX)
    ADD_EXECUTABLE(paramgrill ${PROGRAMS_DIR}/bench.c ${PROGRAMS_DIR}/datagen.c ${TESTS_DIR}/paramgrill.c)
    TARGET_LINK_LIBRARIES(paramgrill libzstd_static m) #m is math library
ENDIF (UNIX)
