#######################################################
##############   SOURCE SOURCE SOURCE    ##############
#######################################################
cmake_minimum_required(VERSION 3.12)
project(unitest)
message("-- making vastai_video_sdk origin unitest and sample unitest")

# samples config
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(${CMAKE_C_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	set(CMAKE_COMPILER_IS_CLANG TRUE)
endif()

if(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86|x64|x86_64|amd64|e2k)")
	if(NOT MSVC)
		set(ARCH_SIMD_FLAGS "-mmmx" "-msse" "-msse2")
	endif()
elseif(LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64(le)?")
	set(ARCH_SIMD_DEFINES "-DNO_WARN_X86_INTRINSICS")
	set(ARCH_SIMD_FLAGS "-mvsx")
	add_compile_definitions(NO_WARN_X86_INTRINSICS)
else()
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSIMDE_ENABLE_OPENMP")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSIMDE_ENABLE_OPENMP")
	endif()
	set(ARCH_SIMD_FLAGS "")
	message(STATUS "No Native SSE2 SIMD Support - Using SIMDE")
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wvla -Wno-unused-function -Wno-missing-field-initializers ${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
	set(CMAKE_C_FLAGS "-Wall -Wextra -Wvla -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-braces -Wno-missing-field-initializers ${CMAKE_C_FLAGS} -std=gnu99 -fno-strict-aliasing")

	option(USE_LIBC++ "Use libc++ instead of libstdc++" ${APPLE})
	if(USE_LIBC++)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
elseif(MSVC)
	#TODO
endif()

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
	set(RELEASE_BUILD "1")
elseif(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	set(RELEASE_BUILD "0")
else()
	set(RELEASE_BUILD "0")
	message("Did not specify build type, using default :Debug")
endif()

if(ENABLE_DYNAMIC_RES)
    add_definitions(-DENABLE_DYNAMIC_RES)
endif()

if (ENABLE_ANDROID_ARM64 STREQUAL "1")
    set(PTHREAD_LIB "")
else()
    set(PTHREAD_LIB "pthread")
endif()

if (BUILD_ARM64 STREQUAL "1")
   add_definitions(-DARM64)
endif()

if (USING_FFMPEG STREQUAL "1")
   add_definitions(-DUSING_FFMPEG)
endif()

if(MSVC)
	#TODO
else()
	if(RELEASE_BUILD)
		set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 ${CMAKE_C_FLAGS_RELEASE}")
		set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG=1 -D_NDEBUG=1 ${CMAKE_CXX_FLAGS_RELEASE}")
	else()
		set(CMAKE_C_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
		set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
		add_definitions("-Wall -g")
	endif()

endif()
# samples config

# CATCH2
set(CATCH2_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# HEAD FILE
# set(VMPP_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)
set(VMPP_INC_DIR ${CMAKE_SOURCE_DIR}/inc)
set(VMPP_COMMON_DIR ${CMAKE_SOURCE_DIR}/sample/common)
set(VMPP_TOOL_DIR ${CMAKE_SOURCE_DIR}/sample/tool)
set(FFMPEG_INC_DIR ${CMAKE_SOURCE_DIR}/sample/3rdparty/ffmpeg/include)
set(VMPP_SRC_ENC_INC_DIR ${CMAKE_SOURCE_DIR}/src/enc/inc)
set(VMPP_SRC_SOFTWARE_INC_DIR ${CMAKE_SOURCE_DIR}/src/enc/lib/software/inc)
set(VMPP_SRC_SOURCE_INC_DIR ${CMAKE_SOURCE_DIR}/src/dec/lib/software/source/inc)
set(VMPP_SRC_SOURCE_COMMON_DIR ${CMAKE_SOURCE_DIR}/src/dec/lib/software/source/common)

# samples args
set(SAMPLES_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(OSAL_DIR ${CMAKE_SOURCE_DIR}/src/common/osal)
set(VMPP_LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
set(VIDEO_DEC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)
# set(VMPP_INC_DIR ${VMPP_SDK_ROOT}/inc)
# set(CATCH2_INC_DIR ${CMAKE_SOURCE_DIR}/../include)
# set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)
set(LIBRARY_OUTPUT_PATH ${SAMPLES_SOURCE_DIR}/lib)

set(DEC_SOURCE_DIR ${SAMPLES_SOURCE_DIR}/dec)
set(ENC_SOURCE_DIR ${SAMPLES_SOURCE_DIR}/enc)
set(TRANS_SOURCE_DIR ${SAMPLES_SOURCE_DIR}/transcode)
set(SAMPLES_INC_DIR 
	${VMPP_INC_DIR}
	${VMPP_COMMON_DIR}
	${VMPP_TOOL_DIR}
	${FFMPEG_INC_DIR})
include_directories(${CATCH2_INC_DIR} ${SAMPLES_INC_DIR} ${OSAL_DIR})

if (USING_FFMPEG STREQUAL "1")
    set(FFMPEG_LIBS
        ${CMAKE_SOURCE_DIR}/sample/3rdparty/ffmpeg/lib/libavutil.a
        ${CMAKE_SOURCE_DIR}/sample/3rdparty/ffmpeg/lib/libavcodec.a
        ${CMAKE_SOURCE_DIR}/sample/3rdparty/ffmpeg/lib/libavformat.a
        ${CMAKE_SOURCE_DIR}/sample/3rdparty/ffmpeg/lib/libswscale.a
        )
	set(FFMPEG_INC ${CMAKE_SOURCE_DIR}/sample/3rdparty/ffmpeg/include)
	set(FFMPEG_SRC ${CMAKE_SOURCE_DIR}/sample/common/ffmpeg-wrapper.c)
else()
    set(FFMPEG_LIBS "")
	set(FFMPEG_INC "")
	set(FFMPEG_SRC "")
endif()
# samples args end

include_directories(${CATCH2_INC_DIR} ${VMPP_INC_DIR} ${VMPP_COMMON_DIR})
include_directories(${VMPP_SRC_ENC_INC_DIR} ${VMPP_SRC_SOFTWARE_INC_DIR} ${VMPP_SRC_SOURCE_INC_DIR} ${VMPP_SRC_SOURCE_COMMON_DIR})

set(ut_HEADERS
    ${VMPP_COMMON_DIR}/defs.h
    ${VMPP_COMMON_DIR}/stream.h
    ${VMPP_COMMON_DIR}/utils.h
    ${VMPP_COMMON_DIR}/queue.h
    ${VMPP_COMMON_DIR}/log.h
    ${VMPP_COMMON_DIR}/md5.h
#    ${VMPP_COMMON_DIR}/utils.h
#    ${CMAKE_SOURCE_DIR}/decode.h
#    ${CMAKE_SOURCE_DIR}/encode.h
#    ${CMAKE_SOURCE_DIR}/transcode.h
#    ${VMPP_COMMON_DIR}/queue.h
	)

set(ut_SOURCES
    ${VMPP_COMMON_DIR}/stream.c
    ${VMPP_COMMON_DIR}/utils.c
    ${VMPP_COMMON_DIR}/queue.c
    ${VMPP_COMMON_DIR}/log.c
    ${VMPP_COMMON_DIR}/md5.c
	)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/unitest/)
add_compile_definitions(UT_RES_OUT="/opt/output_UT/")
add_compile_definitions(UT_RES_PATH="/video-case/lowlevel_SDK/res_UT/")
add_definitions("-Wall -g")

if (BUILD_TARGET STREQUAL "0")
	add_executable(${PROJECT_NAME}
		${VMPP_TOOL_DIR}/cJSON.c
		# ${SAMPLES_SOURCE_DIR}/common/utils.c
		${VMPP_COMMON_DIR}/buf_queue.c
		# ${SAMPLES_SOURCE_DIR}/common/stream.c
		${VMPP_COMMON_DIR}/ffmpeg-wrapper.c
		# ${SAMPLES_SOURCE_DIR}/common/md5.c
		# ${SAMPLES_SOURCE_DIR}/common/log.c
		# ${SAMPLES_SOURCE_DIR}/common/queue.c
		${ut_HEADERS}
		${ut_SOURCES}
		${CATCH2_INC_DIR}/device_memleak_detection.cpp
		
		${SAMPLES_SOURCE_DIR}/ut_origin/transcode.cpp
		${SAMPLES_SOURCE_DIR}/ut_origin/decode.cpp
		${SAMPLES_SOURCE_DIR}/ut_origin/encode.cpp		

		${DEC_SOURCE_DIR}/video_dec/ut_video_dec.cpp
		${DEC_SOURCE_DIR}/jpeg_dec/ut_jpeg_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_video_dec/ut_multi_thread_video_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_jpeg_dec/ut_multi_thread_jpeg_dec.cpp

		${ENC_SOURCE_DIR}/video_enc/ut_video_enc.cpp
		${ENC_SOURCE_DIR}/jpeg_enc/ut_jpeg_enc.cpp
		${ENC_SOURCE_DIR}/video_enc_multi_thread/ut_video_enc_multi_thread.cpp
		${ENC_SOURCE_DIR}/jpeg_enc_multi_thread/ut_jpeg_enc_multi_thread.cpp
		${CMAKE_SOURCE_DIR}/sample/enc/video_enc/option.c

		${TRANS_SOURCE_DIR}/ut_transcode.cpp
		${TRANS_SOURCE_DIR}/../transcode_mt/ut_transcode_mt.cpp
		
		${SAMPLES_SOURCE_DIR}/ut_origin/dev_memleak_detect.cpp
		${SAMPLES_SOURCE_DIR}/ut_origin/ut.cpp)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		-Wl,--whole-archive
		-Wl,--start-group
		-Wl,-Bstatic
		${FFMPEG_LIBS}
		# ${VMPP_LIB_DIR}/libvmpp-dec.so
		# ${VMPP_LIB_DIR}/libvmpp-enc.so
		-Wl,--no-whole-archive
		-Wl,--end-group
		-Wl,-Bdynamic
		${PTHREAD_LIB}
		stdc++
		dl
		m)
	target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
		vmpp-dec
		vmpp-enc
		)

elseif (BUILD_TARGET STREQUAL "1")
	message("-- Making dec sample UT")
	add_executable(${PROJECT_NAME}
		${VMPP_TOOL_DIR}/cJSON.c
		${VMPP_COMMON_DIR}/utils.c
		${VMPP_COMMON_DIR}/buf_queue.c
		# ${SAMPLES_SOURCE_DIR}/common/stream.c
		# ${SAMPLES_SOURCE_DIR}/common/ffmpeg-wrapper.c
		${VMPP_COMMON_DIR}/md5.c
		${VMPP_COMMON_DIR}/log.c
		${VMPP_COMMON_DIR}/queue.c

		${DEC_SOURCE_DIR}/video_dec/ut_video_dec.cpp
		${DEC_SOURCE_DIR}/jpeg_dec/ut_jpeg_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_video_dec/ut_multi_thread_video_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_jpeg_dec/ut_multi_thread_jpeg_dec.cpp

		${SAMPLES_SOURCE_DIR}/ut_origin/ut.cpp)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		-Wl,--whole-archive
		# -Wl,--start-group
		# -Wl,-Bstatic
		# ${FFMPEG_LIBS}
		-Wl,--no-whole-archive
		# -Wl,--end-group
		# -Wl,-Bdynamic
		${PTHREAD_LIB}
		stdc++
		dl
		m)
	target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
		vmpp-dec
		)
		
elseif (BUILD_TARGET STREQUAL "2")
	message("-- Making enc sample UT")
	add_executable(${PROJECT_NAME}
		${VMPP_TOOL_DIR}/cJSON.c
		${VMPP_COMMON_DIR}/utils.c
		# ${SAMPLES_SOURCE_DIR}/common/buf_queue.c
		${VMPP_COMMON_DIR}/stream.c
		# ${SAMPLES_SOURCE_DIR}/common/ffmpeg-wrapper.c
		${VMPP_COMMON_DIR}/md5.c
		${VMPP_COMMON_DIR}/log.c
		${VMPP_COMMON_DIR}/queue.c
		
		${ENC_SOURCE_DIR}/video_enc/ut_video_enc.cpp
		${ENC_SOURCE_DIR}/jpeg_enc/ut_jpeg_enc.cpp
		${ENC_SOURCE_DIR}/video_enc_multi_thread/ut_video_enc_multi_thread.cpp
		${ENC_SOURCE_DIR}/jpeg_enc_multi_thread/ut_jpeg_enc_multi_thread.cpp
		# ${ENC_SOURCE_DIR}/option.c

		${SAMPLES_SOURCE_DIR}/ut_origin/ut.cpp)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		-Wl,--whole-archive
		# -Wl,--start-group
		-Wl,-Bstatic
		${FFMPEG_LIBS}
		-Wl,--no-whole-archive
		# -Wl,--end-group
		-Wl,-Bdynamic
		${PTHREAD_LIB}
		dl
		m)
	target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
		vmpp-enc
		)
endif()

set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/ut/ctest")
include(Catch)
catch_discover_tests(${PROJECT_NAME})