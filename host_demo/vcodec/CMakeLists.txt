cmake_minimum_required(VERSION 3.15)


add_library(freertos_vcodec STATIC)


add_library(freertos_vcodec_include INTERFACE)

target_include_directories(freertos_vcodec_include  INTERFACE  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(freertos_vcodec_include
    INTERFACE
)

########################################################################
## add_subdirectory(include)
## add_subdirectory(portable)
#target_include_directories(freertos_vcodec 
#    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
#)

add_compile_options( -D__FREERTOS__ )

target_sources(freertos_vcodec PRIVATE
    vcx_cmdbuf_obj.c
)

target_link_libraries(freertos_vcodec
    PUBLIC
        mailbox_a78_include
        freertos_config
        freertos_kernel_include
        freertos_kernel_port_headers
        freertos_vcodec_include
        freertos_utils_include
    PRIVATE
        freertos_kernel
)

########################################################################
