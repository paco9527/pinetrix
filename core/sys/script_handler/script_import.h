#ifndef _SCRIPT_IMPORT
#define _SCRIPT_IMPORT
#include "lua.h"
#include "lauxlib.h"
#include "lvgl.h"
#include "render.h"

typedef struct _SCRIPT_LIST
{
    int node_id;
    char name[16];
    int env_ref;            /* entry.lua 的 _ENV */
    int loop_interval;      /* setup().loop, 缺省 PTX_LOOP_INTERVAL */
    int bg_interval;        /* setup().background, 缺省 PTX_BG_INTERVAL */
    int loop_timer;         /* ptx 计时器 id, 无则 -1 */
    int bg_timer;           /* ptx 计时器 id, 无则 -1 */
    int on_show_ref;        /* luaL_ref, 无则 LUA_NOREF */
    int on_hide_ref;        /* luaL_ref, 无则 LUA_NOREF */
    struct _SCRIPT_LIST* next;

    lv_obj_t* screen;       /* 每个 app 一个独立 lvgl screen */
    lv_style_t screen_style;
}SCRIPT_NODE;

int sys_getapi(lua_State* state);

int script_env_deinit(lua_State* state);

/* 应用管理 */
void app_manager_init(void);

SCRIPT_NODE* app_load_from_file(char* dir, char* app_name);
void app_del(SCRIPT_NODE** head, int node_id);
int app_clear(SCRIPT_NODE* head);

SCRIPT_NODE* get_script_list_head(void);
SCRIPT_NODE* get_app_by_id(int node_id);
SCRIPT_NODE* get_app_by_name(const char* name);
SCRIPT_NODE* app_get_foreground(void);

void get_app_list(void);
void release_app_list(void);

/* 把 luavgl root / 当前 app 绑定到指定 app; NULL 表示绑定到空屏 */
void app_bind(SCRIPT_NODE* app);
void app_restore_foreground(void);

#endif
