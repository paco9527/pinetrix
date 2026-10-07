#include "runtime.h"
#include <lauxlib.h>
#include <lualib.h>
#include "display.h"
#include "luavgl.h"
#include "lua_import.h"
#include "net_lua.h"
#include "script_import.h"
#include "timer_mgr.h"
#include "platform_api.h"

ptx_Runtime* ptx_runtime(void)
{
	static ptx_Runtime instance = {0};
	return &instance;
}

int ptx_runtime_init(void)
{
	ptx_Runtime* rt = ptx_runtime();

	rt->L = luaL_newstate();
	luaL_openlibs(rt->L);

	ptx_app_register(rt->L);
	ptx_timer_init();

	ptx_display()->register_lua(rt->L);

	luaL_requiref(rt->L, "net", ptx_net_lua_open, 1);
	lua_pop(rt->L, 1);

	lua_import_lib(rt->L);

	luaL_requiref(rt->L, "sys", sys_getapi, 1);
	lua_pop(rt->L, 1);

	return 0;
}

void ptx_runtime_deinit(void)
{
	ptx_Runtime* rt = ptx_runtime();
	if (rt->L) {
		lua_close(rt->L);
		rt->L = NULL;
	}
}
