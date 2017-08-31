# ################################################################
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

PROJECT(pzstd)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
SET(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
SET(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
SET(PZSTD_DIR ${ZSTD_SOURCE_DIR}/contrib/pzstd)
INCLUDE_DIRECTORIES(${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${PZSTD_DIR})

ADD_EXECUTABLE(pzstd ${PZSTD_DIR}/main.cpp ${PZSTD_DIR}/Options.cpp ${PZSTD_DIR}/Pzstd.cpp ${PZSTD_DIR}/SkippableFrame.cpp)
SET_PROPERTY(TARGET pzstd APPEND PROPERTY COMPILE_DEFINITIONS "NDEBUG")
SET_PROPERTY(TARGET pzstd APPEND PROPERTY COMPILE_OPTIONS "-Wno-shadow")

SET(THREADS_PREFER_PTHREAD_FLAG ON)
FIND_PACKAGE(Threads REQUIRED)
IF (CMAKE_USE_PTHREADS_INIT)
    TARGET_LINK_LIBRARIES(pzstd libzstd_shared ${CMAKE_THREAD_LIBS_INIT})
ELSE()
    MESSAGE(SEND_ERROR "ZSTD currently does not support thread libraries other than pthreads")
ENDIF()

INSTALL(TARGETS pzstd RUNTIME DESTINATION "bin")
