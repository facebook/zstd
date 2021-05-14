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

# name: Cache variable name. The value is expected to be a semicolon-separated
# list of command line flags
# default_value: Value to initialize the option with. Can be space separated.
function(AddTestFlagsOption name default_value doc)
    string(STRIP "${default_value}" default_value)
    string(REGEX REPLACE " +" ";" default_value "${default_value}")
    set(${name} ${default_value} CACHE STRING "${doc}")
    mark_as_advanced(${name})
endfunction()

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
set(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
set(TESTS_DIR ${ZSTD_SOURCE_DIR}/tests)
include_directories(${TESTS_DIR} ${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${LIBRARY_DIR}/compress ${LIBRARY_DIR}/dictBuilder)

add_executable(datagen ${PROGRAMS_DIR}/datagen.c ${TESTS_DIR}/datagencli.c)
target_link_libraries(datagen libzstd_static)

#
# fullbench
#
add_executable(fullbench ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${PROGRAMS_DIR}/benchfn.c ${PROGRAMS_DIR}/benchzstd.c ${TESTS_DIR}/fullbench.c)
set_property(TARGET fullbench APPEND PROPERTY COMPILE_OPTIONS "-Wno-deprecated-declarations")
target_link_libraries(fullbench libzstd_static)
add_test(NAME fullbench COMMAND fullbench ${ZSTD_FULLBENCH_FLAGS})

#
# fuzzer
#
add_executable(fuzzer ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${TESTS_DIR}/fuzzer.c)
set_property(TARGET fuzzer APPEND PROPERTY COMPILE_OPTIONS "-Wno-deprecated-declarations")
target_link_libraries(fuzzer libzstd_static)
AddTestFlagsOption(ZSTD_FUZZER_FLAGS "$ENV{FUZZERTEST} $ENV{FUZZER_FLAGS}"
    "Semicolon-separated list of flags to pass to the fuzzer test (see `fuzzer -h` for usage)")
add_test(NAME fuzzer COMMAND fuzzer ${ZSTD_FUZZER_FLAGS})
# Disable the timeout since the run time is too long for the default timeout of
# 1500 seconds and varies considerably between low-end and high-end CPUs.
# set_tests_properties(fuzzer PROPERTIES TIMEOUT 0)

#
# zstreamtest
#
add_executable(zstreamtest ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${TESTS_DIR}/seqgen.c ${TESTS_DIR}/zstreamtest.c)
set_property(TARGET zstreamtest APPEND PROPERTY COMPILE_OPTIONS "-Wno-deprecated-declarations")
target_link_libraries(zstreamtest libzstd_static)
AddTestFlagsOption(ZSTD_ZSTREAM_FLAGS "$ENV{ZSTREAM_TESTTIME} $ENV{FUZZER_FLAGS}"
    "Semicolon-separated list of flags to pass to the zstreamtest test (see `zstreamtest -h` for usage)")
add_test(NAME zstreamtest COMMAND zstreamtest ${ZSTD_ZSTREAM_FLAGS})

#
# playTests.sh
#
AddTestFlagsOption(ZSTD_PLAYTESTS_FLAGS "$ENV{PLAYTESTS_FLAGS}"
    "Semicolon-separated list of flags to pass to the playTests.sh test")
add_test(NAME playTests COMMAND sh -c "\"${TESTS_DIR}/playTests.sh\" ${ZSTD_PLAYTESTS_FLAGS}")
if (ZSTD_BUILD_PROGRAMS)
    set_property(TEST playTests APPEND PROPERTY ENVIRONMENT
        "ZSTD_BIN=$<TARGET_FILE:zstd>"
        "DATAGEN_BIN=$<TARGET_FILE:datagen>"
        )
else()
    message(STATUS "Disabling playTests.sh test because ZSTD_BUILD_PROGRAMS is not enabled")
    set_tests_properties(playTests PROPERTIES DISABLED YES)
endif()

# Label the "Medium" set of tests (see TESTING.md)
set_property(TEST fuzzer zstreamtest playTests APPEND PROPERTY LABELS Medium)

add_executable(paramgrill ${PROGRAMS_DIR}/benchfn.c ${PROGRAMS_DIR}/benchzstd.c ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${TESTS_DIR}/paramgrill.c)
if (UNIX)
    target_link_libraries(paramgrill libzstd_static m) #m is math library
else()
    target_link_libraries(paramgrill libzstd_static)
endif ()
