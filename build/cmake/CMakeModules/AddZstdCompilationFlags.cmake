include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

function(EnableCompilerFlag _flag _C _CXX)
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
endfunction()

MACRO(ADD_ZSTD_COMPILATION_FLAGS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR MINGW) #Not only UNIX but also WIN32 for MinGW
        #Set c++11 by default
        EnableCompilerFlag("-std=c++11" false true)
        #Set c99 by default
        EnableCompilerFlag("-std=c99" true false)
        EnableCompilerFlag("-Wall" true true)
        EnableCompilerFlag("-Wextra" true true)
        EnableCompilerFlag("-Wundef" true true)
        EnableCompilerFlag("-Wshadow" true true)
        EnableCompilerFlag("-Wcast-align" true true)
        EnableCompilerFlag("-Wcast-qual" true true)
        EnableCompilerFlag("-Wstrict-prototypes" true false)
    elseif (MSVC) # Add specific compilation flags for Windows Visual
        EnableCompilerFlag("/Wall" true true)

        # Only for DEBUG version
        EnableCompilerFlag("/RTC1" true true)
        EnableCompilerFlag("/Zc:forScope" true true)
        EnableCompilerFlag("/Gd" true true)
        EnableCompilerFlag("/analyze:stacksize25000" true true)

        if (MSVC80 OR MSVC90 OR MSVC10 OR MSVC11)
            # To avoid compiler warning (level 4) C4571, compile with /EHa if you still want
            # your catch(...) blocks to catch structured exceptions.
            EnableCompilerFlag("/EHa" false true)
        endif (MSVC80 OR MSVC90 OR MSVC10 OR MSVC11)

        set(ACTIVATE_MULTITHREADED_COMPILATION "ON" CACHE BOOL "activate multi-threaded compilation (/MP flag)")
        if (ACTIVATE_MULTITHREADED_COMPILATION)
            EnableCompilerFlag("/MP" true true)
        endif ()

        #For exceptions
        EnableCompilerFlag("/EHsc" true true)
        
        # UNICODE SUPPORT
        EnableCompilerFlag("/D_UNICODE" true true)
        EnableCompilerFlag("/DUNICODE" true true)
    endif ()

    # Remove duplicates compilation flags
    FOREACH (flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
             CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
             CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        separate_arguments(${flag_var})
        list(REMOVE_DUPLICATES ${flag_var})
        string(REPLACE ";" " " ${flag_var} "${${flag_var}}")
    ENDFOREACH (flag_var)

    if (MSVC)
        # Replace /MT to /MD flag
        # Replace /O2 to /O3 flag
        FOREACH (flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                 CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                 CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                 CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            STRING(REGEX REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
            STRING(REGEX REPLACE "/O2" "/Ox" ${flag_var} "${${flag_var}}")
        ENDFOREACH (flag_var)
    endif ()

ENDMACRO(ADD_ZSTD_COMPILATION_FLAGS)
