target_sources(modules PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ptx_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/key_cfg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/key_router.c
    ${CMAKE_CURRENT_SOURCE_DIR}/key_lua.c
)
target_include_directories(modules PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
