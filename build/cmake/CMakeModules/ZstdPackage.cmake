# ################################################################
# ZSTD Package Configuration
# ################################################################

include(CMakePackageConfigHelpers)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfigVersion.cmake"
    VERSION ${zstd_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Configure package for installation
set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/zstd)

foreach(target_suffix IN ITEMS "_shared" "_static" "")
    if(TARGET "libzstd${target_suffix}")
        # Export targets for build directory
        export(EXPORT "zstdExports${target_suffix}"
                FILE "${CMAKE_CURRENT_BINARY_DIR}/zstdTargets${target_suffix}.cmake"
                NAMESPACE zstd::
        )
        # Install exported targets
        install(EXPORT "zstdExports${target_suffix}"
                FILE "zstdTargets${target_suffix}.cmake"
                NAMESPACE zstd::
                DESTINATION ${ConfigPackageLocation}
        )
    endif()
endforeach()

# Configure and install package config file
configure_package_config_file(
    zstdConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfig.cmake"
    INSTALL_DESTINATION ${ConfigPackageLocation}
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/zstdConfigVersion.cmake"
    DESTINATION ${ConfigPackageLocation}
)
