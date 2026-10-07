set(HAL_SOURCES)

# 屏幕后端
if(PTX_SCREEN STREQUAL "dummy")
    list(APPEND HAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/hal_screen_dummy.c)
else()
    list(APPEND HAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/hal_screen_ws2812.c)
endif()

# GPIO 后端 (按平台)
if(PTX_PLATFORM STREQUAL "dummy")
    list(APPEND HAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/hal_gpio_dummy.c)
else()
    list(APPEND HAL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/hal_gpio_linux.c)
endif()

add_library(hal STATIC ${HAL_SOURCES})
