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

project(tests)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
set(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
set(TESTS_DIR ${ZSTD_SOURCE_DIR}/tests)
include_directories(${TESTS_DIR} ${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${LIBRARY_DIR}/compress ${LIBRARY_DIR}/dictBuilder)

add_executable(datagen ${PROGRAMS_DIR}/datagen.c ${TESTS_DIR}/datagencli.c)
target_link_libraries(datagen libzstd_static)

add_executable(fullbench ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${PROGRAMS_DIR}/benchfn.c ${PROGRAMS_DIR}/benchzstd.c ${TESTS_DIR}/fullbench.c)
target_link_libraries(fullbench libzstd_static)

add_executable(fuzzer ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${TESTS_DIR}/fuzzer.c)
target_link_libraries(fuzzer libzstd_static)

if (UNIX)
    add_executable(paramgrill ${PROGRAMS_DIR}/benchfn.c ${PROGRAMS_DIR}/benchzstd.c ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${TESTS_DIR}/paramgrill.c)
    target_link_libraries(paramgrill libzstd_static m) #m is math library
endif ()
