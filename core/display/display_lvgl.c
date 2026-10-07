#include "display.h"
#include "lvgl.h"
#include "luavgl.h"
#include "lv_adapter.h"

static void lvgl_init(unsigned int w, unsigned int h)
{
	lvgl_core_init(w, h);
}

static void lvgl_tick(void)
{
	lv_timer_handler();
}

static void* lvgl_create_app_root(void)
{
	return lv_obj_create(NULL);   /* 每个 app 一个独立 screen */
}

static void lvgl_destroy_app_root(void* root)
{
	lv_obj_del((lv_obj_t*)root);
}

static int lvgl_register_lua(lua_State* L)
{
	luaL_requiref(L, "lvgl", luaopen_lvgl, 1);
	lua_pop(L, 1);
	return 0;
}

static const ptx_DisplayBackend s_lvgl_backend = {
	.init             = lvgl_init,
	.deinit           = NULL,
	.tick             = lvgl_tick,
	.create_app_root  = lvgl_create_app_root,
	.destroy_app_root = lvgl_destroy_app_root,
	.register_lua     = lvgl_register_lua,
};

const ptx_DisplayBackend* ptx_display(void)
{
	return &s_lvgl_backend;
}
