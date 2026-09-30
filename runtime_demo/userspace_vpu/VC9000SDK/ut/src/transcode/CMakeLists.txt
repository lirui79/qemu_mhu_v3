cmake_minimum_required(VERSION 3.10)
PROJECT(transcode_ut)

message("-- Making transcode Unit Test")

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

include_directories(${CATCH2_INC_DIR} ${VIDEO_DEC_INC_DIR})

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
    ut_transcode.cpp
    ut_trans.cpp)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    -Wl,--whole-archive
    -Wl,--start-group
    -Wl,-Bstatic
    ${FFMPEG_LIBS}
    -Wl,-Bdynamic
    ${VMPP_LIB_DIR}/libvmpp-dec.so
    ${VMPP_LIB_DIR}/libvmpp-enc.so
    -Wl,--end-group
    -Wl,--no-whole-archive
    ${PTHREAD_LIB}
    dl
    m
)
