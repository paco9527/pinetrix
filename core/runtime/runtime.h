#ifndef _PTX_RUNTIME_H
#define _PTX_RUNTIME_H

#include <lua.h>

typedef struct {
	lua_State* L;
} ptx_Runtime;

ptx_Runtime* ptx_runtime(void);

int  ptx_runtime_init(void);
void ptx_runtime_deinit(void);

#endif
