cmake_minimum_required(VERSION 3.10)
PROJECT(cjson)

message("-- Making cjson")

# set(CJSON_DEC_INC_DIR ${CMAKE_SOURCE_DIR}/inc)
# set(CJSON_DEC_LIB_DIR ${CMAKE_SOURCE_DIR}/build/out)

# include_directories(${H264_DEC_INC_DIR})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build/out)

add_executable(${PROJECT_NAME}
    cJSON_Utils.c
    cJSON.c)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    -Wl,--whole-archive
    -Wl,-Bstatic
    ${VMPP_BDYNAMIC}
    -Wl,--no-whole-archive
    dl
    m
)