/*
 * ptx_key -- 平台无关的按键扫描: 轮询 + 消抖 + 短/长/重复事件
 * 通过 hal_gpio 读引脚, 通过 ptx_os_time 计时; 事件经 ptx_key_cb 回调送出。
 */
#include "ptx_key.h"
#include "hal_gpio.h"
#include "os_time.h"
#include "log.h"

#include <string.h>
#include <stdlib.h>

#define PTX_KEY_DEBOUNCE_MS 30
#define PTX_KEY_DEFAULT_LONG_MS 800

typedef struct {
    const ptx_key_cfg_t* cfg;
    ptx_HalPin* pin;
    int     has_pin;
    int     stable;         /* 稳定电平: 按下=1 */
    int     raw;            /* 最近一次原始采样 */
    int64_t raw_since;
    int64_t down_ms;
    int     long_fired;
    int64_t last_repeat_ms;
} ptx_key_state_t;

static ptx_key_state_t* s_keys = NULL;
static int              s_count = 0;
static ptx_key_cb_t     s_cb = NULL;
static void*            s_cb_ud = NULL;

static ptx_key_state_t* key_find(const char* id)
{
    if (!id) return NULL;
    for (int i = 0; i < s_count; i++)
        if (strcmp(s_keys[i].cfg->id, id) == 0)
            return &s_keys[i];
    return NULL;
}

static void key_emit(const char* id, int ev)
{
    if (s_cb)
        s_cb(id, ev, s_cb_ud);
}

int ptx_key_pressed(const char* id)
{
    ptx_key_state_t* k = key_find(id);
    return k ? k->stable : -1;
}

int ptx_key_is_reserved(const char* id)
{
    ptx_key_state_t* k = key_find(id);
    return k ? k->cfg->reserved : 0;
}

void ptx_key_set_cb(ptx_key_cb_t cb, void* ud)
{
    s_cb = cb;
    s_cb_ud = ud;
}

int ptx_key_init(void)
{
    int n = 0;
    const ptx_key_cfg_t* cfg = ptx_key_cfg(&n);
    s_count = n;
    s_keys = (ptx_key_state_t*)calloc(n ? n : 1, sizeof(*s_keys));
    if (!s_keys) {
        s_count = 0;
        return -1;
    }

    for (int i = 0; i < n; i++) {
        ptx_key_state_t* k = &s_keys[i];
        k->cfg = &cfg[i];
        k->pin = ptx_hal_pin_open(cfg[i].spec);
        if (!k->pin) {
            LOG_ERROR("key %s: open '%s' failed", cfg[i].id, cfg[i].spec);
            continue;
        }
        ptx_hal_pin_config_input(k->pin,
                                 cfg[i].active_low ? PTX_GPIO_PULL_UP
                                                   : PTX_GPIO_PULL_DOWN);
        k->has_pin = 1;

        int lvl = ptx_hal_pin_read(k->pin);
        int pressed = cfg[i].active_low ? (lvl == 0) : (lvl == 1);
        k->stable = pressed;
        k->raw = pressed;
        k->raw_since = ptx_os_time_now_ms();
    }
    return 0;
}

void ptx_key_tick(void)
{
    int64_t now = ptx_os_time_now_ms();

    for (int i = 0; i < s_count; i++) {
        ptx_key_state_t* k = &s_keys[i];
        if (!k->has_pin)
            continue;

        int lvl = ptx_hal_pin_read(k->pin);
        if (lvl < 0)
            continue;
        int pressed = k->cfg->active_low ? (lvl == 0) : (lvl == 1);

        /* 原始值变化 -> 重新计时消抖 */
        if (pressed != k->raw) {
            k->raw = pressed;
            k->raw_since = now;
            continue;
        }
        if (now - k->raw_since < PTX_KEY_DEBOUNCE_MS)
            continue;

        /* 原始值已稳定 */
        if (pressed == k->stable) {
            if (k->stable) {
                int lp = k->cfg->long_press_ms > 0 ? k->cfg->long_press_ms
                                                   : PTX_KEY_DEFAULT_LONG_MS;
                if (!k->long_fired && now - k->down_ms >= lp) {
                    k->long_fired = 1;
                    k->last_repeat_ms = now;
                    key_emit(k->cfg->id, PTX_KEY_LONG);
                } else if (k->long_fired && now - k->last_repeat_ms >= lp) {
                    k->last_repeat_ms = now;
                    key_emit(k->cfg->id, PTX_KEY_REPEAT);
                }
            }
            continue;
        }

        /* 稳定电平发生跳变 */
        k->stable = pressed;
        if (pressed) {
            k->down_ms = now;
            k->long_fired = 0;
            key_emit(k->cfg->id, PTX_KEY_DOWN);
        } else {
            key_emit(k->cfg->id,
                     k->long_fired ? PTX_KEY_UP : PTX_KEY_SHORT);
        }
    }
}
