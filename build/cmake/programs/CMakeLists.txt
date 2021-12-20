# ################################################################
# Copyright (c) 2015-present, Yann Collet, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

project(programs C)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# Define programs directory, where sources and header files are located
set(LIBRARY_DIR ${ZSTD_SOURCE_DIR}/lib)
set(PROGRAMS_DIR ${ZSTD_SOURCE_DIR}/programs)
include_directories(${PROGRAMS_DIR} ${LIBRARY_DIR} ${LIBRARY_DIR}/common ${LIBRARY_DIR}/compress ${LIBRARY_DIR}/dictBuilder)

if (ZSTD_LEGACY_SUPPORT)
    set(PROGRAMS_LEGACY_DIR ${PROGRAMS_DIR}/legacy)
    include_directories(${PROGRAMS_LEGACY_DIR} ${LIBRARY_DIR}/legacy)
endif ()

if (ZSTD_PROGRAMS_LINK_SHARED)
    set(PROGRAMS_ZSTD_LINK_TARGET libzstd_shared)
else ()
    set(PROGRAMS_ZSTD_LINK_TARGET libzstd_static)
endif ()

if (MSVC)
    set(MSVC_RESOURCE_DIR ${ZSTD_SOURCE_DIR}/build/VS2010/zstd)
    set(PlatformDependResources ${MSVC_RESOURCE_DIR}/zstd.rc)
endif ()

add_executable(zstd ${PROGRAMS_DIR}/zstdcli.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${PROGRAMS_DIR}/fileio.c ${PROGRAMS_DIR}/benchfn.c ${PROGRAMS_DIR}/benchzstd.c ${PROGRAMS_DIR}/datagen.c ${PROGRAMS_DIR}/dibio.c ${PROGRAMS_DIR}/zstdcli_trace.c ${PlatformDependResources})
target_link_libraries(zstd ${PROGRAMS_ZSTD_LINK_TARGET})
if (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
    target_link_libraries(zstd rt)
endif ()
install(TARGETS zstd
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}")

if (UNIX)
    add_custom_target(zstdcat ALL ${CMAKE_COMMAND} -E create_symlink zstd zstdcat DEPENDS zstd COMMENT "Creating zstdcat symlink")
    add_custom_target(unzstd ALL ${CMAKE_COMMAND} -E create_symlink zstd unzstd DEPENDS zstd COMMENT "Creating unzstd symlink")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zstdcat DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/unzstd DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(PROGRAMS ${PROGRAMS_DIR}/zstdgrep DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(PROGRAMS ${PROGRAMS_DIR}/zstdless DESTINATION "${CMAKE_INSTALL_BINDIR}")

    add_custom_target(zstd.1 ALL
        ${CMAKE_COMMAND} -E copy ${PROGRAMS_DIR}/zstd.1 .
        COMMENT "Copying manpage zstd.1")
    add_custom_target(zstdgrep.1 ALL
        ${CMAKE_COMMAND} -E copy ${PROGRAMS_DIR}/zstdgrep.1 .
        COMMENT "Copying manpage zstdgrep.1")
    add_custom_target(zstdless.1 ALL
        ${CMAKE_COMMAND} -E copy ${PROGRAMS_DIR}/zstdless.1 .
        COMMENT "Copying manpage zstdless.1")
    add_custom_target(zstdcat.1 ALL ${CMAKE_COMMAND} -E create_symlink zstd.1 zstdcat.1 DEPENDS zstd.1 COMMENT "Creating zstdcat.1 symlink")
    add_custom_target(unzstd.1 ALL ${CMAKE_COMMAND} -E create_symlink zstd.1 unzstd.1 DEPENDS zstd.1 COMMENT "Creating unzstd.1 symlink")

    # Define MAN_INSTALL_DIR if necessary
    if (MAN_INSTALL_DIR)
    else ()
        set(MAN_INSTALL_DIR ${CMAKE_INSTALL_MANDIR}/man1)
    endif ()

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/zstd.1
        ${CMAKE_CURRENT_BINARY_DIR}/zstdcat.1
        ${CMAKE_CURRENT_BINARY_DIR}/unzstd.1
        ${CMAKE_CURRENT_BINARY_DIR}/zstdgrep.1
        ${CMAKE_CURRENT_BINARY_DIR}/zstdless.1
        DESTINATION "${MAN_INSTALL_DIR}")

    add_executable(zstd-frugal ${PROGRAMS_DIR}/zstdcli.c ${PROGRAMS_DIR}/util.c ${PROGRAMS_DIR}/timefn.c ${PROGRAMS_DIR}/fileio.c)
    target_link_libraries(zstd-frugal ${PROGRAMS_ZSTD_LINK_TARGET})
    set_property(TARGET zstd-frugal APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_NOBENCH;ZSTD_NODICT;ZSTD_NOTRACE")
endif ()

# Add multi-threading support definitions

if (ZSTD_MULTITHREAD_SUPPORT)
    set_property(TARGET zstd APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")

    if (UNIX)
        target_link_libraries(zstd ${THREADS_LIBS})

        add_custom_target(zstdmt ALL ${CMAKE_COMMAND} -E create_symlink zstd zstdmt DEPENDS zstd COMMENT "Creating zstdmt symlink")
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zstdmt DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif ()
endif ()

option(ZSTD_ZLIB_SUPPORT "ZLIB SUPPORT" OFF)
option(ZSTD_LZMA_SUPPORT "LZMA SUPPORT" OFF)
option(ZSTD_LZ4_SUPPORT "LZ4 SUPPORT" OFF)

# Add gzip support
if (ZSTD_ZLIB_SUPPORT)
    find_package(ZLIB REQUIRED)

    if (ZLIB_FOUND)
        include_directories(${ZLIB_INCLUDE_DIRS})
        target_link_libraries(zstd ${ZLIB_LIBRARIES})
        set_property(TARGET zstd APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_GZCOMPRESS;ZSTD_GZDECOMPRESS")
    else ()
        message(SEND_ERROR "zlib library is missing")
    endif ()
endif ()

# Add lzma support
if (ZSTD_LZMA_SUPPORT)
    find_package(LibLZMA REQUIRED)

    if (LIBLZMA_FOUND)
        include_directories(${LIBLZMA_INCLUDE_DIRS})
        target_link_libraries(zstd ${LIBLZMA_LIBRARIES})
        set_property(TARGET zstd APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_LZMACOMPRESS;ZSTD_LZMADECOMPRESS")
    else ()
        message(SEND_ERROR "lzma library is missing")
    endif ()
endif ()

# Add lz4 support
if (ZSTD_LZ4_SUPPORT)
    find_package(LibLZ4 REQUIRED)

    if (LIBLZ4_FOUND)
        include_directories(${LIBLZ4_INCLUDE_DIRS})
        target_link_libraries(zstd ${LIBLZ4_LIBRARIES})
        set_property(TARGET zstd APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_LZ4COMPRESS;ZSTD_LZ4DECOMPRESS")
    else ()
        message(SEND_ERROR "lz4 library is missing")
    endif ()
endif ()
