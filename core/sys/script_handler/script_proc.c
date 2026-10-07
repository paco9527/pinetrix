#include "script_proc.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "log.h"
#include "lvgl.h"
#include "ptx_config.h"
#include "script_import.h"
#include "cmd_queue.h"
#include "runtime.h"
#include "os_thread.h"
#include "os_console.h"

static void repl_thread(void* arg)
{
    char buf[PTX_CMD_LINE_LEN];
    (void)arg;
    LOG_INFO("Enter pinetrix REPL. Lua statements only.");
    while (ptx_os_console_read_line(buf, sizeof(buf)) > 0) {
        if (buf[0] == '\n' || buf[0] == '\r') continue;
        ptx_cmd_queue_push(buf);
    }
}

void manager_cmdline_init(void)
{
    ptx_os_thread_start(repl_thread, NULL);
}

int loader_init(void)
{
    ptx_runtime_init();
    ptx_cmd_queue_init();

    app_manager_init();

    manager_cmdline_init();

    return 0;
}

void loader_deinit(void)
{
    app_clear(get_script_list_head());
    lv_deinit();
    ptx_runtime_deinit();
}
