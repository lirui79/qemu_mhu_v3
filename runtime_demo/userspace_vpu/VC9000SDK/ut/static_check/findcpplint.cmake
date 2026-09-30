# - Find Cpplint
# this module looks for Cpplint
#
#  CPPLINT_EXECUTABLE - the full path to Cpplint
#  CPPLINT_FOUND      - If false, don't attempt to use Cpplint.

include (FindPackageHandleStandardArgs)

find_program (CPPLINT_EXECUTABLE
              NAMES cpplint
              PATHS ${CPPLINT_POSSIBLE_BIN_PATHS})

if (CPPLINT_EXECUTABLE)
    execute_process (COMMAND ${CPPLINT_EXECUTABLE} --version
                             OUTPUT_VARIABLE CPPLINT_VERSION_OUTPUT_VARIABLE
                             RESULT_VARIABLE CPPLINT_VERSION_RESULT_VARIABLE
                             ERROR_QUIET
                             OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT CPPLINT_VERSION_RESULT_VARIABLE)
        string (REGEX REPLACE "Cpplint fork.*cpplint ([0-9]+.[0-9]+.[0-9]+).*" "\\1" CPPLINT_VERSION "${CPPLINT_VERSION_OUTPUT_VARIABLE}")
    endif ()
endif ()

# handle the QUIETLY and REQUIRED arguments and set Cpplint_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS (cpplint
                                   REQUIRED_VARS CPPLINT_EXECUTABLE
                                   VERSION_VAR   CPPLINT_VERSION)

mark_as_advanced (CPPLINT_EXECUTABLE)
