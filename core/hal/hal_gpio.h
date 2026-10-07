/*
 * ptx_hal_gpio -- GPIO 硬件抽象层
 *
 * 平台无关接口; 具体实现由后端提供:
 *   hal_gpio_linux.c  (Linux GPIO character device v2, /dev/gpiochipN)
 *   hal_gpio_dummy.c  (测试用)
 * 不 include 任何平台头, 只依赖 stdint/stddef。
 *
 * 引脚寻址串 (spec) 由后端解析:
 *   linux:  "BOARD11" (物理排针) | "BCM17" | "gpiochip0:17"
 *   stm32:  "PG6" | "PC13"
 */
#ifndef _PTX_HAL_GPIO_H
#define _PTX_HAL_GPIO_H

#include <stdint.h>

typedef enum {
    PTX_GPIO_PULL_NONE = 0,
    PTX_GPIO_PULL_UP,
    PTX_GPIO_PULL_DOWN,
} ptx_gpio_pull_t;

typedef struct ptx_HalPin ptx_HalPin;   /* 不透明句柄 */

int  ptx_hal_gpio_init(void);
void ptx_hal_gpio_deinit(void);

/* 按寻址串打开一个引脚, 失败返回 NULL */
ptx_HalPin* ptx_hal_pin_open(const char* spec);
void        ptx_hal_pin_close(ptx_HalPin* pin);

int ptx_hal_pin_config_input(ptx_HalPin* pin, ptx_gpio_pull_t pull);
int ptx_hal_pin_read(ptx_HalPin* pin);                  /* 返回 0/1, 出错 -1 */

int ptx_hal_pin_config_output(ptx_HalPin* pin, int initial_level);
int ptx_hal_pin_write(ptx_HalPin* pin, int level);      /* 0/1, 出错 -1 */

#endif /* _PTX_HAL_GPIO_H */
