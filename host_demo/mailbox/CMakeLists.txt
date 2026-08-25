cmake_minimum_required(VERSION 3.15)

add_library(mailbox_a78_include INTERFACE)

target_include_directories(mailbox_a78_include  INTERFACE  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mailbox_a78_include
    INTERFACE
)

add_compile_options( -D__FREERTOS__ )


add_library(mailbox_a78 STATIC)

########################################################################
## add_subdirectory(include)
## add_subdirectory(portable)

target_sources(mailbox_a78 PRIVATE
    cmdnode.c
    cmda78_mgr.c
    cmda78_msg.c
    cmda78_proc.c
    mhu_v3_client.c
    cmda78_session.c
)

target_link_libraries(mailbox_a78
    PUBLIC
        freertos_config
        freertos_kernel_include
        freertos_kernel_port_headers
        freertos_utils_include
        freertos_vcodec_include
        mailbox_a78_include
    PRIVATE
        freertos_utils
        freertos_task
        freertos_vcodec
)

########################################################################
