# ################################################################
# ZSTD Build Targets Configuration
# ################################################################

# Always build the library first (this defines ZSTD_BUILD_STATIC/SHARED options)
add_subdirectory(lib)

# Validate build configuration after lib options are defined
if(ZSTD_BUILD_PROGRAMS)
    if(NOT ZSTD_BUILD_STATIC AND NOT ZSTD_PROGRAMS_LINK_SHARED)
        message(SEND_ERROR "Static library required to build zstd CLI programs")
    elseif(NOT ZSTD_BUILD_SHARED AND ZSTD_PROGRAMS_LINK_SHARED)
        message(SEND_ERROR "Shared library required to build zstd CLI programs")
    endif()
endif()

if(ZSTD_BUILD_TESTS AND NOT ZSTD_BUILD_STATIC)
    message(SEND_ERROR "Static library required to build test suite")
endif()

# Add programs if requested
if(ZSTD_BUILD_PROGRAMS)
    add_subdirectory(programs)
endif()

# Add tests if requested
if(ZSTD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Add contrib utilities if requested
if(ZSTD_BUILD_CONTRIB)
    add_subdirectory(contrib)
endif()

# Clean-all target for thorough cleanup
add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/
    COMMENT "Performing complete clean including build directory"
)
