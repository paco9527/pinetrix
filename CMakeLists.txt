cmake_minimum_required(VERSION 3.22)
if(DEFINED COMPILER_PREFIX)
    set(CMAKE_SYSTEM_NAME Linux)
    string(REGEX REPLACE "-.*" "" CMAKE_SYSTEM_PROCESSOR "${COMPILER_PREFIX}")
    set(CMAKE_C_COMPILER ${COMPILER_PREFIX}gcc)
    set(CMAKE_CXX_COMPILER ${COMPILER_PREFIX}g++)
endif()
project(pitrix)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ============================================================
# 配置项
# ============================================================
set(PTX_SCREEN "ws2812" CACHE STRING "Screen backend: ws2812 or dummy")
set_property(CACHE PTX_SCREEN PROPERTY STRINGS ws2812 dummy)

set(PTX_PLATFORM "linux" CACHE STRING "Platform backend: linux or dummy")
set_property(CACHE PTX_PLATFORM PROPERTY STRINGS linux dummy)

set(MATRIX_WIDTH  32   CACHE STRING "Screen width")
set(MATRIX_HEIGHT 8    CACHE STRING "Screen height")
set(GPIO_PIN      18   CACHE STRING "WS2812 GPIO pin")
set(WS2812_DMA    10   CACHE STRING "WS2812 DMA channel")
set(PTX_LOOP_INTERVAL      100  CACHE STRING "Foreground loop interval ms")
set(PTX_BG_INTERVAL        1000 CACHE STRING "Background loop interval ms")
set(SETUP_SCRIPT_BUF_LEN   4096 CACHE STRING "Setup script buffer size")
set(MAX_EXPORT_CONTENT_LEN 1024 CACHE STRING "Max log content length")
set(PTX_TICK_MS 10 CACHE STRING "Main loop tick interval ms")

if(PTX_SCREEN STREQUAL "dummy")
    set(PTX_SCREEN_DUMMY 1)
    set(PTX_SCREEN_WS2812 0)
else()
    set(PTX_SCREEN_WS2812 1)
    set(PTX_SCREEN_DUMMY 0)
endif()

# 生成配置头文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/ptx_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/ptx_config.h
    @ONLY
)

# ============================================================
# 全局 include 路径
# ============================================================
set(EXTLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core/extlib/output")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/hal")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/os")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/display")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/runtime")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/modules/log")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/modules/key_handle")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/modules/render")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/modules/network")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_SOURCE_DIR}/core/sys/script_handler")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${EXTLIB_DIR}/include")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${EXTLIB_DIR}/include/lvgl")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${EXTLIB_DIR}/include/ws2811")
set(INCLUDE_DIR "${INCLUDE_DIR}\;${CMAKE_CURRENT_BINARY_DIR}/generated")

include_directories(${INCLUDE_DIR})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/extlib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/hal)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/os)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/display)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/modules)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/runtime)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core/sys)

add_executable(pitrix
    ${CMAKE_CURRENT_SOURCE_DIR}/core/sys/main/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/core/sys/main/glibc_compat.c
)

target_link_directories(pitrix PUBLIC ${EXTLIB_DIR}/lib)
target_link_libraries(pitrix -Wl,--start-group)
target_link_libraries(pitrix hal display runtime sys modules os luavgl lvgl)
target_link_libraries(pitrix -Wl,--end-group)
target_link_libraries(pitrix lua-cjson)
target_link_libraries(pitrix lua)
target_link_libraries(pitrix m)
if(PTX_SCREEN STREQUAL "ws2812")
    target_link_libraries(pitrix ws2811)
endif()

add_dependencies(sys extlib)
add_dependencies(modules extlib)
add_dependencies(hal extlib)
add_dependencies(display extlib)
add_dependencies(runtime extlib)
