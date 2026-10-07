target_sources(modules PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/urlparser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/net_lua.c)
target_include_directories(modules PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
