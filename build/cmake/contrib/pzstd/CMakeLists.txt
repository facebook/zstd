# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

project(pzstd)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
set(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
set(PZSTD_DIR ${ZSTD_SOURCE_DIR}/contrib/pzstd)
include_directories(${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${PZSTD_DIR})

add_executable(pzstd ${PROGRAMS_DIR}/util.c ${PZSTD_DIR}/main.cpp ${PZSTD_DIR}/Options.cpp ${PZSTD_DIR}/Pzstd.cpp ${PZSTD_DIR}/SkippableFrame.cpp)
set_property(TARGET pzstd APPEND PROPERTY COMPILE_DEFINITIONS "NDEBUG")
set_property(TARGET pzstd APPEND PROPERTY COMPILE_OPTIONS "-Wno-shadow")

if (ZSTD_BUILD_SHARED)
    set(ZSTD_LIB libzstd_shared)
else()
    set(ZSTD_LIB libzstd_static)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
  target_link_libraries(pzstd ${ZSTD_LIB} ${CMAKE_THREAD_LIBS_INIT})
else()
    message(SEND_ERROR "ZSTD currently does not support thread libraries other than pthreads")
endif()

install(TARGETS pzstd RUNTIME DESTINATION "bin")
