#ifndef _PTX_DISPLAY_H
#define _PTX_DISPLAY_H

#include <lua.h>

typedef struct {
	void (*init)(unsigned int w, unsigned int h);
	void (*deinit)(void);
	void (*tick)(void);
	void* (*create_app_root)(void);
	void  (*destroy_app_root)(void* root);
	int  (*register_lua)(lua_State* L);
} ptx_DisplayBackend;

const ptx_DisplayBackend* ptx_display(void);

#endif
