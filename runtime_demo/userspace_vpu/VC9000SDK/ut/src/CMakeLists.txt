cmake_minimum_required(VERSION 3.12)

project(sample_unitest)

message("-- vastai_video_sdk sample unitest")

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

    if (MEM_ONLY_DEV_CHECK STREQUAL "1")
        add_definitions(-DMEM_ONLY_DEV_CHECK)
    endif()
endif()

set(VMPP_SDK_ROOT ${CMAKE_SOURCE_DIR}/../..)
set(VMPP_LIB_DIR ${VMPP_SDK_ROOT}/lib)
set(VMPP_INC_DIR ${VMPP_SDK_ROOT}/inc)
set(CATCH2_INC_DIR ${CMAKE_SOURCE_DIR}/../include)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)

set(DEC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/dec)
set(ENC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/enc)
set(TRANS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/transcode)
set(SAMPLES_INC_DIR 
	${VMPP_INC_DIR}
	${CMAKE_SOURCE_DIR}/common
	${CMAKE_SOURCE_DIR}/tool
	${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include)
include_directories(${CATCH2_INC_DIR} ${SAMPLES_INC_DIR})

if (USING_FFMPEG STREQUAL "1")
set(FFMPEG_LIBS
	${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavutil.a
	${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavcodec.a
	${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavformat.a
	${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libswscale.a)
else()
    set(FFMPEG_LIBS "")
endif()


if (BUILD_TARGET STREQUAL "0")
	add_executable(${PROJECT_NAME}
		${CMAKE_SOURCE_DIR}/tool/cJSON.c
		${CMAKE_SOURCE_DIR}/common/utils.c
		${CMAKE_SOURCE_DIR}/common/buf_queue.c
		${CMAKE_SOURCE_DIR}/common/stream.c
		${CMAKE_SOURCE_DIR}/common/ffmpeg-wrapper.c
		${CMAKE_SOURCE_DIR}/common/md5.c
		${CMAKE_SOURCE_DIR}/common/log.c
		${CMAKE_SOURCE_DIR}/common/queue.c
		${DEC_SOURCE_DIR}/video_dec/ut_video_dec.cpp # decode
		${DEC_SOURCE_DIR}/jpeg_dec/ut_jpeg_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_video_dec/ut_multi_thread_video_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_jpeg_dec/ut_multi_thread_jpeg_dec.cpp

		${ENC_SOURCE_DIR}/video_enc/ut_video_enc.cpp # encode
		${ENC_SOURCE_DIR}/jpeg_enc/ut_jpeg_enc.cpp
		${ENC_SOURCE_DIR}/jpeg_enc_multi_thread/ut_jpeg_enc_multi_thread.cpp
		${ENC_SOURCE_DIR}/video_enc_multi_thread/ut_video_enc_multi_thread.cpp
		${ENC_SOURCE_DIR}/option.c

		${TRANS_SOURCE_DIR}/ut_transcode.cpp # transcode
		ut_samples.cpp)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		-Wl,--whole-archive
		-Wl,--start-group
		-Wl,-Bstatic
		${FFMPEG_LIBS}
		${VMPP_LIB_DIR}/libvmpp-dec.so
		${VMPP_LIB_DIR}/libvmpp-enc.so
		-Wl,--no-whole-archive
		-Wl,--end-group
		-Wl,-Bdynamic
		${PTHREAD_LIB}
		dl
		m)

elseif (BUILD_TARGET STREQUAL "1")
	message("-- Making dec sample UT")
	
	add_executable(${PROJECT_NAME}
		${CMAKE_SOURCE_DIR}/tool/cJSON.c
		${CMAKE_SOURCE_DIR}/common/utils.c
		${CMAKE_SOURCE_DIR}/common/buf_queue.c
		${CMAKE_SOURCE_DIR}/common/md5.c
		${CMAKE_SOURCE_DIR}/common/log.c
		${CMAKE_SOURCE_DIR}/common/queue.c
		${CMAKE_SOURCE_DIR}/common/stream.c
		${DEC_SOURCE_DIR}/video_dec/ut_video_dec.cpp
		${DEC_SOURCE_DIR}/jpeg_dec/ut_jpeg_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_video_dec/ut_multi_thread_video_dec.cpp
		${DEC_SOURCE_DIR}/multi_thread_jpeg_dec/ut_multi_thread_jpeg_dec.cpp
		${DEC_SOURCE_DIR}/ut_dec.cpp)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		-Wl,--whole-archive
		${VMPP_LIB_DIR}/libvmpp-dec.so
		-Wl,--no-whole-archive
		${PTHREAD_LIB}
		dl
		m
	)

elseif (BUILD_TARGET STREQUAL "2")
	message("-- Making enc sample UT")

	add_executable(${PROJECT_NAME}
		${ENC_SOURCE_DIR}/ut_enc.cpp
		${ENC_SOURCE_DIR}/video_enc/ut_video_enc.cpp
		${ENC_SOURCE_DIR}/jpeg_enc/ut_jpeg_enc.cpp
		${ENC_SOURCE_DIR}/video_enc_multi_thread/ut_video_enc_multi_thread.cpp
		${ENC_SOURCE_DIR}/jpeg_enc_multi_thread/ut_jpeg_enc_multi_thread.cpp
		${ENC_SOURCE_DIR}/option.c
		# ${CMAKE_SOURCE_DIR}/tool/cJSON_Utils.c
		${CMAKE_SOURCE_DIR}/tool/cJSON.c
		${CMAKE_SOURCE_DIR}/common/utils.c
		${CMAKE_SOURCE_DIR}/common/md5.c
		${CMAKE_SOURCE_DIR}/common/stream.c
		${CMAKE_SOURCE_DIR}/common/ffmpeg-wrapper.c
		${CMAKE_SOURCE_DIR}/common/queue.c
		${CMAKE_SOURCE_DIR}/common/log.c)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		-Wl,--whole-archive
		${FFMPEG_LIBS}
		${VMPP_LIB_DIR}/libvmpp-enc.so
		-Wl,-Bdynamic
		-Wl,--no-whole-archive
		${PTHREAD_LIB}
		dl
		m
	)
elseif (BUILD_TARGET STREQUAL "3")
	message("-- Making transcode sample UT")
	
	add_executable(${PROJECT_NAME}
		${CMAKE_SOURCE_DIR}/tool/cJSON_Utils.c
		${CMAKE_SOURCE_DIR}/tool/cJSON.c
		${CMAKE_SOURCE_DIR}/common/md5.c
		${CMAKE_SOURCE_DIR}/common/utils.c
		${CMAKE_SOURCE_DIR}/common/stream.c
		${CMAKE_SOURCE_DIR}/common/ffmpeg-wrapper.c
		${CMAKE_SOURCE_DIR}/common/log.c
		${CMAKE_SOURCE_DIR}/common/queue.c
		${TRANS_SOURCE_DIR}/ut_transcode.cpp
		${TRANS_SOURCE_DIR}/ut_trans.cpp)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		-Wl,--whole-archive
		-Wl,--start-group
		-Wl,-Bstatic
		${FFMPEG_LIBS}
		-Wl,-Bdynamic
		-Wl,--end-group
		-Wl,--no-whole-archive
		${PTHREAD_LIB}
		dl
		m
	)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		${VMPP_LIB_DIR}/libvmpp-dec.so
		${VMPP_LIB_DIR}/libvmpp-enc.so
	)
endif()
