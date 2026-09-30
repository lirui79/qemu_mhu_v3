cmake_minimum_required(VERSION 3.10)
PROJECT(transcode_mt)

message("-- Making multi-thread transcode sample")

if(MSVC)
    set(OSAL_DIR ${CMAKE_SOURCE_DIR}/common/osal)
    # set(VMPP_LIB_DIR ${CMAKE_SOURCE_DIR}/../build/out/${CMAKE_BUILD_TYPE})
    set(VMPP_SAMPLE_DIR ${CMAKE_SOURCE_DIR})

	set(FFMPEG_LIBS ${VMPP_SAMPLE_DIR}/3rdparty/ffmpeg/win64/lib/avutil.lib
					${VMPP_SAMPLE_DIR}/3rdparty/ffmpeg/win64/lib/avcodec.lib
					${VMPP_SAMPLE_DIR}/3rdparty/ffmpeg/win64/lib/avformat.lib
					${VMPP_SAMPLE_DIR}/3rdparty/ffmpeg/win64/lib/swscale.lib)

	set(OSAL_HEADER  ${OSAL_DIR}/win32/include/sched.h
					${OSAL_DIR}/win32/include/pthread.h
					${OSAL_DIR}/win32/include/semaphore.h
					${OSAL_DIR}/base_type.h
					${OSAL_DIR}/os_define.h
					${OSAL_DIR}/os_platform.h
					${OSAL_DIR}/win32/os_define_win32.h
					${OSAL_DIR}/win32/os_platform_win32.h)
	set(OSAL_SRC ${OSAL_DIR}/win32/os_platform_win32.c)
    add_compile_options(
				/MP
				/W3
				/FC
				/D_CRT_SECURE_NO_WARNINGS
				/D_CRT_NONSTDC_NO_WARNINGS)
				
	set(VIDEO_DEC_INC_DIR 
    ${VMPP_INC_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/tool
    ${FFMPEG_INC}
	${OSAL_DIR}
	${OSAL_DIR}/win32
	${OSAL_DIR}/win32/include)

	set(VIDEO_DEC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)

	include_directories(${VIDEO_DEC_INC_DIR})

	set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)
	

	add_executable(${PROJECT_NAME}
		${CMAKE_SOURCE_DIR}/tool/cJSON_Utils.c
		${CMAKE_SOURCE_DIR}/tool/cJSON.c
		${CMAKE_SOURCE_DIR}/common/md5.c
		${CMAKE_SOURCE_DIR}/common/utils.c
		${CMAKE_SOURCE_DIR}/common/stream.c
		${FFMPEG_SRC}
		${CMAKE_SOURCE_DIR}/common/log.c
		${CMAKE_SOURCE_DIR}/common/queue.c
		${OSAL_SRC}
		transcode_mt.c
		${CMAKE_SOURCE_DIR}/common/option.c)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		${FFMPEG_LIBS}
		${VMPP_LIB_DIR}/vmpp-dec.lib
		${VMPP_LIB_DIR}/vmpp-enc.lib
		${VMPP_LIB_DIR}/pthreadVC2.lib
	)
	
	add_custom_command (TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${VMPP_LIB_DIR} ${VMPP_SAMPLE_DIR}/build/out/${CMAKE_BUILD_TYPE}
        COMMAND ${CMAKE_COMMAND} -E copy_directory  ${VMPP_SAMPLE_DIR}/3rdparty/deps/ ${VMPP_SAMPLE_DIR}/build/out/${CMAKE_BUILD_TYPE}
        COMMAND ${CMAKE_COMMAND} -E copy  ${VMPP_SAMPLE_DIR}/3rdparty/runtime/vaccrt64.dll ${VMPP_SAMPLE_DIR}/build/out/${CMAKE_BUILD_TYPE}
    )
    add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${VMPP_SAMPLE_DIR}/3rdparty/ffmpeg/win64/dll ${VMPP_SAMPLE_DIR}/build/out/${CMAKE_BUILD_TYPE}
    )
	
	target_compile_definitions(${PROJECT_NAME} PRIVATE OS_WINDOWS)
	target_compile_options(${PROJECT_NAME} PRIVATE 
        "/wd4101"   # disable "unused variables ..." warnings
        "/wd4996"   # disable "This function or variable may be unsafe ..." warnings
        "/wd4819"   # disable "The file contains a character that cannot be represented ..." warnings
        )
else()
    #set(OSAL_DIR ${CMAKE_SOURCE_DIR}/common/osal)
    #set(VMPP_LIB_DIR ${CMAKE_SOURCE_DIR}/../build/out/${CMAKE_BUILD_TYPE})
	if (USING_FFMPEG STREQUAL "1")
		set(FFMPEG_LIBS
			${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavutil.a
			${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavcodec.a
			${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavformat.a
			${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libswscale.a
			)
		set(FFMPEG_INC
				${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include)
		set(FFMPEG_SRC
				${CMAKE_SOURCE_DIR}/common/ffmpeg-wrapper.c)
	else()
		set(FFMPEG_LIBS "")
		set(FFMPEG_INC "")
		set(FFMPEG_SRC "")
	endif()

	set(VIDEO_DEC_INC_DIR 
		${VMPP_INC_DIR}
		${CMAKE_SOURCE_DIR}/common
		${CMAKE_SOURCE_DIR}/tool
		${FFMPEG_INC})

	set(VIDEO_DEC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)

	include_directories(${VIDEO_DEC_INC_DIR})

	set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

	add_executable(${PROJECT_NAME}
		${CMAKE_SOURCE_DIR}/tool/cJSON_Utils.c
		${CMAKE_SOURCE_DIR}/tool/cJSON.c
		${CMAKE_SOURCE_DIR}/common/md5.c
		${CMAKE_SOURCE_DIR}/common/utils.c
		${CMAKE_SOURCE_DIR}/common/stream.c
		${FFMPEG_SRC}
		${CMAKE_SOURCE_DIR}/common/log.c
		${CMAKE_SOURCE_DIR}/common/queue.c
		transcode_mt.c
		${CMAKE_SOURCE_DIR}/common/option.c)

	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC
		${VMPP_WHOLE_ARCHIVE}
		-Wl,--start-group
		-Wl,-Bstatic
		${FFMPEG_LIBS}
		${VMPP_BDYNAMIC}
		${VMPP_DEC_LIB}
		${VMPP_ENC_LIB}
		-Wl,--end-group
		${VMPP_NO_WHOLE_ARCHIVE}
		${PTHREAD_LIB}
        stdc++
		dl
		m
	)
endif(MSVC)
