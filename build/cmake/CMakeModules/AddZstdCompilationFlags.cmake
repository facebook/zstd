include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.18)
    set(ZSTD_HAVE_CHECK_LINKER_FLAG true)
else ()
    set(ZSTD_HAVE_CHECK_LINKER_FLAG false)
endif ()
if (ZSTD_HAVE_CHECK_LINKER_FLAG)
    include(CheckLinkerFlag)
endif()

function(EnableCompilerFlag _flag _C _CXX _LD)
    string(REGEX REPLACE "\\+" "PLUS" varname "${_flag}")
    string(REGEX REPLACE "[^A-Za-z0-9]+" "_" varname "${varname}")
    string(REGEX REPLACE "^_+" "" varname "${varname}")
    string(TOUPPER "${varname}" varname)
    if (_C)
        CHECK_C_COMPILER_FLAG(${_flag} C_FLAG_${varname})
        if (C_FLAG_${varname})
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_flag}" PARENT_SCOPE)
        endif ()
    endif ()
    if (_CXX)
        CHECK_CXX_COMPILER_FLAG(${_flag} CXX_FLAG_${varname})
        if (CXX_FLAG_${varname})
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_flag}" PARENT_SCOPE)
        endif ()
    endif ()
    if (_LD)
        # We never add a linker flag with CMake < 3.18. We will
        # implement CHECK_LINKER_FLAG() like feature for CMake < 3.18
        # or require CMake >= 3.18 when we need to add a required
        # linker flag in future.
        #
        # We also skip linker flags check for MSVC compilers (which includes
        # clang-cl) since currently check_linker_flag() doesn't give correct
        # results for this configuration,
        # see: https://gitlab.kitware.com/cmake/cmake/-/issues/22023
        if (ZSTD_HAVE_CHECK_LINKER_FLAG AND NOT MSVC)
            CHECK_LINKER_FLAG(C ${_flag} LD_FLAG_${varname})
        else ()
            set(LD_FLAG_${varname} false)
        endif ()
        if (LD_FLAG_${varname})
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_flag}" PARENT_SCOPE)
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_flag}" PARENT_SCOPE)
        endif ()
    endif ()
endfunction()

macro(ADD_ZSTD_COMPILATION_FLAGS)
    # We set ZSTD_HAS_NOEXECSTACK if we are certain we've set all the required
    # compiler flags to mark the stack as non-executable.
    set(ZSTD_HAS_NOEXECSTACK false)

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR MINGW) #Not only UNIX but also WIN32 for MinGW
        # It's possible to select the exact standard used for compilation.
        # It's not necessary, but can be employed for specific purposes.
        # Note that zstd source code is compatible with both C++98 and above
        # and C-gnu90 (c90 + long long + variadic macros ) and above
        # EnableCompilerFlag("-std=c++11" false true) # Set C++ compilation to c++11 standard
        # EnableCompilerFlag("-std=c99" true false)   # Set C compilation to c99 standard
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MSVC)
            # clang-cl normally maps -Wall to -Weverything.
            EnableCompilerFlag("/clang:-Wall" true true false)
        else ()
            EnableCompilerFlag("-Wall" true true false)
        endif ()
        EnableCompilerFlag("-Wextra" true true false)
        EnableCompilerFlag("-Wundef" true true false)
        EnableCompilerFlag("-Wshadow" true true false)
        EnableCompilerFlag("-Wcast-align" true true false)
        EnableCompilerFlag("-Wcast-qual" true true false)
        EnableCompilerFlag("-Wstrict-prototypes" true false false)
        # Enable asserts in Debug mode
        if (CMAKE_BUILD_TYPE MATCHES "Debug")
            EnableCompilerFlag("-DDEBUGLEVEL=1" true true false)
        endif ()
        # Add noexecstack flags
        # LDFLAGS
        EnableCompilerFlag("-Wl,-z,noexecstack" false false true)
        # CFLAGS & CXXFLAGS
        EnableCompilerFlag("-Qunused-arguments" true true false)
        EnableCompilerFlag("-Wa,--noexecstack" true true false)
        # NOTE: Using 3 nested ifs because the variables are sometimes
        # empty if the condition is false, and sometimes equal to false.
        # This implicitly converts them to truthy values. There may be
        # a better way to do this, but this reliably works.
        if (${LD_FLAG_WL_Z_NOEXECSTACK})
            if (${C_FLAG_WA_NOEXECSTACK})
                if (${CXX_FLAG_WA_NOEXECSTACK})
                    # We've succeeded in marking the stack as non-executable
                    set(ZSTD_HAS_NOEXECSTACK true)
                endif()
            endif()
        endif()
    elseif (MSVC) # Add specific compilation flags for Windows Visual

        set(ACTIVATE_MULTITHREADED_COMPILATION "ON" CACHE BOOL "activate multi-threaded compilation (/MP flag)")
        if (CMAKE_GENERATOR MATCHES "Visual Studio" AND ACTIVATE_MULTITHREADED_COMPILATION)
            EnableCompilerFlag("/MP" true true false)
        endif ()

        # UNICODE SUPPORT
        EnableCompilerFlag("/D_UNICODE" true true false)
        EnableCompilerFlag("/DUNICODE" true true false)
        # Enable asserts in Debug mode
        if (CMAKE_BUILD_TYPE MATCHES "Debug")
            EnableCompilerFlag("/DDEBUGLEVEL=1" true true false)
        endif ()
    endif ()

    # Remove duplicates compilation flags
    foreach (flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
             CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
             CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if( ${flag_var} )
            separate_arguments(${flag_var})
            string(REPLACE ";" " " ${flag_var} "${${flag_var}}")
        endif()
    endforeach ()

    if (MSVC AND ZSTD_USE_STATIC_RUNTIME)
        foreach (flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                 CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                 CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                 CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if ( ${flag_var} )
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif()
        endforeach ()
    endif ()

endmacro()
