# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# ################################################################

project(libzstd C ASM)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
option(ZSTD_BUILD_STATIC "BUILD STATIC LIBRARIES" ON)
option(ZSTD_BUILD_SHARED "BUILD SHARED LIBRARIES" ON)
option(ZSTD_BUILD_COMPRESSION "BUILD COMPRESSION MODULE" ON)
option(ZSTD_BUILD_DECOMPRESSION "BUILD DECOMPRESSION MODULE" ON)
option(ZSTD_BUILD_DICTBUILDER "BUILD DICTBUILDER MODULE" ON)
option(ZSTD_BUILD_DEPRECATED "BUILD DEPRECATED MODULE" OFF)

set(ZSTDLIB_VISIBLE "" CACHE STRING "Visibility for ZSTDLIB API")
set(ZSTDERRORLIB_VISIBLE "" CACHE STRING "Visibility for ZSTDERRORLIB_VISIBLE API")
set(ZDICTLIB_VISIBLE "" CACHE STRING "Visibility for ZDICTLIB_VISIBLE API")
set(ZSTDLIB_STATIC_API "" CACHE STRING "Visibility for ZSTDLIB_STATIC_API API")
set(ZDICTLIB_STATIC_API "" CACHE STRING "Visibility for ZDICTLIB_STATIC_API API")

set_property(CACHE ZSTDLIB_VISIBLE PROPERTY STRINGS "" "hidden" "default" "protected" "internal")
set_property(CACHE ZSTDERRORLIB_VISIBLE PROPERTY STRINGS "" "hidden" "default" "protected" "internal")
set_property(CACHE ZDICTLIB_VISIBLE PROPERTY STRINGS "" "hidden" "default" "protected" "internal")
set_property(CACHE ZSTDLIB_STATIC_API PROPERTY STRINGS "" "hidden" "default" "protected" "internal")
set_property(CACHE ZDICTLIB_STATIC_API PROPERTY STRINGS "" "hidden" "default" "protected" "internal")

if(NOT ZSTD_BUILD_SHARED AND NOT ZSTD_BUILD_STATIC)
    message(SEND_ERROR "You need to build at least one flavor of libzstd")
endif()

file(GLOB CommonSources ${LIBRARY_DIR}/common/*.c)
file(GLOB CompressSources ${LIBRARY_DIR}/compress/*.c)
file(GLOB DecompressSources ${LIBRARY_DIR}/decompress/*.c)
if (MSVC)
    add_compile_options(-DZSTD_DISABLE_ASM)
else ()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|AMD64.*|x86_64.*|X86_64.*" AND ${ZSTD_HAS_NOEXECSTACK})
        set(DecompressSources ${DecompressSources} ${LIBRARY_DIR}/decompress/huf_decompress_amd64.S)
    else()
        add_compile_options(-DZSTD_DISABLE_ASM)
    endif()
endif ()
file(GLOB DictBuilderSources ${LIBRARY_DIR}/dictBuilder/*.c)
file(GLOB DeprecatedSources ${LIBRARY_DIR}/deprecated/*.c)

file(GLOB PublicHeaders ${LIBRARY_DIR}/*.h)
file(GLOB CommonHeaders ${LIBRARY_DIR}/common/*.h)
file(GLOB CompressHeaders ${LIBRARY_DIR}/compress/*.h)
file(GLOB DecompressHeaders ${LIBRARY_DIR}/decompress/*.h)
file(GLOB DictBuilderHeaders ${LIBRARY_DIR}/dictBuilder/*.h)
file(GLOB DeprecatedHeaders ${LIBRARY_DIR}/deprecated/*.h)

set(Sources ${CommonSources})
set(Headers ${PublicHeaders} ${CommonHeaders})
if (ZSTD_BUILD_COMPRESSION)
    set(Sources ${Sources} ${CompressSources})
    set(Headers ${Headers} ${CompressHeaders})
endif()
if (ZSTD_BUILD_DECOMPRESSION)
    set(Sources ${Sources} ${DecompressSources})
    set(Headers ${Headers} ${DecompressHeaders})
endif()
if (ZSTD_BUILD_DICTBUILDER)
    set(Sources ${Sources} ${DictBuilderSources})
    set(Headers ${Headers} ${DictBuilderHeaders})
endif()
if (ZSTD_BUILD_DEPRECATED)
    set(Sources ${Sources} ${DeprecatedSources})
    set(Headers ${Headers} ${DeprecatedHeaders})
endif()

if (ZSTD_LEGACY_SUPPORT)
    set(LIBRARY_LEGACY_DIR ${LIBRARY_DIR}/legacy)

    set(Sources ${Sources}
            ${LIBRARY_LEGACY_DIR}/zstd_v01.c
            ${LIBRARY_LEGACY_DIR}/zstd_v02.c
            ${LIBRARY_LEGACY_DIR}/zstd_v03.c
            ${LIBRARY_LEGACY_DIR}/zstd_v04.c
            ${LIBRARY_LEGACY_DIR}/zstd_v05.c
            ${LIBRARY_LEGACY_DIR}/zstd_v06.c
            ${LIBRARY_LEGACY_DIR}/zstd_v07.c)

    set(Headers ${Headers}
            ${LIBRARY_LEGACY_DIR}/zstd_legacy.h
            ${LIBRARY_LEGACY_DIR}/zstd_v01.h
            ${LIBRARY_LEGACY_DIR}/zstd_v02.h
            ${LIBRARY_LEGACY_DIR}/zstd_v03.h
            ${LIBRARY_LEGACY_DIR}/zstd_v04.h
            ${LIBRARY_LEGACY_DIR}/zstd_v05.h
            ${LIBRARY_LEGACY_DIR}/zstd_v06.h
            ${LIBRARY_LEGACY_DIR}/zstd_v07.h)
endif ()

if (MSVC)
    set(MSVC_RESOURCE_DIR ${ZSTD_SOURCE_DIR}/build/VS2010/libzstd-dll)
    set(PlatformDependResources ${MSVC_RESOURCE_DIR}/libzstd-dll.rc)
endif ()

# Explicitly set the language to C for all files, including ASM files.
# Our assembly expects to be compiled by a C compiler, and is only enabled for
# __GNUC__ compatible compilers. Otherwise all the ASM code is disabled by
# macros.
if(NOT CMAKE_ASM_COMPILER STREQUAL CMAKE_C_COMPILER)
    set_source_files_properties(${Sources} PROPERTIES LANGUAGE C)
endif()

macro (add_definition target var)
    if (NOT ("${${var}}" STREQUAL ""))
        set_property(TARGET ${target} APPEND PROPERTY COMPILE_DEFINITIONS "${var}=__attribute__((visibility(\"${${var}}\")))")
    endif ()
endmacro ()

# Define directories containing the library's public headers
set(PUBLIC_INCLUDE_DIRS ${LIBRARY_DIR})
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I \"${LIBRARY_DIR}\"")
# Split project to static and shared libraries build
set(library_targets)
if (ZSTD_BUILD_SHARED)
    add_library(libzstd_shared SHARED ${Sources} ${Headers} ${PlatformDependResources})
    target_include_directories(libzstd_shared INTERFACE $<BUILD_INTERFACE:${PUBLIC_INCLUDE_DIRS}>)
    list(APPEND library_targets libzstd_shared)
    if (ZSTD_MULTITHREAD_SUPPORT)
        set_property(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        if (UNIX)
            target_link_libraries(libzstd_shared ${THREADS_LIBS})
        endif ()
    endif ()
    add_definition(libzstd_shared ZSTDLIB_VISIBLE)
    add_definition(libzstd_shared ZSTDERRORLIB_VISIBLE)
    add_definition(libzstd_shared ZDICTLIB_VISIBLE)
endif ()
if (ZSTD_BUILD_STATIC)
    add_library(libzstd_static STATIC ${Sources} ${Headers})
    target_include_directories(libzstd_static INTERFACE $<BUILD_INTERFACE:${PUBLIC_INCLUDE_DIRS}>)
    list(APPEND library_targets libzstd_static)
    if (ZSTD_MULTITHREAD_SUPPORT)
        set_property(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_MULTITHREAD")
        if (UNIX)
            target_link_libraries(libzstd_static ${THREADS_LIBS})
        endif ()
    endif ()
    add_definition(libzstd_static ZSTDLIB_VISIBLE)
    add_definition(libzstd_static ZSTDERRORLIB_VISIBLE)
    add_definition(libzstd_static ZDICTLIB_VISIBLE)
    add_definition(libzstd_static ZSTDLIB_STATIC_API)
    add_definition(libzstd_static ZDICTLIB_STATIC_API)
endif ()
if (ZSTD_BUILD_SHARED AND NOT ZSTD_BUILD_STATIC)
    if (NOT BUILD_SHARED_LIBS)
        message(WARNING "BUILD_SHARED_LIBS is OFF, but ZSTD_BUILD_SHARED is ON and ZSTD_BUILD_STATIC is OFF, which takes precedence, so libzstd is a shared library")
    endif ()
    add_library(libzstd INTERFACE)
    target_link_libraries(libzstd INTERFACE libzstd_shared)
    list(APPEND library_targets libzstd)
endif ()
if (ZSTD_BUILD_STATIC AND NOT ZSTD_BUILD_SHARED)
    if (BUILD_SHARED_LIBS)
        message(WARNING "BUILD_SHARED_LIBS is ON, but ZSTD_BUILD_SHARED is OFF and ZSTD_BUILD_STATIC is ON, which takes precedence, is set so libzstd is a static library")
    endif ()
    add_library(libzstd INTERFACE)
    target_link_libraries(libzstd INTERFACE libzstd_static)
    list(APPEND library_targets libzstd)
endif ()
if (ZSTD_BUILD_SHARED AND ZSTD_BUILD_STATIC)
    # If both ZSTD_BUILD_SHARED and ZSTD_BUILD_STATIC are set, which is the
    # default, fallback to using BUILD_SHARED_LIBS to determine whether to
    # set libzstd to static or shared.
    if (BUILD_SHARED_LIBS)
        add_library(libzstd INTERFACE)
        target_link_libraries(libzstd INTERFACE libzstd_shared)
        list(APPEND library_targets libzstd)
    else ()
        add_library(libzstd INTERFACE)
        target_link_libraries(libzstd INTERFACE libzstd_static)
        list(APPEND library_targets libzstd)
    endif ()
endif ()

# Add specific compile definitions for MSVC project
if (MSVC)
    if (ZSTD_BUILD_SHARED)
        set_property(TARGET libzstd_shared APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_DLL_EXPORT=1;ZSTD_HEAPMODE=0;_CONSOLE;_CRT_SECURE_NO_WARNINGS")
    endif ()
    if (ZSTD_BUILD_STATIC)
        set_property(TARGET libzstd_static APPEND PROPERTY COMPILE_DEFINITIONS "ZSTD_HEAPMODE=0;_CRT_SECURE_NO_WARNINGS")
    endif ()
endif ()

# With MSVC static library needs to be renamed to avoid conflict with import library
if (MSVC OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT MINGW))
    set(STATIC_LIBRARY_BASE_NAME zstd_static)
else ()
    set(STATIC_LIBRARY_BASE_NAME zstd)
endif ()

# Define static and shared library names
if (ZSTD_BUILD_SHARED)
    set_target_properties(
            libzstd_shared
            PROPERTIES
            OUTPUT_NAME zstd
            VERSION ${ZSTD_FULL_VERSION}
            SOVERSION ${zstd_VERSION_MAJOR})
            
    if (ZSTD_FRAMEWORK)
        set_target_properties(
                libzstd_shared
                PROPERTIES
                FRAMEWORK TRUE
                FRAMEWORK_VERSION "${ZSTD_FULL_VERSION}"
                PRODUCT_BUNDLE_IDENTIFIER "github.com/facebook/zstd"
                XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
                PUBLIC_HEADER "${PublicHeaders}"
                OUTPUT_NAME "zstd"
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
                XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
                XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
                MACOSX_FRAMEWORK_IDENTIFIER "github.com/facebook/zstd"
                MACOSX_FRAMEWORK_BUNDLE_VERSION "${ZSTD_FULL_VERSION}"
                MACOSX_FRAMEWORK_SHORT_VERSION_STRING "${ZSTD_SHORT_VERSION}"
                MACOSX_RPATH TRUE
                RESOURCE ${PublicHeaders})
    endif ()
endif ()

if (ZSTD_BUILD_STATIC)
    set_target_properties(
            libzstd_static
            PROPERTIES
            POSITION_INDEPENDENT_CODE On
            OUTPUT_NAME ${STATIC_LIBRARY_BASE_NAME})

    if (ZSTD_FRAMEWORK)
        set_target_properties(
                libzstd_static
                PROPERTIES
                FRAMEWORK TRUE
                FRAMEWORK_VERSION "${ZSTD_FULL_VERSION}"
                PRODUCT_BUNDLE_IDENTIFIER "github.com/facebook/zstd/${STATIC_LIBRARY_BASE_NAME}"
                XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
                PUBLIC_HEADER "${PublicHeaders}"
                OUTPUT_NAME "${STATIC_LIBRARY_BASE_NAME}"
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
                XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
                XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
                MACOSX_FRAMEWORK_IDENTIFIER "github.com/facebook/zstd/${STATIC_LIBRARY_BASE_NAME}"
                MACOSX_FRAMEWORK_BUNDLE_VERSION "${ZSTD_FULL_VERSION}"
                MACOSX_FRAMEWORK_SHORT_VERSION_STRING "${ZSTD_SHORT_VERSION}"
                MACOSX_RPATH TRUE
                RESOURCE ${PublicHeaders})
    endif ()
endif ()

# pkg-config
include(JoinPaths) # can be replaced by cmake_path(APPEND) in CMake 3.20
set(PREFIX "${CMAKE_INSTALL_PREFIX}")
set(EXEC_PREFIX "\${prefix}")
join_paths(LIBDIR "\${exec_prefix}" "${CMAKE_INSTALL_LIBDIR}")
join_paths(INCLUDEDIR "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")
set(LIBS_PRIVATE "${THREADS_LIBS}")
set(VERSION "${zstd_VERSION}")

configure_file("${LIBRARY_DIR}/libzstd.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/libzstd.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libzstd.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# install target
install(FILES ${PublicHeaders} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(TARGETS ${library_targets}
    EXPORT zstdExports
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT runtime OPTIONAL
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

# uninstall target
if (NOT TARGET uninstall)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)

    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif ()
