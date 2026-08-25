cmake_minimum_required(VERSION 3.15)

add_library(mailbox_r52_include INTERFACE)

target_include_directories(mailbox_r52_include  INTERFACE  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mailbox_r52_include
    INTERFACE
)

add_library(mailbox_r52 STATIC)

########################################################################
## add_subdirectory(include)
## add_subdirectory(portable)

target_sources(mailbox_r52 PRIVATE
    cmdr52_proc.c
    cmdr52_mgr.c
    mhu_v3_r52.c
    cmdr52_session.c
)

target_link_libraries(mailbox_r52
    PUBLIC
        freertos_config
        freertos_kernel_include
        freertos_kernel_port_headers
        freertos_vcodec_include
        freertos_utils_include
        mailbox_r52_include
    PRIVATE
        freertos_utils
        freertos_task
        freertos_vcodec
)

########################################################################
