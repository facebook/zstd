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

project(programs)
cmake_minimum_required(VERSION 2.8.12)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

IF (ZSTD_LEGACY_SUPPORT)
    include_directories(legacy)
    SET(ZSTD_FILEIO_LEGACY legacy/fileio_legacy.c)
ENDIF (ZSTD_LEGACY_SUPPORT)

SET(Sources_zstd zstdcli.c fileio.c ${ZSTD_FILEIO_LEGACY} bench.c xxhash.c datagen.c)

add_executable(zstd ${Sources_zstd})
target_link_libraries(zstd libzstd_static)

add_executable(zstd-noBench zstdcli.c fileio.c ${ZSTD_FILEIO_LEGACY})
target_link_libraries(zstd-noBench libzstd_static)
set_target_properties(zstd-noBench PROPERTIES COMPILE_DEFINITIONS "ZSTD_NOBENCH")

add_executable(fullbench datagen.c fullbench.c)
target_link_libraries(fullbench libzstd_static)

add_executable(fuzzer datagen.c xxhash.c fuzzer.c)
target_link_libraries(fuzzer libzstd_static)

add_executable(zbufftest datagen.c xxhash.c zbufftest.c)
target_link_libraries(zbufftest libzstd_static)

add_executable(paramgrill datagen.c xxhash.c paramgrill.c)
target_link_libraries(paramgrill libzstd_static)
IF (NOT MSVC)
    set_target_properties(paramgrill PROPERTIES LINK_FLAGS "-lm")
ENDIF (NOT MSVC)

add_executable(datagen datagen.c datagencli.c)
target_link_libraries(datagen libzstd_static)
