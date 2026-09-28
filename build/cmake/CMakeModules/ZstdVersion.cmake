# ################################################################
# ZSTD Version Configuration
# ################################################################

# Setup CMake policy version
set(ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION "3")
set(ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION "13")

# Determine appropriate policy version
if("${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}" EQUAL "${CMAKE_MAJOR_VERSION}" AND
   "${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}" GREATER "${CMAKE_MINOR_VERSION}")
    set(ZSTD_CMAKE_POLICY_VERSION "${CMAKE_VERSION}")
else()
    set(ZSTD_CMAKE_POLICY_VERSION "${ZSTD_MAX_VALIDATED_CMAKE_MAJOR_VERSION}.${ZSTD_MAX_VALIDATED_CMAKE_MINOR_VERSION}.0")
endif()

cmake_policy(VERSION ${ZSTD_CMAKE_POLICY_VERSION})

# Parse version from header file
include(GetZstdLibraryVersion)
GetZstdLibraryVersion(${LIBRARY_DIR}/zstd.h zstd_VERSION_MAJOR zstd_VERSION_MINOR zstd_VERSION_PATCH)

# Set version variables
set(ZSTD_SHORT_VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}")
set(ZSTD_FULL_VERSION "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")

# Project metadata
set(zstd_HOMEPAGE_URL "https://facebook.github.io/zstd")
set(zstd_DESCRIPTION "Zstandard is a real-time compression algorithm, providing high compression ratios.")

message(STATUS "ZSTD VERSION: ${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")
