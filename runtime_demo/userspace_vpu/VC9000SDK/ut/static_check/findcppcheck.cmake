# - Find Cppcheck
# this module looks for Cppcheck
#
#  CPPCHECK_EXECUTABLE - the full path to Cppcheck
#  CPPCHECK_FOUND      - If false, don't attempt to use Cppcheck.

include (FindPackageHandleStandardArgs)

find_program (CPPCHECK_EXECUTABLE
              NAMES cppcheck
              PATHS ${CPPCHECK_POSSIBLE_BIN_PATHS})

if (CPPCHECK_EXECUTABLE)
    execute_process (COMMAND ${CPPCHECK_EXECUTABLE} --version
                             OUTPUT_VARIABLE CPPCHECK_VERSION_OUTPUT_VARIABLE
                             RESULT_VARIABLE CPPCHECK_VERSION_RESULT_VARIABLE
                             ERROR_QUIET
                             OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT CPPCHECK_VERSION_RESULT_VARIABLE)
        string (REGEX REPLACE "Cppcheck ([0-9]+.[0-9]+.[0-9]+).*" "\\1" CPPCHECK_VERSION "${CPPCHECK_VERSION_OUTPUT_VARIABLE}")
    endif ()
endif ()

# handle the QUIETLY and REQUIRED arguments and set Cppcheck_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS (cppcheck
                                   REQUIRED_VARS CPPCHECK_EXECUTABLE
                                   VERSION_VAR   CPPCHECK_VERSION)

mark_as_advanced (CPPCHECK_EXECUTABLE)
