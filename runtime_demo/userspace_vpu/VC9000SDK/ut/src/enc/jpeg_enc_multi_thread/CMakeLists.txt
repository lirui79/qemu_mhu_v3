cmake_minimum_required(VERSION 3.10)
PROJECT(jpeg_enc_mt)

message("-- Making jpeg_enc multi-thread sample")

set(JPEG_ENC_INC_DIR
    ${VMPP_INC_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include)

set(FFMPEG_LIBS
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavutil.a
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavcodec.a
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libavformat.a
    ${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/lib/libswscale.a
)

set(JPEG_ENC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)

include_directories(${JPEG_ENC_INC_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

add_executable(${PROJECT_NAME}
    jpeg_enc_multi_thread.c
    ${CMAKE_SOURCE_DIR}/common/utils.c
    ${CMAKE_SOURCE_DIR}/common/ffmpeg-wrapper.c
    ${CMAKE_SOURCE_DIR}/common/log.c
    ${CMAKE_SOURCE_DIR}/common/queue.c
    ${CMAKE_SOURCE_DIR}/common/md5.c)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    -Wl,--whole-archive
    ${FFMPEG_LIBS}
    ${VMPP_LIB_DIR}/libvmpp-enc.so
    -Wl,-Bdynamic
    -Wl,--no-whole-archive
    pthread
    dl
    m
)