#include "platform_api.h"
#include "runtime.h"
#include "log.h"
#include <lauxlib.h>
#include <stdio.h>
#include <string.h>

static SCRIPT_NODE* s_current_app = NULL;

void ptx_app_set_current(SCRIPT_NODE* app)
{
	s_current_app = app;
}

SCRIPT_NODE* ptx_app_get_current(void)
{
	return s_current_app;
}

static int ptx_app_include(lua_State* L)
{
	const char* name = luaL_checkstring(L, 1);
	SCRIPT_NODE* app = ptx_app_get_current();
	if (!app)
		return luaL_error(L, "app.include called outside app context");

	lua_rawgeti(L, LUA_REGISTRYINDEX, app->env_ref);
	lua_getfield(L, -1, "APPROOT");
	const char* root = lua_tostring(L, -1);
	if (!root) {
		lua_pop(L, 2);
		return luaL_error(L, "APPROOT not set");
	}

	char path[256];
	snprintf(path, sizeof(path), "%s/%s.lua", root, name);
	lua_pop(L, 2);

	if (luaL_loadfile(L, path) != LUA_OK)
		return lua_error(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, app->env_ref);
	lua_setupvalue(L, -2, 1);

	if (lua_pcall(L, 0, 1, 0) != LUA_OK)
		return lua_error(L);
	return 1;
}

static int ptx_app_log(lua_State* L)
{
	const char* msg = luaL_checkstring(L, 1);
	SCRIPT_NODE* app = ptx_app_get_current();
	LOG_INFO("[app:%s] %s", app ? app->name : "?", msg);
	return 0;
}

static const luaL_Reg app_lib[] = {
	{"include",       ptx_app_include},
	{"log",           ptx_app_log},
	{NULL, NULL}
};

void ptx_app_register(lua_State* L)
{
	lua_newtable(L);
	luaL_setfuncs(L, app_lib, 0);
	lua_setglobal(L, "app");
}
