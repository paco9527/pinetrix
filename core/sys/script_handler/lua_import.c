#include "lua_import.h"
#include "lualib.h"

extern int luaopen_cjson(lua_State *l);

int lua_import_lib(lua_State* L)
{
    luaL_openlibs(L);
    luaopen_cjson(L);
    lua_setglobal(L, "cjson");   /* luaopen_cjson 返回的表注册为全局 cjson */
    return 1;
}

