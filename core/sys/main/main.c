#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "log.h"
#include "render.h"
#include "display.h"
#include "script_proc.h"
#include "timer_mgr.h"
#include "cmd_queue.h"
#include "ptx_key.h"
#include "os_time.h"
#include "ptx_config.h"

volatile sig_atomic_t ptx_run_flag = 0;

void exit_handler(int sig)
{
    ptx_run_flag = 0;
}

int main(int argc, char** argv)
{
    struct sigaction act = {0};
    act.sa_handler = exit_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    RENDER render = NULL;

    render = render_init(MATRIX_WIDTH, MATRIX_HEIGHT);
    ptx_run_flag = 1;

    loader_init();

    while(ptx_run_flag)
    {
        lv_tick_inc(PTX_TICK_MS);
        ptx_key_tick();
        ptx_display()->tick();
        ptx_timer_tick();
        ptx_cmd_queue_process();
        ptx_os_sleep_ms(PTX_TICK_MS);
    }
    render_deinit(render);
    loader_deinit();

    return 0;
}
