target_sources(sys PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/script_proc.c ${CMAKE_CURRENT_SOURCE_DIR}/script_import.c ${CMAKE_CURRENT_SOURCE_DIR}/lua_import.c)
target_include_directories(sys PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
