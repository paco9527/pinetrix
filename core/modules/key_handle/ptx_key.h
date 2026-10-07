#ifndef _PTX_KEY_H
#define _PTX_KEY_H

#include <stdint.h>

typedef enum {
    PTX_KEY_DOWN = 0,    /* 按下(消抖后触发一次) */
    PTX_KEY_SHORT,       /* 短按(释放时触发) */
    PTX_KEY_LONG,        /* 长按(达到阈值时触发一次) */
    PTX_KEY_REPEAT,      /* 长按保持期间周期触发 */
    PTX_KEY_UP,          /* 长按后释放 */
} ptx_key_ev_t;

/* 单个按键配置 (板级表放在 key_cfg.c, 换板子改那个文件) */
typedef struct {
    const char* id;            /* 逻辑名, 事件回调/查询用 */
    const char* spec;          /* ptx_hal_pin_open 寻址串, 如 "BOARD11" */
    int         active_low;    /* 1=按下接地(内部上拉) */
    int         long_press_ms; /* 长按阈值, 0=默认 */
    int         reserved;      /* 1=保留键(优先给 always/系统条目) */
} ptx_key_cfg_t;

/* 板级按键表 */
const ptx_key_cfg_t* ptx_key_cfg(int* count);

int  ptx_key_init(void);
void ptx_key_tick(void);

int  ptx_key_pressed(const char* id);      /* 当前是否按下; -1=未知 id */
int  ptx_key_is_reserved(const char* id);  /* 1=保留键 */

typedef void (*ptx_key_cb_t)(const char* id, int ev, void* ud);
void ptx_key_set_cb(ptx_key_cb_t cb, void* ud);

#endif /* _PTX_KEY_H */
