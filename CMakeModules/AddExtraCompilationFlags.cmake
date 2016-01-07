MACRO(ADD_EXTRA_COMPILATION_FLAGS)
    include(CheckCXXCompilerFlag)
    include(CheckCCompilerFlag)
    if (CMAKE_COMPILER_IS_GNUCXX OR MINGW) #Not only UNIX but also WIN32 for MinGW

        set(POSITION_INDEPENDENT_CODE_FLAG "-fPIC")
        CHECK_C_COMPILER_FLAG(${POSITION_INDEPENDENT_CODE_FLAG} POSITION_INDEPENDENT_CODE_FLAG_ALLOWED)
        if (POSITION_INDEPENDENT_CODE_FLAG_ALLOWED)
            MESSAGE("Compiler flag ${POSITION_INDEPENDENT_CODE_FLAG} allowed")
            set(ACTIVATE_POSITION_INDEPENDENT_CODE_FLAG "ON" CACHE BOOL "activate -fPIC flag")
        else ()
            MESSAGE("Compiler flag ${POSITION_INDEPENDENT_CODE_FLAG} not allowed")
        endif (POSITION_INDEPENDENT_CODE_FLAG_ALLOWED)

        set(WARNING_UNDEF "-Wundef")
        CHECK_C_COMPILER_FLAG(${WARNING_UNDEF} WARNING_UNDEF_ALLOWED)
        if (WARNING_UNDEF_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_UNDEF} allowed")
            set(ACTIVATE_WARNING_UNDEF "ON" CACHE BOOL "activate -Wundef flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_UNDEF} not allowed")
        endif (WARNING_UNDEF_ALLOWED)

        set(WARNING_SHADOW "-Wshadow")
        CHECK_C_COMPILER_FLAG(${WARNING_SHADOW} WARNING_SHADOW_ALLOWED)
        if (WARNING_SHADOW_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_SHADOW} allowed")
            set(ACTIVATE_WARNING_SHADOW "ON" CACHE BOOL "activate -Wshadow flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_SHADOW} not allowed")
        endif (WARNING_SHADOW_ALLOWED)

        set(WARNING_CAST_ALIGN "-Wcast-align")
        CHECK_C_COMPILER_FLAG(${WARNING_CAST_ALIGN} WARNING_CAST_ALIGN_ALLOWED)
        if (WARNING_CAST_ALIGN_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_CAST_ALIGN} allowed")
            set(ACTIVATE_WARNING_CAST_ALIGN "ON" CACHE BOOL "activate -Wcast-align flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_CAST_ALIGN} not allowed")
        endif (WARNING_CAST_ALIGN_ALLOWED)

        set(WARNING_CAST_QUAL "-Wcast-qual")
        CHECK_C_COMPILER_FLAG(${WARNING_CAST_QUAL} WARNING_CAST_QUAL_ALLOWED)
        if (WARNING_CAST_QUAL_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_CAST_QUAL} allowed")
            set(ACTIVATE_WARNING_CAST_QUAL "ON" CACHE BOOL "activate -Wcast-qual flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_CAST_QUAL} not allowed")
        endif (WARNING_CAST_QUAL_ALLOWED)

        set(WARNING_STRICT_PROTOTYPES "-Wstrict-prototypes")
        CHECK_C_COMPILER_FLAG(${WARNING_STRICT_PROTOTYPES} WARNING_STRICT_PROTOTYPES_ALLOWED)
        if (WARNING_STRICT_PROTOTYPES_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_STRICT_PROTOTYPES} allowed")
            set(ACTIVATE_WARNING_STRICT_PROTOTYPES "ON" CACHE BOOL "activate -Wstrict-prototypes flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_STRICT_PROTOTYPES} not allowed")
        endif (WARNING_STRICT_PROTOTYPES_ALLOWED)

        set(WARNING_ALL "-Wall")
        CHECK_C_COMPILER_FLAG(${WARNING_ALL} WARNING_ALL_ALLOWED)
        if (WARNING_ALL_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_ALL} allowed")
            set(ACTIVATE_WARNING_ALL "ON" CACHE BOOL "activate -Wall flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_ALL} not allowed")
        endif (WARNING_ALL_ALLOWED)

        set(WARNING_EXTRA "-Wextra")
        CHECK_C_COMPILER_FLAG(${WARNING_EXTRA} WARNING_EXTRA_ALLOWED)
        if (WARNING_EXTRA_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_EXTRA} allowed")
            set(ACTIVATE_WARNING_EXTRA "ON" CACHE BOOL "activate -Wextra flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_EXTRA} not allowed")
        endif (WARNING_EXTRA_ALLOWED)

        set(WARNING_FLOAT_EQUAL "-Wfloat-equal")
        CHECK_C_COMPILER_FLAG(${WARNING_FLOAT_EQUAL} WARNING_FLOAT_EQUAL_ALLOWED)
        if (WARNING_FLOAT_EQUAL_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_FLOAT_EQUAL} allowed")
            set(ACTIVATE_WARNING_FLOAT_EQUAL "OFF" CACHE BOOL "activate -Wfloat-equal flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_FLOAT_EQUAL} not allowed")
        endif (WARNING_FLOAT_EQUAL_ALLOWED)

        set(WARNING_SIGN_CONVERSION "-Wsign-conversion")
        CHECK_C_COMPILER_FLAG(${WARNING_SIGN_CONVERSION} WARNING_SIGN_CONVERSION_ALLOWED)
        if (WARNING_SIGN_CONVERSION_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_SIGN_CONVERSION} allowed")
            set(ACTIVATE_WARNING_SIGN_CONVERSION "OFF" CACHE BOOL "activate -Wsign-conversion flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_SIGN_CONVERSION} not allowed")
        endif (WARNING_SIGN_CONVERSION_ALLOWED)

        if (ACTIVATE_POSITION_INDEPENDENT_CODE_FLAG)
            list(APPEND CMAKE_C_FLAGS ${POSITION_INDEPENDENT_CODE_FLAG})
        else ()
            string(REPLACE ${POSITION_INDEPENDENT_CODE_FLAG} "" CMAKE_C_FLAGS "${POSITION_INDEPENDENT_CODE_FLAG}")
        endif (ACTIVATE_POSITION_INDEPENDENT_CODE_FLAG)

        if (ACTIVATE_WARNING_UNDEF)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_UNDEF})
            list(APPEND CMAKE_C_FLAGS ${WARNING_UNDEF})
        else ()
            string(REPLACE ${WARNING_UNDEF} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_UNDEF} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_UNDEF)

        if (ACTIVATE_WARNING_SHADOW)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_SHADOW})
            list(APPEND CMAKE_C_FLAGS ${WARNING_SHADOW})
        else ()
            string(REPLACE ${WARNING_SHADOW} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_SHADOW} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_SHADOW)

        if (ACTIVATE_WARNING_CAST_QUAL)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_CAST_QUAL})
            list(APPEND CMAKE_C_FLAGS ${WARNING_CAST_QUAL})
        else ()
            string(REPLACE ${WARNING_CAST_QUAL} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_CAST_QUAL} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_CAST_QUAL)

        if (ACTIVATE_WARNING_CAST_ALIGN)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_CAST_ALIGN})
            list(APPEND CMAKE_C_FLAGS ${WARNING_CAST_ALIGN})
        else ()
            string(REPLACE ${WARNING_CAST_ALIGN} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_CAST_ALIGN} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_CAST_ALIGN)

        if (ACTIVATE_WARNING_STRICT_PROTOTYPES)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_STRICT_PROTOTYPES})
            list(APPEND CMAKE_C_FLAGS ${WARNING_STRICT_PROTOTYPES})
        else ()
            string(REPLACE ${WARNING_STRICT_PROTOTYPES} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_STRICT_PROTOTYPES} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_STRICT_PROTOTYPES)

        if (ACTIVATE_WARNING_ALL)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_ALL})
            list(APPEND CMAKE_C_FLAGS ${WARNING_ALL})
        else ()
            string(REPLACE ${WARNING_ALL} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_ALL} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_ALL)

        if (ACTIVATE_WARNING_EXTRA)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_EXTRA})
            list(APPEND CMAKE_C_FLAGS ${WARNING_EXTRA})
        else ()
            string(REPLACE ${WARNING_EXTRA} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_EXTRA} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_EXTRA)

        if (ACTIVATE_WARNING_FLOAT_EQUAL)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_FLOAT_EQUAL})
            list(APPEND CMAKE_C_FLAGS ${WARNING_FLOAT_EQUAL})
        else ()
            string(REPLACE ${WARNING_FLOAT_EQUAL} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_FLOAT_EQUAL} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_FLOAT_EQUAL)

        if (ACTIVATE_WARNING_SIGN_CONVERSION)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_SIGN_CONVERSION})
            list(APPEND CMAKE_C_FLAGS ${WARNING_SIGN_CONVERSION})
        else ()
            string(REPLACE ${WARNING_SIGN_CONVERSION} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_SIGN_CONVERSION} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_WARNING_SIGN_CONVERSION)

        #Set c++11 by default
        list(APPEND CMAKE_CXX_FLAGS "-std=c++11")

        #Set c99 by default
        list(APPEND CMAKE_C_FLAGS "-std=c99")

    elseif (MSVC)
        # Add specific compilation flags for Windows Visual

        set(WARNING_ALL "/Wall")
        CHECK_C_COMPILER_FLAG(${WARNING_ALL} WARNING_ALL_ALLOWED)
        if (WARNING_ALL_ALLOWED)
            MESSAGE("Compiler flag ${WARNING_ALL} allowed")
            set(ACTIVATE_WARNING_ALL "OFF" CACHE BOOL "activate /Wall flag")
        else ()
            MESSAGE("Compiler flag ${WARNING_ALL} not allowed")
        endif (WARNING_ALL_ALLOWED)

        if (ACTIVATE_WARNING_ALL)
            list(APPEND CMAKE_CXX_FLAGS ${WARNING_ALL})
            list(APPEND CMAKE_C_FLAGS ${WARNING_ALL})
        else ()
            string(REPLACE ${WARNING_ALL} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${WARNING_ALL} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif ()
        if (MSVC80 OR MSVC90 OR MSVC10 OR MSVC11)
            # To avoid compiler warning (level 4) C4571, compile with /EHa if you still want
            # your catch(...) blocks to catch structured exceptions.
            list(APPEND CMAKE_CXX_FLAGS "/EHa")
        endif (MSVC80 OR MSVC90 OR MSVC10 OR MSVC11)

        set(MULTITHREADED_COMPILATION "/MP")
        MESSAGE("Compiler flag ${MULTITHREADED_COMPILATION} allowed")
        set(ACTIVATE_MULTITHREADED_COMPILATION "ON" CACHE BOOL "activate /MP flag")

        if (ACTIVATE_MULTITHREADED_COMPILATION)
            list(APPEND CMAKE_CXX_FLAGS ${MULTITHREADED_COMPILATION})
            list(APPEND CMAKE_C_FLAGS ${MULTITHREADED_COMPILATION})
        else ()
            string(REPLACE ${MULTITHREADED_COMPILATION} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            string(REPLACE ${MULTITHREADED_COMPILATION} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif (ACTIVATE_MULTITHREADED_COMPILATION)

        #For exceptions
        list(APPEND CMAKE_CXX_FLAGS "/EHsc")
    endif ()

    # Remove duplicates compilation flags
    separate_arguments(CMAKE_CXX_FLAGS)
    list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS)
    string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "common C++ build flags" FORCE)

    separate_arguments(CMAKE_C_FLAGS)
    list(REMOVE_DUPLICATES CMAKE_C_FLAGS)
    string(REPLACE ";" " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "common C build flags" FORCE)

    if (MSVC)
        # Replace /MT to /MD flag
        FOREACH (flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            STRING(REGEX REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
        ENDFOREACH (flag_var)

        FOREACH (flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
            STRING(REGEX REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
        ENDFOREACH (flag_var)
    endif ()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "Updated flags" FORCE)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}" CACHE STRING "Updated flags" FORCE)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}" CACHE STRING "Updated flags" FORCE)

ENDMACRO(ADD_EXTRA_COMPILATION_FLAGS)
