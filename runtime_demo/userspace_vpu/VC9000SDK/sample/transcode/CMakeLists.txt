cmake_minimum_required(VERSION 3.10)
PROJECT(transcode)

message("-- Making transcode sample")

set(VIDEO_DEC_INC_DIR 
    ${VMPP_INC_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/tool
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include)

set(VIDEO_DEC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)

if (USING_FFMPEG STREQUAL "1")
    set(FFMPEG_LIBS
        ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavutil.a
        ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavcodec.a
        ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavformat.a
        ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libswscale.a
        )
else()
    set(FFMPEG_LIBS "")
endif()

include_directories(${VIDEO_DEC_INC_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

add_executable(${PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/tool/cJSON_Utils.c
    ${CMAKE_SOURCE_DIR}/tool/cJSON.c
    ${CMAKE_SOURCE_DIR}/common/md5.c
    ${CMAKE_SOURCE_DIR}/common/utils.c
    ${CMAKE_SOURCE_DIR}/common/stream.c
    ${CMAKE_SOURCE_DIR}/common/ffmpeg-wrapper.c
    ${CMAKE_SOURCE_DIR}/common/log.c
    ${CMAKE_SOURCE_DIR}/common/queue.c
    transcode.c
    option.c)

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
