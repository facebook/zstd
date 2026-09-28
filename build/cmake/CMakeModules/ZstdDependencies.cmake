# ################################################################
# ZSTD Dependencies Configuration
# ################################################################

# Function to handle HP-UX thread configuration
function(setup_hpux_threads)
    find_package(Threads)
    if(NOT Threads_FOUND)
        set(CMAKE_USE_PTHREADS_INIT 1 PARENT_SCOPE)
        set(CMAKE_THREAD_LIBS_INIT -lpthread PARENT_SCOPE)
        set(CMAKE_HAVE_THREADS_LIBRARY 1 PARENT_SCOPE)
        set(Threads_FOUND TRUE PARENT_SCOPE)
    endif()
endfunction()

# Configure threading support
if(ZSTD_MULTITHREAD_SUPPORT AND UNIX)
    if(CMAKE_SYSTEM_NAME MATCHES "HP-UX")
        setup_hpux_threads()
    else()
        set(THREADS_PREFER_PTHREAD_FLAG ON)
        find_package(Threads REQUIRED)
    endif()
    
    if(CMAKE_USE_PTHREADS_INIT)
        set(THREADS_LIBS "${CMAKE_THREAD_LIBS_INIT}")
    else()
        message(SEND_ERROR "ZSTD currently does not support thread libraries other than pthreads")
    endif()
endif()
