add_library(os STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/os_console_linux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/os_fs_linux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/os_net_linux.c
)
