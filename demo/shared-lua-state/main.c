/*
 * demo: 单 lua_State + _ENV 隔离 + timer 事件驱动
 *
 * 编译（Linux）：
 *   gcc main.c -o demo -llua -ldl -lm
 *
 * 编译（MinGW）：
 *   gcc main.c -o demo.exe -llua -lm
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

/* ============================================================
 * 常量
 * ============================================================ */

#define MAX_TIMERS  32
#define MAX_APPS    8
#define MAX_NAME    64

/* ============================================================
 * 单 lua_State + 全局运行时
 * ============================================================ */

typedef struct {
    int     func_ref;
    int     period_ms;
    int64_t next_ms;
} Timer;

typedef struct {
    char    name[MAX_NAME];
    int     env_ref;
} App;

typedef struct {
    lua_State*  L;
    Timer       timers[MAX_TIMERS];
    int         timer_count;
    App         apps[MAX_APPS];
    int         app_count;
    int         running;
} Runtime;

static Runtime R = {0};

static int64_t now_ms(void)
{
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* ============================================================
 * Lua API
 * ============================================================ */

static int plat_set_timer(lua_State *L)
{
    int period = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (R.timer_count >= MAX_TIMERS)
        return luaL_error(L, "too many timers");

    Timer *t = &R.timers[R.timer_count++];
    t->period_ms = period;
    t->func_ref  = luaL_ref(L, LUA_REGISTRYINDEX);
    t->next_ms   = now_ms() + period;

    return 0;
}

static int plat_log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    fprintf(stdout, "[app] %s\n", msg);
    return 0;
}

static int plat_dump(lua_State *L)
{
    int i;
    for (i = 0; i < R.app_count; i++) {
        fprintf(stdout, "\n--- app: %s ---\n", R.apps[i].name);

        lua_rawgeti(L, LUA_REGISTRYINDEX, R.apps[i].env_ref);

        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const char *k = lua_tostring(L, -2);
            if (k && k[0] != '_') {
                const char *v = lua_tostring(L, -1);
                fprintf(stdout, "  %s = %s\n", k, v ? v : "(type not string)");
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    fflush(stdout);
    return 0;
}

static const luaL_Reg plat_lib[] = {
    {"set_timer", plat_set_timer},
    {"log",       plat_log},
    {"dump",      plat_dump},
    {NULL, NULL}
};

/* ============================================================
 * _ENV 隔离
 * ============================================================ */

static int load_app(const char *filepath, const char *name)
{
    lua_State *L = R.L;

    if (luaL_loadfile(L, filepath) != LUA_OK) {
        fprintf(stderr, "[ERROR] load %s: %s\n", filepath, lua_tostring(L, -1));
        lua_pop(L, 1);
        return -1;
    }

    lua_newtable(L);

    lua_newtable(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);

    lua_setupvalue(L, -2, 1);

    lua_pushvalue(L, -1);
    if (R.app_count < MAX_APPS) {
        strncpy(R.apps[R.app_count].name, name, MAX_NAME - 1);
        R.apps[R.app_count].env_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        R.app_count++;
    } else {
        lua_pop(L, 1);
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "[ERROR] exec %s: %s\n", filepath, lua_tostring(L, -1));
        lua_pop(L, 1);
        return -1;
    }

    fprintf(stdout, "[OK] loaded app: %s  (%s)\n", name, filepath);
    fflush(stdout);
    return 0;
}

/* ============================================================
 * Timer 事件循环
 * ============================================================ */

static void run_event_loop(void)
{
    lua_State *L = R.L;

    fprintf(stdout, "\n>>> Event loop started.\n");
    fprintf(stdout, "    Timers drive all updates. No polling, no re-parsing.\n");
    fprintf(stdout, "    Press Enter to dump app states, Ctrl+C to exit.\n\n");
    fflush(stdout);

    R.running = 1;

    while (R.running) {
        int64_t now = now_ms();
        int i;

        for (i = 0; i < R.timer_count; i++) {
            Timer *t = &R.timers[i];
            if (now >= t->next_ms) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, t->func_ref);
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    fprintf(stderr, "[TIMER ERROR] %s\n", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
                t->next_ms = now + t->period_ms;
            }
        }

#ifdef _WIN32
        if (_kbhit()) {
            int c = _getch();
            if (c == 'q' || c == 'Q') {
                R.running = 0;
            } else if (c == '\r' || c == '\n') {
                plat_dump(L);
            }
        }
#else
        {
            /* 非阻塞检查 stdin */
            static char buf[4];
            if (fgets(buf, sizeof(buf), stdin)) {
                if (buf[0] == 'q' || buf[0] == 'Q')
                    R.running = 0;
                else if (buf[0] == '\n' || buf[0] == '\r')
                    plat_dump(L);
            }
        }
#endif

        sleep_ms(10);
    }
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    lua_State *L = luaL_newstate();
    R.L = L;

    luaL_openlibs(L);

    lua_newtable(L);
    luaL_setfuncs(L, plat_lib, 0);
    lua_setglobal(L, "platform");

    load_app("apps/clock.lua",   "clock");
    load_app("apps/counter.lua", "counter");

    run_event_loop();

    lua_close(L);
    fprintf(stdout, "Bye.\n");
    return 0;
}
