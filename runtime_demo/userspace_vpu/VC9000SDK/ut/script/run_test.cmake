execute_process (COMMAND hostname OUTPUT_VARIABLE hostname OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NOT DEFINED SOURCE_TREE)
    message (FATAL_ERROR "please specify -DSOURCE_TREE=")
endif ()

set(CTEST_SOURCE_DIRECTORY "${SOURCE_TREE}")
set(CTEST_BINARY_DIRECTORY "${SOURCE_TREE}/build")

set(CTEST_SITE "${hostname}")
set(CTEST_CMAKE_GENERATOR "cmake")
set(CTEST_COVERAGE_COMMAND "gcov")
set(CTEST_BUILD_NAME "lowlevel_coverage")
set(CTEST_LABELS_FOR_SUBPROJECTS "lowlevel_coverage")
message(STATUS "Host is ${CMAKE_SYSTEM_PROCESSOR}, copying files ...")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    execute_process (COMMAND bash -c "rm -rf dmesg_log.txt && touch dmesg_log.txt && rm -rf /video-case/lowlevel_SDK/res_UT/funcs_align_out/*")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64|armv7l")
    execute_process (COMMAND bash -c "rm -rf dmesg_log.txt && touch dmesg_log.txt && rm -rf /video-case/lowlevel_SDK/res_UT/funcs_align_out_arm64/* ")
else()
    message(STATUS "Host processor: ${CMAKE_SYSTEM_PROCESSOR} is not support")
endif()
message(STATUS "Copy test files")
execute_process (COMMAND bash -c "[ ! -d /opt/funcs_align_src/ ] && { echo 'no /opt/funcs_align_src/, files copying ...' && mkdir -p /opt/funcs_align_src/ /opt/output_UT/yuv /opt/output_UT/funcs_align_out && cp /video-case/lowlevel_SDK/res_UT/funcs_align_src/1080p_nv12_* /video-case/lowlevel_SDK/res_UT/funcs_align_src/Park* /video-case/lowlevel_SDK/res_UT/funcs_align_src/1080p.rgb /video-case/lowlevel_SDK/res_UT/resolutions_yuv/3840x2160.yuv /video-case/lowlevel_SDK/res_UT/resolutions_multicore/cdzj_multicore_8k.yuv /opt/funcs_align_src/; } || echo '/opt/funcs_align_src/ is exists, skip copying files'")

set(CTEST_NOTES_FILES "dmesg_log.txt")
set(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 5000000)
set(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 5000000)
set(TEST_PASS "0")
set(TEST_SKIP "0")
set(TEST_FAIL "0")
set(TEST_COST "0")
set(CASE_COST "0")

set(CTEST_NIGHTLY_START_TIME "15:02:03 UTC")
ctest_start("Continuous")
ctest_submit(PARTS Notes SUBMIT_URL http://192.168.30.90:32181/submit.php?project=unitest RETURN_VALUE submit_result)
string(TIMESTAMP _begin_test "%s")
message("Test .......")
string(TIMESTAMP _begin_case "%s")
# ^lowlevel*
ctest_test(INCLUDE unitest RETURN_VALUE test_result CAPTURE_CMAKE_ERROR cmake_err)
execute_process (COMMAND bash -c "dmesg -T >> dmesg_log.txt")
ctest_submit(PARTS Update SUBMIT_URL http://192.168.30.90:32181/submit.php?project=unitest RETURN_VALUE submit_result)
string(TIMESTAMP _end_case "%s")
math(EXPR CASE_COST "${_end_case} - ${_begin_case}")
ctest_submit(PARTS Test SUBMIT_URL http://192.168.30.90:32181/submit.php?project=unitest RETURN_VALUE submit_result)

# generate a GCOV tarball and upload it to CDash.
include(CTestCoverageCollectGCOV)
ctest_coverage_collect_gcov(
  TARBALL gcov.tar
  SOURCE ${CTEST_SOURCE_DIRECTORY}
  BUILD ${CTEST_BINARY_DIRECTORY}
  GCOV_COMMAND ${CTEST_COVERAGE_COMMAND}
)
if(EXISTS "${CTEST_BINARY_DIRECTORY}/gcov.tar")
  message("=====upload gcov result=======")
  ctest_submit(CDASH_UPLOAD "${CTEST_BINARY_DIRECTORY}/gcov.tar"
    CDASH_UPLOAD_TYPE GcovTar)
endif()
