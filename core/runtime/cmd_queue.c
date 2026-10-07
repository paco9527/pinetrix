#include "cmd_queue.h"
#include "runtime.h"
#include <lauxlib.h>
#include <lua.h>
#include <string.h>
#include <stdio.h>
#include "os_thread.h"
#include "log.h"

static char s_queue[PTX_CMD_QUEUE_SIZE][PTX_CMD_LINE_LEN];
static int  s_head;
static int  s_tail;
static ptx_os_mutex_t* s_mutex = NULL;

void ptx_cmd_queue_init(void)
{
	s_head = s_tail = 0;
	if (!s_mutex)
		s_mutex = ptx_os_mutex_create();
}

int ptx_cmd_queue_push(const char* line)
{
	ptx_os_mutex_lock(s_mutex);
	int next = (s_tail + 1) % PTX_CMD_QUEUE_SIZE;
	if (next == s_head) {
		ptx_os_mutex_unlock(s_mutex);
		return -1;
	}
	strncpy(s_queue[s_tail], line, PTX_CMD_LINE_LEN - 1);
	s_queue[s_tail][PTX_CMD_LINE_LEN - 1] = '\0';
	s_tail = next;
	ptx_os_mutex_unlock(s_mutex);
	return 0;
}

int ptx_cmd_queue_pop(char* out)
{
	ptx_os_mutex_lock(s_mutex);
	if (s_head == s_tail) {
		ptx_os_mutex_unlock(s_mutex);
		return -1;
	}
	strcpy(out, s_queue[s_head]);
	s_head = (s_head + 1) % PTX_CMD_QUEUE_SIZE;
	ptx_os_mutex_unlock(s_mutex);
	return 0;
}

void ptx_cmd_queue_process(void)
{
	lua_State* L = ptx_runtime()->L;
	char line[PTX_CMD_LINE_LEN];
	int status;

	while (ptx_cmd_queue_pop(line) == 0) {
		if (luaL_loadstring(L, line) != LUA_OK
		    || (status = lua_pcall(L, 0, 0, 0)) != LUA_OK) {
			LOG_ERROR("%s", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
}
