#ifndef _PTX_TIMER_MGR_H
#define _PTX_TIMER_MGR_H

#include <stdint.h>

#define PTX_MAX_TIMERS 32

typedef struct {
	int      id;
	int      app_id;
	int      func_ref;
	int      period_ms;
	int64_t  next_ms;
	int      active;
	int      background;   /* 0=前台 loop, 1=后台 background */
} ptx_Timer;

void ptx_timer_init(void);
int  ptx_timer_add(int app_id, int period_ms, int func_ref, int background);
void ptx_timer_remove(int timer_id);
void ptx_timer_remove_app(int app_id);
void ptx_timer_tick(void);

#endif
