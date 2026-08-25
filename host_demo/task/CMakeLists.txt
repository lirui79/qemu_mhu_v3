cmake_minimum_required(VERSION 3.15)


add_library(freertos_task STATIC)


add_library(freertos_task_include INTERFACE)

target_include_directories(freertos_task_include  INTERFACE  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(freertos_task_include
    INTERFACE
)

########################################################################
## add_subdirectory(include)
## add_subdirectory(portable)
#target_include_directories(freertos_vcodec 
#    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
#)

add_compile_options( -D__FREERTOS__ )

target_sources(freertos_task PRIVATE
    vcodec.c
)

target_link_libraries(freertos_task
    PUBLIC
        freertos_config
        freertos_kernel_include
        freertos_kernel_port_headers
        freertos_vcodec_include
        freertos_utils_include
        mailbox_a78_include
    PRIVATE
        freertos_kernel
        mailbox_a78
)

########################################################################
