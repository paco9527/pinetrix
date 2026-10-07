#include "key_lua.h"

#include <string.h>

#include "key_router.h"
#include "ptx_key.h"
#include "platform_api.h"

static int do_key_on(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int always = 0;
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "always");
        always = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    lua_pushvalue(L, 1);                       /* 复制回调到栈顶供 luaL_ref */
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    SCRIPT_NODE* app = ptx_app_get_current();
    int handle = ptx_key_router_add(app ? app->node_id : -1, ref, always);
    if (handle < 0) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        lua_pushnil(L);
        lua_pushstring(L, "too many key handlers");
        return 2;
    }
    lua_pushinteger(L, handle);
    return 1;
}

static int do_key_off(lua_State* L)
{
    int handle = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, ptx_key_router_remove(handle));
    return 1;
}

static int do_key_pressed(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    int p = ptx_key_pressed(id);
    lua_pushboolean(L, p == 1);
    return 1;
}

/* 测试用: 直接注入一个按键事件, 走与真实按键相同的分发路径 */
static int do_key_send(lua_State* L)
{
    const char* id  = luaL_checkstring(L, 1);
    const char* evs = luaL_checkstring(L, 2);
    int ev;
    if      (!strcmp(evs, "down"))   ev = PTX_KEY_DOWN;
    else if (!strcmp(evs, "short"))  ev = PTX_KEY_SHORT;
    else if (!strcmp(evs, "long"))   ev = PTX_KEY_LONG;
    else if (!strcmp(evs, "repeat")) ev = PTX_KEY_REPEAT;
    else if (!strcmp(evs, "up"))     ev = PTX_KEY_UP;
    else return luaL_error(L, "bad key event: %s", evs);

    ptx_key_router_dispatch(id, ev);
    lua_pushboolean(L, 1);
    return 1;
}

static int do_key_list(lua_State* L)
{
    int n = 0;
    const ptx_key_cfg_t* cfg = ptx_key_cfg(&n);
    lua_newtable(L);
    for (int i = 0; i < n; i++) {
        lua_newtable(L);
        lua_pushstring(L, cfg[i].id);        lua_setfield(L, -2, "id");
        lua_pushstring(L, cfg[i].spec);      lua_setfield(L, -2, "spec");
        lua_pushboolean(L, cfg[i].reserved); lua_setfield(L, -2, "reserved");
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static const struct luaL_Reg key_lib[] =
{
    {"on",      do_key_on},
    {"off",     do_key_off},
    {"pressed", do_key_pressed},
    {"send",    do_key_send},
    {"list",    do_key_list},
    {NULL, NULL}
};

void ptx_key_lua_attach(lua_State* L)
{
    luaL_newlib(L, key_lib);
    lua_setfield(L, -2, "key");
}
