# PROJECT(venc_sample)
# add_subdirectory(jpeg_enc)
# add_subdirectory(video_enc)
# add_subdirectory(video_enc_multi_thread)
# message("sample enc")
cmake_minimum_required(VERSION 3.10)
PROJECT(enc_ut)

message("-- Making enc Unit Test")

set(JPEG_ENC_INC_DIR
    ${VMPP_INC_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/tool
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include)

set(JPEG_ENC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)

include_directories(${CATCH2_INC_DIR} ${JPEG_ENC_INC_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

add_executable(${PROJECT_NAME}
    ut_enc.cpp
    video_enc/ut_video_enc.cpp
    jpeg_enc/ut_jpeg_enc.cpp
    video_enc_multi_thread/ut_video_enc_multi_thread.cpp
    jpeg_enc_multi_thread/ut_jpeg_enc_multi_thread.cpp
    option.c
    # ${CMAKE_SOURCE_DIR}/tool/cJSON_Utils.c
    ${CMAKE_SOURCE_DIR}/tool/cJSON.c
    ${CMAKE_SOURCE_DIR}/common/utils.c
    ${CMAKE_SOURCE_DIR}/common/md5.c
    ${CMAKE_SOURCE_DIR}/common/stream.c
    ${CMAKE_SOURCE_DIR}/common/queue.c
    ${CMAKE_SOURCE_DIR}/common/log.c)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    -Wl,--whole-archive
    ${VMPP_LIB_DIR}/libvmpp-enc.so
    -Wl,-Bdynamic
    -Wl,--no-whole-archive
    ${PTHREAD_LIB}
    dl
    m
)
