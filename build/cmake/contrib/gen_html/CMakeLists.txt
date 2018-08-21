# ################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

PROJECT(gen_html)
INCLUDE(GetZstdLibraryVersion)

SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
SET(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
SET(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
SET(GENHTML_DIR ${ZSTD_SOURCE_DIR}/contrib/gen_html)
SET(GENHTML_BINARY ${PROJECT_BINARY_DIR}/gen_html${CMAKE_EXECUTABLE_SUFFIX})
INCLUDE_DIRECTORIES(${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${GENHTML_DIR})

ADD_EXECUTABLE(gen_html ${GENHTML_DIR}/gen_html.cpp)

GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h VMAJOR VMINOR VRELEASE)
SET(LIBVERSION "${VMAJOR}.${VMINOR}.${VRELEASE}")
ADD_CUSTOM_TARGET(zstd_manual.html ALL
                  ${GENHTML_BINARY} "${LIBVERSION}" "${LIBRARY_DIR}/zstd.h" "${PROJECT_BINARY_DIR}/zstd_manual.html"
                  DEPENDS gen_html COMMENT "Update zstd manual")

INSTALL(FILES "${PROJECT_BINARY_DIR}/zstd_manual.html" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
