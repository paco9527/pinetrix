#ifndef _PTX_KEY_LUA_H
#define _PTX_KEY_LUA_H

#include "lua.h"

/* 在栈顶表(即 sys 表)上设置 "key" 子表 */
void ptx_key_lua_attach(lua_State* L);

#endif /* _PTX_KEY_LUA_H */
