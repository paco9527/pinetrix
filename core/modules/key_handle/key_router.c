#include "key_router.h"

#include <string.h>

#include "runtime.h"
#include "log.h"
#include "ptx_key.h"
#include "script_import.h"

#define PTX_MAX_KEY_BINDINGS 16

typedef struct {
    int handle;
    int app_id;     /* 注册者 app; -1=REPL/全局(无需绑定 app) */
    int ref;        /* luaL_ref; LUA_NOREF 表示空槽 */
    int always;
} key_binding_t;

static key_binding_t g_keyb[PTX_MAX_KEY_BINDINGS];
static int g_keyb_next = 1;
static int g_keyb_inited = 0;

/* 静态数组零初始化, 而 ref==0 是合法 luaL_ref, 故需显式把空槽标成 LUA_NOREF */
static void key_router_ensure_init(void)
{
    if (g_keyb_inited)
        return;
    for (int i = 0; i < PTX_MAX_KEY_BINDINGS; i++)
        g_keyb[i].ref = LUA_NOREF;
    g_keyb_inited = 1;
}

static key_binding_t* key_slot_by_handle(int handle)
{
    for (int i = 0; i < PTX_MAX_KEY_BINDINGS; i++)
        if (g_keyb[i].ref != LUA_NOREF && g_keyb[i].handle == handle)
            return &g_keyb[i];
    return NULL;
}

static void key_slot_release(key_binding_t* b)
{
    if (b->ref != LUA_NOREF)
        luaL_unref(ptx_runtime()->L, LUA_REGISTRYINDEX, b->ref);
    b->ref = LUA_NOREF;
}

int ptx_key_router_remove(int handle)
{
    key_router_ensure_init();
    key_binding_t* b = key_slot_by_handle(handle);
    if (!b)
        return 0;
    key_slot_release(b);
    return 1;
}

void ptx_key_router_unregister_app(int app_id)
{
    key_router_ensure_init();
    for (int i = 0; i < PTX_MAX_KEY_BINDINGS; i++)
        if (g_keyb[i].ref != LUA_NOREF && g_keyb[i].app_id == app_id)
            key_slot_release(&g_keyb[i]);
}

int ptx_key_router_add(int app_id, int ref, int always)
{
    key_router_ensure_init();
    for (int i = 0; i < PTX_MAX_KEY_BINDINGS; i++) {
        if (g_keyb[i].ref == LUA_NOREF) {
            g_keyb[i].handle = g_keyb_next++;
            g_keyb[i].app_id = app_id;
            g_keyb[i].ref = ref;
            g_keyb[i].always = always;
            return g_keyb[i].handle;
        }
    }
    return -1;
}

/* 绑定条目所属 app 的 env/root, pcall 回调, 还原。
 * 返回 handled(0/1); -1 表示注册者已删除(由调用方释放槽)。 */
static int key_invoke(key_binding_t* b, const char* id, const char* ev)
{
    lua_State* L = ptx_runtime()->L;
    SCRIPT_NODE* app = NULL;
    if (b->app_id >= 0) {
        app = get_app_by_id(b->app_id);
        if (!app)
            return -1;
    }

    int top = lua_gettop(L);
    if (app)
        app_bind(app);

    lua_rawgeti(L, LUA_REGISTRYINDEX, b->ref);
    lua_pushstring(L, id);
    lua_pushstring(L, ev);
    int handled = 0;
    if (lua_pcall(L, 2, 1, 0) == LUA_OK)
        handled = lua_toboolean(L, -1);
    else
        LOG_ERROR("key cb: %s", lua_tostring(L, -1));
    lua_settop(L, top);

    if (app)
        app_restore_foreground();
    return handled;
}

/* 走一趟并调用匹配的条目; 命中(消费)即停。返回 1 = 已消费 */
static int dispatch_pass(int always_pass, int fg_id, const char* id, const char* ev)
{
    for (int i = 0; i < PTX_MAX_KEY_BINDINGS; i++) {
        key_binding_t* b = &g_keyb[i];
        if (b->ref == LUA_NOREF)
            continue;
        if (always_pass) {
            if (!b->always)
                continue;
        } else {
            if (b->always || b->app_id != fg_id)
                continue;
        }

        int r = key_invoke(b, id, ev);
        if (r < 0) {              /* 注册者已删除, 顺手清理 */
            key_slot_release(b);
            continue;
        }
        if (r)
            return 1;
    }
    return 0;
}

/* 保留键 -> 只给 always; 普通键 -> 前台条目优先, 未消费再给 always */
static void key_event_cb(const char* id, int ev, void* ud)
{
    (void)ud;
    static const char* names[] = { "down", "short", "long", "repeat", "up" };
    const char* name = (ev >= 0 && ev < 5) ? names[ev] : "?";
    int reserved;

    key_router_ensure_init();
    reserved = ptx_key_is_reserved(id);
    SCRIPT_NODE* fg = app_get_foreground();
    int fg_id = fg ? fg->node_id : -1;

    if (!reserved && dispatch_pass(0, fg_id, id, name))
        return;
    dispatch_pass(1, fg_id, id, name);
}

void ptx_key_router_dispatch(const char* id, int ev)
{
    key_event_cb(id, ev, NULL);
}

void ptx_key_router_init(void)
{
    key_router_ensure_init();
    ptx_key_set_cb(key_event_cb, NULL);
    ptx_key_init();
}
