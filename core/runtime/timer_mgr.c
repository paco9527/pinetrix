#include "timer_mgr.h"
#include "runtime.h"
#include "platform_api.h"
#include "script_import.h"
#include "luavgl.h"
#include "log.h"
#include "os_time.h"
#include <string.h>

static ptx_Timer s_timers[PTX_MAX_TIMERS];
static int s_timer_count = 0;
static int s_next_id = 1;

void ptx_timer_init(void)
{
	memset(s_timers, 0, sizeof(s_timers));
	s_timer_count = 0;
	s_next_id = 1;
}

int ptx_timer_add(int app_id, int period_ms, int func_ref, int background)
{
	int i;
	if (s_timer_count >= PTX_MAX_TIMERS)
		return -1;

	for (i = 0; i < PTX_MAX_TIMERS; i++) {
		if (!s_timers[i].active)
			goto found;
	}
	return -1;

found:
	s_timers[i].id        = s_next_id++;
	s_timers[i].app_id    = app_id;
	s_timers[i].func_ref  = func_ref;
	s_timers[i].period_ms = period_ms;
	s_timers[i].next_ms   = ptx_os_time_now_ms() + period_ms;
	s_timers[i].active    = 1;
	s_timers[i].background = background;
	s_timer_count++;
	return s_timers[i].id;
}

void ptx_timer_remove(int timer_id)
{
	int i;
	for (i = 0; i < PTX_MAX_TIMERS; i++) {
		if (s_timers[i].active && s_timers[i].id == timer_id) {
			lua_State* L = ptx_runtime()->L;
			luaL_unref(L, LUA_REGISTRYINDEX, s_timers[i].func_ref);
			s_timers[i].active = 0;
			s_timer_count--;
			return;
		}
	}
}

void ptx_timer_remove_app(int app_id)
{
	int i;
	lua_State* L = ptx_runtime()->L;
	for (i = 0; i < PTX_MAX_TIMERS; i++) {
		if (s_timers[i].active && s_timers[i].app_id == app_id) {
			luaL_unref(L, LUA_REGISTRYINDEX, s_timers[i].func_ref);
			s_timers[i].active = 0;
			s_timer_count--;
		}
	}
}

void ptx_timer_tick(void)
{
	int i;
	int64_t now = ptx_os_time_now_ms();
	lua_State* L = ptx_runtime()->L;

	for (i = 0; i < PTX_MAX_TIMERS; i++) {
		ptx_Timer* t = &s_timers[i];
		if (!t->active || now < t->next_ms)
			continue;

		SCRIPT_NODE* app = get_app_by_id(t->app_id);
		if (!app) {
			luaL_unref(L, LUA_REGISTRYINDEX, t->func_ref);
			t->active = 0;
			s_timer_count--;
			continue;
		}

		/* 非前台时跳过前台 loop; 后台计时器常驻. 跳过时不推进 next_ms,
		 * 切回前台会立即补一次刷新 */
		if (!t->background && app != app_get_foreground())
			continue;

		app_bind(app);

		lua_rawgeti(L, LUA_REGISTRYINDEX, t->func_ref);
		if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
			LOG_ERROR("timer cb: %s", lua_tostring(L, -1));
			lua_pop(L, 1);
		}

		app_restore_foreground();

		t->next_ms = now + t->period_ms;
	}
}
