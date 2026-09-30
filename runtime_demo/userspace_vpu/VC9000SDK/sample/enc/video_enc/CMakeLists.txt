cmake_minimum_required(VERSION 3.10)
PROJECT(video_enc)

message("-- Making video_enc sample")

set(JPEG_ENC_INC_DIR
    ${VMPP_INC_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include)

set(JPEG_ENC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)

include_directories(${JPEG_ENC_INC_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

add_executable(${PROJECT_NAME}
    video_enc.c
    option.c
    ${CMAKE_SOURCE_DIR}/common/utils.c
    ${CMAKE_SOURCE_DIR}/common/md5.c
    ${CMAKE_SOURCE_DIR}/common/stream.c
    ${CMAKE_SOURCE_DIR}/common/queue.c
    ${CMAKE_SOURCE_DIR}/common/log.c)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    ${VMPP_WHOLE_ARCHIVE}
    ${VMPP_ENC_LIB}
    ${VMPP_BDYNAMIC}
    ${VMPP_NO_WHOLE_ARCHIVE}
    ${PTHREAD_LIB}
    stdc++
    dl
    m
)