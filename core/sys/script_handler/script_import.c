#include "script_import.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "luavgl.h"
#include "runtime.h"
#include "platform_api.h"
#include "timer_mgr.h"
#include "display.h"
#include "os_thread.h"
#include "os_fs.h"
#include "lua_import.h"
#include "net_lua.h"
#include "key_router.h"
#include "key_lua.h"
#include "ptx_config.h"

#ifndef PTX_BG_INTERVAL
#define PTX_BG_INTERVAL 1000
#endif

#define WORKING_DIR_VARIABLE_NAME       "APPROOT"
#define SETUP_SCRIPT "/entry.lua"
#define MIN_INTERVAL_MS 10

static SCRIPT_NODE* g_app_list_head = NULL;
static ptx_os_mutex_t* list_mutex = NULL;
static ptx_os_cond_t*  list_cond = NULL;
static int list_modifying = 0;
static int script_cnt = 0;

/* 前台状态 */
static SCRIPT_NODE* g_fg = NULL;
static int          g_last_fg_id = -1;
static lv_obj_t*    g_blank = NULL;   /* hide 用宿主空屏 */

/* 前置声明 */
static int  app_show(SCRIPT_NODE* target);
static void app_hide(void);
static SCRIPT_NODE* app_prev_node(SCRIPT_NODE* cur);

/* ------------------ 全局态: 绑定/收口 ------------------ */

void app_bind(SCRIPT_NODE* app)
{
    lua_State* L = ptx_runtime()->L;
    if (app) {
        luavgl_set_root(L, app->screen);
        ptx_app_set_current(app);
    } else {
        luavgl_set_root(L, g_blank);
        ptx_app_set_current(NULL);
    }
}

void app_restore_foreground(void)
{
    app_bind(g_fg);
}

SCRIPT_NODE* app_get_foreground(void)
{
    return g_fg;
}

static void app_call_hook(SCRIPT_NODE* app, int ref)
{
    if (app == NULL || ref == LUA_NOREF)
        return;

    lua_State* L = ptx_runtime()->L;
    app_bind(app);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        LOG_ERROR("hook: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    app_restore_foreground();
}

/* ------------------ home(空屏) --------------------- */

void app_manager_init(void)
{
    static lv_style_t blank_style = {0};

    if (!list_mutex) list_mutex = ptx_os_mutex_create();
    if (!list_cond)  list_cond = ptx_os_cond_create();

    lv_style_init(&blank_style);
    lv_style_set_bg_opa(&blank_style, LV_OPA_COVER);
    lv_style_set_radius(&blank_style, 0);
    lv_style_set_pad_all(&blank_style, 0);
    lv_style_set_border_width(&blank_style, 0);
    lv_style_set_bg_color(&blank_style, lv_color_hex(0));

    g_blank = lv_obj_create(NULL);
    if (g_blank)
    {
        lv_obj_add_style(g_blank, &blank_style, LV_PART_MAIN);
        lv_scr_load(g_blank);   /* 启动即黑屏, 避免默认白屏整屏亮起 */
    }

    /* 按键: 初始化路由(设备回调 + 扫描) */
    ptx_key_router_init();
}

/* ------------------ linked list ---------------------- */

SCRIPT_NODE* get_script_list_head()
{
    return g_app_list_head;
}

SCRIPT_NODE* get_app_by_id(int node_id)
{
    SCRIPT_NODE* head = g_app_list_head;
    if (!head) return NULL;
    SCRIPT_NODE* node = head;
    do {
        if (node->node_id == node_id)
            return node;
        node = node->next;
    } while (node != head);
    return NULL;
}

SCRIPT_NODE* get_app_by_name(const char* name)
{
    SCRIPT_NODE* head = g_app_list_head;
    if (!head || !name) return NULL;
    SCRIPT_NODE* node = head;
    do {
        if (strcmp(node->name, name) == 0)
            return node;
        node = node->next;
    } while (node != head);
    return NULL;
}

void get_app_list(void)
{
    if (!list_mutex) { list_modifying = 1; return; }
    ptx_os_mutex_lock(list_mutex);
    while(list_modifying)
    {
        ptx_os_cond_wait(list_cond, list_mutex);
    }
    list_modifying = 1;
    ptx_os_mutex_unlock(list_mutex);
}

void release_app_list(void)
{
    if (!list_mutex) { list_modifying = 0; return; }
    ptx_os_mutex_lock(list_mutex);
    list_modifying = 0;
    ptx_os_cond_signal(list_cond);
    ptx_os_mutex_unlock(list_mutex);
}

void script_node_ls(SCRIPT_NODE* head)
{
    if(NULL == head)
        return;

    SCRIPT_NODE* node = head;
    LOG_DEBUG("current loaded apps:");
    do
    {
        LOG_DEBUG("id: %d\tname: %s%s, node: %p",
                  node->node_id, node->name,
                  (node == g_fg) ? " (fg)" : "", node);
        node = node->next;
    }while(node != head);
}

SCRIPT_NODE* script_node_add(SCRIPT_NODE** head)
{
    static int node_id = 0;
    SCRIPT_NODE* pnode = *head;
    SCRIPT_NODE* tail_node = *head;
    pnode = (SCRIPT_NODE*)malloc(sizeof(SCRIPT_NODE));
    if (pnode == NULL)
        return NULL;
    memset(pnode, 0, sizeof(SCRIPT_NODE));
    pnode->env_ref = LUA_NOREF;
    pnode->on_show_ref = LUA_NOREF;
    pnode->on_hide_ref = LUA_NOREF;
    pnode->loop_interval = PTX_LOOP_INTERVAL;
    pnode->bg_interval = PTX_BG_INTERVAL;
    pnode->loop_timer = -1;
    pnode->bg_timer = -1;
    if(*head)
    {
        while(tail_node->next != *head)
        {
            tail_node = tail_node->next;
        }
        tail_node->next = pnode;
    }
    else
    {
        *head = pnode;
    }
    pnode->next = *head;
    pnode->node_id = node_id;
    LOG_DEBUG("add node id: %d\n", node_id);
    node_id++;
    script_cnt++;
    return pnode;
}

int app_clear(SCRIPT_NODE* head)
{
    if(NULL == head)
        return -1;
    lua_State* L = ptx_runtime()->L;
    SCRIPT_NODE* p = head;

    do
    {
        SCRIPT_NODE* n = p->next;
        ptx_timer_remove_app(p->node_id);
        ptx_key_router_unregister_app(p->node_id);
        if(p->env_ref != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, p->env_ref);
        if(p->on_show_ref != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, p->on_show_ref);
        if(p->on_hide_ref != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, p->on_hide_ref);
        if(p->screen)
            ptx_display()->destroy_app_root(p->screen);
        free(p);
        p = n;
    }while(p != head);

    g_app_list_head = NULL;
    g_fg = NULL;
    script_cnt = 0;
    return 0;
}

void app_del(SCRIPT_NODE** head, int node_id)
{
    if(NULL == *head)
        return;

    lua_State* L = ptx_runtime()->L;

    /* 定位目标及其前驱 */
    SCRIPT_NODE* cur = *head;
    SCRIPT_NODE* prev = NULL;
    do
    {
        if(cur->node_id == node_id)
            break;
        prev = cur;
        cur = cur->next;
    }while(cur != *head);

    if(cur->node_id != node_id)
        return;   /* 未找到 */

    SCRIPT_NODE* succ = (cur->next == cur) ? NULL : cur->next;
    int was_fg = (cur == g_fg);
    if(was_fg)
        g_fg = NULL;

    /* 释放资源 */
    ptx_timer_remove_app(cur->node_id);
    ptx_key_router_unregister_app(cur->node_id);
    if(cur->env_ref != LUA_NOREF)
        luaL_unref(L, LUA_REGISTRYINDEX, cur->env_ref);
    if(cur->on_show_ref != LUA_NOREF)
        luaL_unref(L, LUA_REGISTRYINDEX, cur->on_show_ref);
    if(cur->on_hide_ref != LUA_NOREF)
        luaL_unref(L, LUA_REGISTRYINDEX, cur->on_hide_ref);
    if(cur->screen)
        ptx_display()->destroy_app_root(cur->screen);

    /* 摘链 */
    if(succ == NULL)
    {
        *head = NULL;
    }
    else if(cur == *head)
    {
        SCRIPT_NODE* tail = succ;
        while(tail->next != cur)
            tail = tail->next;
        *head = succ;
        tail->next = succ;
    }
    else
    {
        prev->next = succ;
    }
    free(cur);
    if(script_cnt > 0)
        script_cnt--;

    /* 前台改派 */
    if(was_fg)
    {
        if(*head)
            app_show(succ ? succ : *head);
        else
            app_hide();
    }
    else
    {
        app_restore_foreground();
    }
}

/* ------------------ 前台切换 ---------------------- */

static SCRIPT_NODE* app_prev_node(SCRIPT_NODE* cur)
{
    if(!g_app_list_head || !cur)
        return NULL;
    SCRIPT_NODE* p = g_app_list_head;
    if(p->next == cur)
        return p;
    p = p->next;
    while(p != g_app_list_head)
    {
        if(p->next == cur)
            return p;
        p = p->next;
    }
    return NULL;
}

/* 屏幕切换过渡: 从字符串名映射到 lvgl 枚举 (自己实现, 不用 luavgl 那套拼写) */
static lv_scr_load_anim_t anim_from_name(const char* s)
{
    if(!s) return LV_SCR_LOAD_ANIM_NONE;
    if(!strcmp(s, "over_left"))   return LV_SCR_LOAD_ANIM_OVER_LEFT;
    if(!strcmp(s, "over_right"))  return LV_SCR_LOAD_ANIM_OVER_RIGHT;
    if(!strcmp(s, "over_top"))    return LV_SCR_LOAD_ANIM_OVER_TOP;
    if(!strcmp(s, "over_bottom")) return LV_SCR_LOAD_ANIM_OVER_BOTTOM;
    if(!strcmp(s, "move_left"))   return LV_SCR_LOAD_ANIM_MOVE_LEFT;
    if(!strcmp(s, "move_right"))  return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    if(!strcmp(s, "move_top"))    return LV_SCR_LOAD_ANIM_MOVE_TOP;
    if(!strcmp(s, "move_bottom")) return LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
    if(!strcmp(s, "fade_in") || !strcmp(s, "fade_on")) return LV_SCR_LOAD_ANIM_FADE_IN;
    if(!strcmp(s, "fade_out"))    return LV_SCR_LOAD_ANIM_FADE_OUT;
    if(!strcmp(s, "out_left"))    return LV_SCR_LOAD_ANIM_OUT_LEFT;
    if(!strcmp(s, "out_right"))   return LV_SCR_LOAD_ANIM_OUT_RIGHT;
    if(!strcmp(s, "out_top"))     return LV_SCR_LOAD_ANIM_OUT_TOP;
    if(!strcmp(s, "out_bottom"))  return LV_SCR_LOAD_ANIM_OUT_BOTTOM;
    return LV_SCR_LOAD_ANIM_NONE;
}

/* 从 opts 表(可无) 取过渡参数; 缺省 = 不过渡 */
static void anim_from_opts(lua_State* L, int idx,
                           lv_scr_load_anim_t* anim, uint32_t* time, uint32_t* delay)
{
    *anim = LV_SCR_LOAD_ANIM_NONE;
    *time = 0;
    *delay = 0;
    if(!lua_istable(L, idx))
        return;
    lua_getfield(L, idx, "anim");
    if(lua_isstring(L, -1))
        *anim = anim_from_name(lua_tostring(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, idx, "time");
    if(lua_isinteger(L, -1))
        *time = (uint32_t)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, idx, "delay");
    if(lua_isinteger(L, -1))
        *delay = (uint32_t)lua_tointeger(L, -1);
    lua_pop(L, 1);
}

static int app_show_anim(SCRIPT_NODE* target, lv_scr_load_anim_t anim,
                         uint32_t time, uint32_t delay)
{
    if(target == NULL)
    {
        app_hide();
        return 0;
    }
    if(target == g_fg)
        return 0;

    if(g_fg)
        app_call_hook(g_fg, g_fg->on_hide_ref);

    g_fg = target;
    g_last_fg_id = target->node_id;

    if(anim == LV_SCR_LOAD_ANIM_NONE && time == 0)
        lv_scr_load(target->screen);
    else
        lv_scr_load_anim(target->screen, anim, time, delay, false);

    app_bind(target);
    app_call_hook(target, target->on_show_ref);
    app_restore_foreground();
    return 0;
}

static int app_show(SCRIPT_NODE* target)
{
    return app_show_anim(target, LV_SCR_LOAD_ANIM_NONE, 0, 0);
}

static void app_hide(void)
{
    if(g_fg)
        g_last_fg_id = g_fg->node_id;
    if(g_fg)
        app_call_hook(g_fg, g_fg->on_hide_ref);

    g_fg = NULL;
    if(g_blank)
        lv_scr_load(g_blank);
    app_restore_foreground();
}

static int app_switch(int dir, lv_scr_load_anim_t anim, uint32_t time, uint32_t delay)
{
    if(!g_app_list_head)
        return -1;

    SCRIPT_NODE* start = g_fg;
    if(!start)
        start = get_app_by_id(g_last_fg_id);

    SCRIPT_NODE* node;
    if(!start)
        node = g_app_list_head;
    else
        node = (dir >= 0) ? start->next : app_prev_node(start);
    if(!node)
        node = g_app_list_head;

    app_show_anim(node, anim, time, delay);
    return node->node_id;
}

/* ------------------ app load ---------------------- */

SCRIPT_NODE* app_load_from_file(char* dir, char* app_name)
{
    size_t len = 0;
    char tmp[256] = {0};
    static char entry_buf[SETUP_SCRIPT_BUF_LEN];
    lua_State* L = ptx_runtime()->L;
    int was_empty = (g_app_list_head == NULL);

    SCRIPT_NODE* cur_node = script_node_add(&g_app_list_head);
    if(cur_node == NULL)
        return NULL;

    /* 1. create _ENV table for this app */
    lua_newtable(L);
    lua_newtable(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    cur_node->env_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    /* name: 缺省取目录名 (如 "app/label" -> "label") */
    if(app_name)
    {
        strncpy(cur_node->name, app_name, sizeof(cur_node->name) - 1);
    }
    else
    {
        const char* base = dir ? strrchr(dir, '/') : NULL;
        base = base ? base + 1 : (dir ? dir : "app");
        if(base[0] == '\0')
            base = "app";
        strncpy(cur_node->name, base, sizeof(cur_node->name) - 1);
    }

    /* 2. create screen (每 app 独立 screen) */
    cur_node->screen = (lv_obj_t*)ptx_display()->create_app_root();
    if(cur_node->screen == NULL)
        goto add_node_fail;

    lv_style_init(&cur_node->screen_style);
    lv_style_set_bg_color(&cur_node->screen_style, lv_color_hex(0));
    lv_style_set_bg_opa(&cur_node->screen_style, LV_OPA_COVER);
    lv_style_set_radius(&cur_node->screen_style, 0);
    lv_style_set_pad_all(&cur_node->screen_style, 0);
    lv_style_set_border_width(&cur_node->screen_style, 0);
    lv_obj_add_style(cur_node->screen, &cur_node->screen_style, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(cur_node->screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(cur_node->screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(cur_node->screen, MATRIX_WIDTH, MATRIX_HEIGHT);

    /* 3. set current app + luavgl root (加载期对象的落点) */
    app_bind(cur_node);

    /* 4. set APPROOT in env table */
    lua_rawgeti(L, LUA_REGISTRYINDEX, cur_node->env_ref);
    lua_pushstring(L, dir);
    lua_setfield(L, -2, WORKING_DIR_VARIABLE_NAME);
    lua_pop(L, 1);

    /* 5. execute entry.lua with _ENV isolation */
    snprintf(tmp, sizeof(tmp), "%s%s", dir, SETUP_SCRIPT);
    if(ptx_os_fs_exists(tmp))
    {
        if(ptx_os_fs_read_file(tmp, entry_buf, sizeof(entry_buf), &len) == 0)
        {
            if(luaL_loadbuffer(L, entry_buf, len, "entry") != LUA_OK)
            {
                LOG_ERROR("entry load: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
                goto add_node_fail;
            }
            lua_rawgeti(L, LUA_REGISTRYINDEX, cur_node->env_ref);
            lua_setupvalue(L, -2, 1);
            if(lua_pcall(L, 0, 0, 0) != LUA_OK)
            {
                LOG_ERROR("entry: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
    }

    /* 6. call setup() if defined; 解析返回表 { loop=, background= } */
    lua_rawgeti(L, LUA_REGISTRYINDEX, cur_node->env_ref);   /* [env] */
    lua_getfield(L, -1, "setup");                           /* [env, setup] */
    if(lua_isfunction(L, -1))
    {
        int setup_idx = lua_gettop(L);   /* setup 函数位置, pcall 后结果从这里开始 */
        if(lua_pcall(L, 0, LUA_MULTRET, 0) == LUA_OK)
        {
            int nres = lua_gettop(L) - setup_idx + 1;
            if(nres >= 1 && lua_istable(L, setup_idx))
            {
                lua_getfield(L, setup_idx, "loop");
                if(lua_isinteger(L, -1))
                    cur_node->loop_interval = (int)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, setup_idx, "background");
                if(lua_isinteger(L, -1))
                    cur_node->bg_interval = (int)lua_tointeger(L, -1);
                lua_pop(L, 1);
            }
            lua_pop(L, nres);                               /* [env] */
        }
        else
        {
            LOG_ERROR("setup(): %s", lua_tostring(L, -1));
            lua_pop(L, 1);                                  /* [env] */
        }
    }
    else
    {
        lua_pop(L, 1);                                      /* [env] */
    }

    if(cur_node->loop_interval < MIN_INTERVAL_MS)
        cur_node->loop_interval = MIN_INTERVAL_MS;
    if(cur_node->bg_interval < MIN_INTERVAL_MS)
        cur_node->bg_interval = MIN_INTERVAL_MS;

    /* 7. register loop() as foreground timer if defined */
    lua_getfield(L, -1, "loop");
    if(lua_isfunction(L, -1))
    {
        int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        cur_node->loop_timer = ptx_timer_add(cur_node->node_id,
                                             cur_node->loop_interval,
                                             func_ref, 0);
    }
    else
    {
        lua_pop(L, 1);
    }

    /* 8. register background() as background timer if defined */
    lua_getfield(L, -1, "background");
    if(lua_isfunction(L, -1))
    {
        int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        cur_node->bg_timer = ptx_timer_add(cur_node->node_id,
                                           cur_node->bg_interval,
                                           func_ref, 1);
    }
    else
    {
        lua_pop(L, 1);
    }

    /* 9. on_show / on_hide hooks */
    lua_getfield(L, -1, "on_show");
    if(lua_isfunction(L, -1))
        cur_node->on_show_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    else
        lua_pop(L, 1);

    lua_getfield(L, -1, "on_hide");
    if(lua_isfunction(L, -1))
        cur_node->on_hide_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    else
        lua_pop(L, 1);

    lua_pop(L, 1);   /* pop env */

    LOG_DEBUG("create node: %p", cur_node);

    /* 10. 首个 app 自动成为前台; 其余不切 */
    if(was_empty)
        app_show(cur_node);
    else
        app_restore_foreground();

    return cur_node;

add_node_fail:
    app_del(&g_app_list_head, cur_node->node_id);
    return NULL;
}

//------------------ sys API (Lua side) ---------------------

static int do_load_app(lua_State* L)
{
    if(lua_isstring(L, 1))
    {
        const char* path = lua_tostring(L, 1);
        const char* name = lua_isstring(L, 2) ? lua_tostring(L, 2) : NULL;

        get_app_list();
        SCRIPT_NODE* node = app_load_from_file((char*)path, (char*)name);
        release_app_list();

        if(node)
        {
            lua_pushinteger(L, node->node_id);
            return 1;
        }
        lua_pushnil(L);
        lua_pushstring(L, "load failed");
        return 2;
    }

    lua_pushnil(L);
    lua_pushstring(L, "path required");
    return 2;
}

static int do_del_app(lua_State* state)
{
    int idx = -1;
    if(lua_isinteger(state, 1))
    {
        idx = (int)lua_tointeger(state, 1);
    }
    get_app_list();
    app_del(&g_app_list_head, idx);
    release_app_list();
    lua_pushboolean(state, 1);
    return 1;
}

static int do_ls_app(lua_State* state)
{
    get_app_list();
    script_node_ls(g_app_list_head);

    lua_newtable(state);
    SCRIPT_NODE* head = g_app_list_head;
    if(head)
    {
        int i = 1;
        SCRIPT_NODE* node = head;
        do
        {
            lua_newtable(state);
            lua_pushinteger(state, node->node_id);
            lua_setfield(state, -2, "id");
            lua_pushstring(state, node->name);
            lua_setfield(state, -2, "name");
            lua_pushboolean(state, node == g_fg);
            lua_setfield(state, -2, "fg");
            lua_rawseti(state, -2, i++);
            node = node->next;
        }while(node != head);
    }
    release_app_list();
    return 1;
}

static int do_show_app(lua_State* state)
{
    SCRIPT_NODE* node = NULL;
    get_app_list();
    if(lua_isinteger(state, 1))
        node = get_app_by_id((int)lua_tointeger(state, 1));
    else if(lua_isstring(state, 1))
        node = get_app_by_name(lua_tostring(state, 1));
    else if(lua_isnoneornil(state, 1))
    {
        node = get_app_by_id(g_last_fg_id);
        if(!node)
            node = g_app_list_head;
    }

    if(node)
    {
        lv_scr_load_anim_t anim; uint32_t t, d;
        anim_from_opts(state, 2, &anim, &t, &d);
        app_show_anim(node, anim, t, d);
        release_app_list();
        lua_pushboolean(state, 1);
        return 1;
    }
    release_app_list();
    lua_pushnil(state);
    lua_pushstring(state, "app not found");
    return 2;
}

static int do_hide_app(lua_State* state)
{
    get_app_list();
    app_hide();
    release_app_list();
    lua_pushboolean(state, 1);
    return 1;
}

static int do_current_app(lua_State* state)
{
    if(g_fg)
        lua_pushinteger(state, g_fg->node_id);
    else
        lua_pushnil(state);
    return 1;
}

/* 当前正在执行的 app 的 id (REPL 下为 nil) */
static int do_self(lua_State* state)
{
    SCRIPT_NODE* app = ptx_app_get_current();
    if(app)
        lua_pushinteger(state, app->node_id);
    else
        lua_pushnil(state);
    return 1;
}

static int do_next_app(lua_State* state)
{
    lv_scr_load_anim_t anim; uint32_t t, d;
    anim_from_opts(state, 1, &anim, &t, &d);
    int id = app_switch(1, anim, t, d);
    if(id < 0)
        lua_pushnil(state);
    else
        lua_pushinteger(state, id);
    return 1;
}

static int do_prev_app(lua_State* state)
{
    lv_scr_load_anim_t anim; uint32_t t, d;
    anim_from_opts(state, 1, &anim, &t, &d);
    int id = app_switch(-1, anim, t, d);
    if(id < 0)
        lua_pushnil(state);
    else
        lua_pushinteger(state, id);
    return 1;
}

static int do_set_log_lvl(lua_State* state)
{
    int level = 0;
    if(lua_isinteger(state, 1))
    {
        level = (int)lua_tointeger(state, 1);
    }
    log_set_level(level);
    return 1;
}

static int do_set_brightness(lua_State* state)
{
    int brightness = 0;
    if(lua_isinteger(state, 1))
    {
        brightness = (int)lua_tointeger(state, 1);
    }
    render_hardware_setting(HW_BRIGHTNESS, &brightness, sizeof(int));
    return 1;
}

static const struct luaL_Reg sys_lib[] =
{
    {"load", do_load_app},
    {"ls", do_ls_app},
    {"del", do_del_app},
    {"show", do_show_app},
    {"hide", do_hide_app},
    {"current", do_current_app},
    {"self", do_self},
    {"next", do_next_app},
    {"prev", do_prev_app},
    {"log_lvl", do_set_log_lvl},
    {"set_brightness", do_set_brightness},
    {NULL, NULL}
};

int sys_getapi(lua_State* state)
{
    luaL_newlib(state, sys_lib);
    ptx_key_lua_attach(state);
    return 1;
}
