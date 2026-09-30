cmake_minimum_required(VERSION 3.10)
PROJECT(jpeg_dec_mt)

message("-- Making multi-thread jpeg_dec sample")

set(JPEG_DEC_INC_DIR 
    ${VMPP_INC_DIR}
    ${CMAKE_SOURCE_DIR}/common
)

include_directories(${JPEG_DEC_INC_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

add_executable(${PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/common/utils.c
    ${CMAKE_SOURCE_DIR}/common/log.c
    ${CMAKE_SOURCE_DIR}/common/queue.c
    multi_thread_jpeg_dec.c)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    ${VMPP_WHOLE_ARCHIVE}
    ${VMPP_DEC_LIB}
    ${VMPP_NO_WHOLE_ARCHIVE}
    ${PTHREAD_LIB}
    stdc++
    dl
    m
)