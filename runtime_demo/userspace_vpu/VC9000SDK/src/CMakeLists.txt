cmake_minimum_required(VERSION 3.10)

if(ENABLE_DYNAMIC_RES)
    add_definitions(-DENABLE_DYNAMIC_RES)
endif()

if (BUILD_TARGET STREQUAL "0")
    add_subdirectory(dec)
    add_subdirectory(enc)
elseif (BUILD_TARGET STREQUAL "1")
    add_subdirectory(dec)
elseif (BUILD_TARGET STREQUAL "2")
    add_subdirectory(enc)
endif()