/*
 * hal_gpio_dummy -- 测试/无 GPIO 平台的后端
 * 引脚打开总是成功, 电平恒为 0(释放); 可通过 ptx_hal_gpio_dummy_set 注入(测试用)。
 */
#include "hal_gpio.h"
#include <stdlib.h>

struct ptx_HalPin {
    int level;
    int is_output;
};

int  ptx_hal_gpio_init(void) { return 0; }
void ptx_hal_gpio_deinit(void) {}

ptx_HalPin* ptx_hal_pin_open(const char* spec)
{
    (void)spec;
    ptx_HalPin* pin = (ptx_HalPin*)calloc(1, sizeof(*pin));
    if (pin)
        pin->level = 1;   /* 空闲高(配 active_low 时表示释放) */
    return pin;
}

void ptx_hal_pin_close(ptx_HalPin* pin) { free(pin); }

int ptx_hal_pin_config_input(ptx_HalPin* pin, ptx_gpio_pull_t pull)
{
    (void)pin; (void)pull;
    return 0;
}

int ptx_hal_pin_read(ptx_HalPin* pin)
{
    return pin ? pin->level : -1;
}

int ptx_hal_pin_config_output(ptx_HalPin* pin, int initial_level)
{
    if (!pin) return -1;
    pin->is_output = 1;
    pin->level = initial_level ? 1 : 0;
    return 0;
}

int ptx_hal_pin_write(ptx_HalPin* pin, int level)
{
    if (!pin) return -1;
    pin->level = level ? 1 : 0;
    return 0;
}

/* 测试辅助: 注入输入电平 */
void ptx_hal_gpio_dummy_set(ptx_HalPin* pin, int level)
{
    if (pin && !pin->is_output)
        pin->level = level ? 1 : 0;
}
