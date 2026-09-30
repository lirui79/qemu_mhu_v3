execute_process (COMMAND hostname OUTPUT_VARIABLE hostname OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NOT DEFINED SOURCE_TREE)
    message (FATAL_ERROR "please specify -DSOURCE_TREE=")
endif ()
if (NOT DEFINED TYPE)
    message (FATAL_ERROR "please specify -DTYPE=CPPCHECK|CPPLINT")
endif ()

set(CTEST_SOURCE_DIRECTORY "${SOURCE_TREE}")
set(CTEST_BINARY_DIRECTORY "${SOURCE_TREE}/build")

set(CTEST_SITE "${hostname}")


set(CTEST_BUILD_CONFIGURATION 0)
set(CTEST_BUILD_TOOLCHAIN "gcc7")
set(CTEST_BUILD_FLAGS "install")

execute_process (COMMAND uname -s OUTPUT_VARIABLE os OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process (COMMAND uname -i OUTPUT_VARIABLE arch OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CTEST_BUILD_NAME ${TYPE})
set(CTEST_LABELS_FOR_SUBPROJECTS ${TYPE})
set(CTEST_NOTES_FILES "check_log.txt")

set(WITH_MEMCHECK FALSE)
set(WITH_COVERAGE FALSE)

#######################################################################

#ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
execute_process (COMMAND /bin/bash -c "rm -rf ${CTEST_BINARY_DIRECTORY}/*")

find_program(CTEST_GIT_COMMAND NAMES git)
find_program(CTEST_COVERAGE_COMMAND NAMES gcov)
find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)

set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE ${CTEST_SOURCE_DIRECTORY}/tests/valgrind.supp)

if(NOT EXISTS "${CTEST_SOURCE_DIRECTORY}")
    set(CTEST_CHECKOUT_COMMAND "${CTEST_GIT_COMMAND} clone git@192.168.50.15:video/vastai_video_sdk.git ${CTEST_SOURCE_DIRECTORY}")
endif()

#set(CTEST_UPDATE_COMMAND "${CTEST_GIT_COMMAND}")
set(CTEST_UPDATE_VERSION_ONLY ON)

set(CTEST_CONFIGURE_COMMAND "${CMAKE_COMMAND} -DRELEASE_BUILD=${CTEST_BUILD_CONFIGURATION}")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} ${CTEST_SOURCE_DIRECTORY}")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} -DENABLE_${TYPE}=1")
set(CTEST_CONFIGURE_COMMAND "${CTEST_CONFIGURE_COMMAND} -DBUILD_TARGET=0 -DMEM_ONLY_DEV_CHECK=0 -DENABLE_DYNAMIC_RES=1 -DENABLE_UNIT_TEST=1 -DUSING_FFMPEG=1")
set(CTEST_BUILD_COMMAND     "make")
set(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 5000000)
set(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 5000000)

ctest_start("Continuous")

ctest_update(RETURN_VALUE update_result)

ctest_configure(RETURN_VALUE configure_result)
ctest_build(RETURN_VALUE build_result NUMBER_WARNINGS warning_num)
execute_process (COMMAND /bin/bash -c "cp check_log.txt ${CTEST_BINARY_DIRECTORY}")

ctest_submit(PARTS Update SUBMIT_URL http://192.168.30.90:32181/submit.php?project=SDK_Static_Check RETURN_VALUE submit_result)
ctest_submit(PARTS Build SUBMIT_URL http://192.168.30.90:32181/submit.php?project=SDK_Static_Check RETURN_VALUE submit_result)
ctest_submit(PARTS Notes SUBMIT_URL http://192.168.30.90:32181/submit.php?project=SDK_Static_Check RETURN_VALUE submit_result)
ctest_submit(PARTS Done SUBMIT_URL http://192.168.30.90:32181/submit.php?project=SDK_Static_Check RETURN_VALUE submit_result)
if (NOT ${build_result} EQUAL 0)
#    return()
    message (FATAL_ERROR "Build Fail!")
elseif (NOT ${warning_num} EQUAL 0)
    message (FATAL_ERROR "Build Fail! Warning num != 0")
else ()
    message ("Build Pass!")
endif ()
