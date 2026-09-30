#######################################################
#############             CPPCHECK            #############
#######################################################
option (ENABLE_CPPCHECK "Build with CppCheck"  FALSE)
if (${ENABLE_CPPCHECK})
    if (CMAKE_VERSION VERSION_LESS 3.10)
        message (FATAL_ERROR "Current CMake Version (${CMAKE_VERSION}) is less than 3.10 , which doesn't support Cppcheck")
    endif ()
    # Disable CCache or any launcher if CPPCHECK enabled
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")
    include (findcppcheck)
    if (CPPCHECK_FOUND)
        configure_file("${CMAKE_SOURCE_DIR}/ut/static_check/cppcheck_suppressions.txt.in" ${CMAKE_BINARY_DIR}/cppcheck_suppressions.txt)
        set (CMAKE_CXX_CPPCHECK ${CPPCHECK_EXECUTABLE})

        #Fix Me
        #set (CMAKE_C_CPPCHECK ${CPPCHECK_EXECUTABLE})
        message ("-- Cppcheck: ${CPPCHECK_VERSION}")
        if (CMAKE_CXX_CPPCHECK)
            list(
                APPEND CMAKE_CXX_CPPCHECK 
                "--enable=warning,performance,portability"
                "--inconclusive"
                "--force" 
                "--inline-suppr"
                "--template=gcc"
                "--suppressions-list=${CMAKE_BINARY_DIR}/cppcheck_suppressions.txt"
                )
        endif()

        if (CMAKE_C_CPPCHECK)
            list(
                APPEND CMAKE_C_CPPCHECK 
                "--enable=warning"
                "--inconclusive"
                "--force" 
                "--inline-suppr"
                "--template=gcc"
                "--suppressions-list=${CMAKE_BINARY_DIR}/cppcheck_suppressions.txt"
                )
        endif()
    else ()
        message (FATAL_ERROR "-- Cppcheck: Not Find")
    endif ()
endif ()

#######################################################
#############             CPPLINT            #############
#######################################################
option (ENABLE_CPPLINT "Build with CPPLINT"  FALSE)
if (${ENABLE_CPPLINT})
    if (CMAKE_VERSION VERSION_LESS 3.8)
        message (FATAL_ERROR "Current CMake Version (${CMAKE_VERSION}) is less than 3.8 , which doesn't support CPPLINT")
    endif ()
    # Disable CCache or any launcher if CPPLINT enabled
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")
    include (findcpplint)
    if (CPPLINT_FOUND)
        set (CMAKE_CXX_CPPLINT ${CPPLINT_EXECUTABLE})
        set (CMAKE_C_CPPLINT ${CPPLINT_EXECUTABLE})
        message ("-- Cpplint: ${CPPLINT_VERSION}")
    endif ()
endif ()

#######################################################
#############     Clang Static Checker    #############
#######################################################
option (ENABLE_CLANG_SC "Build with Clang Static Checker"  TRUE)

if (${ENABLE_CLANG_SC})
    function(add_clang_static_analysis target)
        get_target_property(SRCs ${target} SOURCES)
        add_library(${target}_analyze OBJECT EXCLUDE_FROM_ALL ${SRCs})
        set_target_properties(${target}_analyze PROPERTIES
            COMPILE_OPTIONS "--analyze"
            EXCLUDE_FROM_DEFAULT_BUILD true)
    endfunction()
else ()
    function(add_clang_static_analysis target)
    endfunction()
endif ()
