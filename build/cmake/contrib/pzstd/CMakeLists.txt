# ################################################################
# * Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# * All rights reserved.
# *
# * This source code is licensed under the BSD-style license found in the
# * LICENSE file in the root directory of this source tree. An additional grant
# * of patent rights can be found in the PATENTS file in the same directory.
#
# You can contact the author at :
#  - zstd homepage : http://www.zstd.net/
# ################################################################

PROJECT(pzstd)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
SET(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
SET(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
SET(PZSTD_DIR ${ZSTD_SOURCE_DIR}/contrib/pzstd)
INCLUDE_DIRECTORIES(${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${PZSTD_DIR})

ADD_EXECUTABLE(pzstd ${PZSTD_DIR}/main.cpp ${PZSTD_DIR}/Options.cpp ${PZSTD_DIR}/Pzstd.cpp ${PZSTD_DIR}/SkippableFrame.cpp)
TARGET_LINK_LIBRARIES(pzstd libzstd_shared pthread)
SET_TARGET_PROPERTIES(pzstd PROPERTIES COMPILE_DEFINITIONS "NDEBUG")
SET_TARGET_PROPERTIES(pzstd PROPERTIES COMPILE_OPTIONS "-Wno-shadow")

INSTALL(TARGETS pzstd RUNTIME DESTINATION "bin")
