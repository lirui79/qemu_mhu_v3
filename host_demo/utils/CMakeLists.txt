cmake_minimum_required(VERSION 3.15)


add_library(freertos_utils STATIC)

########################################################################
## add_subdirectory(include)
## add_subdirectory(portable)
add_compile_options( -D__FREERTOS__ )

target_sources(freertos_utils PRIVATE
    libc.c
    kref.c
    crc32.c
    rbtree.c
#    printf.c
    bqueue.c
    cqueue.c
    wait_queue.c
)

target_link_libraries(freertos_utils
    PUBLIC
        freertos_config
        freertos_kernel_include
        freertos_kernel_port_headers
        freertos_utils_include
    PRIVATE
        freertos_kernel
)

########################################################################
