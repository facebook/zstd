# ################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

project(gen_html)
include(GetZstdLibraryVersion)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
set(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
set(GENHTML_DIR ${ZSTD_SOURCE_DIR}/contrib/gen_html)
set(GENHTML_BINARY ${PROJECT_BINARY_DIR}/gen_html${CMAKE_EXECUTABLE_SUFFIX})
include_directories(${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${GENHTML_DIR})

add_executable(gen_html ${GENHTML_DIR}/gen_html.cpp)

GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h VMAJOR VMINOR VRELEASE)
set(LIBVERSION "${VMAJOR}.${VMINOR}.${VRELEASE}")
add_custom_target(zstd_manual.html ALL
                  ${GENHTML_BINARY} "${LIBVERSION}" "${LIBRARY_DIR}/zstd.h" "${PROJECT_BINARY_DIR}/zstd_manual.html"
                  DEPENDS gen_html COMMENT "Update zstd manual")

install(FILES "${PROJECT_BINARY_DIR}/zstd_manual.html" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
