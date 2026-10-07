#ifndef _PTX_APP_API_H
#define _PTX_APP_API_H

#include <lua.h>
#include "script_import.h"

void ptx_app_register(lua_State* L);
void ptx_app_set_current(SCRIPT_NODE* app);
SCRIPT_NODE* ptx_app_get_current(void);

#endif
