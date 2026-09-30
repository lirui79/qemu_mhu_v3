# add_subdirectory(jpeg_dec)
# add_subdirectory(video_dec)
# add_subdirectory(multi_thread_jpeg_dec)
# add_subdirectory(multi_thread_video_dec)

cmake_minimum_required(VERSION 3.10)
PROJECT(dec_ut)

message("-- Making dec sample and Unit Test")

set(DEC_INC_DIR
    ${VMPP_INC_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/tool)

include_directories(${CATCH2_INC_DIR} ${DEC_INC_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

#add_compile_options(-O2)

add_executable(${PROJECT_NAME}
    # ${CMAKE_SOURCE_DIR}/tool/cJSON_Utils.c
    ${CMAKE_SOURCE_DIR}/tool/cJSON.c
    ${CMAKE_SOURCE_DIR}/common/utils.c
    ${CMAKE_SOURCE_DIR}/common/buf_queue.c
    ${CMAKE_SOURCE_DIR}/common/md5.c
    ${CMAKE_SOURCE_DIR}/common/log.c
    ${CMAKE_SOURCE_DIR}/common/queue.c
    video_dec/ut_video_dec.cpp
    jpeg_dec/ut_jpeg_dec.cpp
    multi_thread_video_dec/ut_multi_thread_video_dec.cpp
    multi_thread_jpeg_dec/ut_multi_thread_jpeg_dec.cpp
    ut_dec.cpp)

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
