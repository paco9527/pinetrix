/* ptx_hal_screen -- WS2812 点阵屏硬件抽象层
 *
 * 不依赖 lua.h / lvgl.h, 只依赖 ws2811.h 和 ptx_config.h
 * 上层通过此接口操作硬件, 换屏幕驱动只需改此文件的实现
 */
#ifndef _PTX_HAL_SCREEN_H
#define _PTX_HAL_SCREEN_H

#include <stdint.h>

typedef struct ptx_HalScreen ptx_HalScreen;

ptx_HalScreen* ptx_hal_screen(void);

int  ptx_hal_screen_init(uint16_t w, uint16_t h);
void ptx_hal_screen_deinit(void);

/* 将矩形区域像素写入 LED 缓冲区并渲染
 * buf: lv_color_t* 数组的首地址, 每个像素占 sizeof(lv_color_t) (=4 字节 RGB888)
 * 内部处理 S 形走线坐标转换 + ws2811_render
 */
void ptx_hal_screen_flush(int x1, int y1, int x2, int y2, const void* buf);

void ptx_hal_screen_set_brightness(uint8_t brightness);

/* 获取 LED 显存指针 (给 graph 等非 lvgl 渲染路径用) */
void* ptx_hal_screen_get_led_buffer(void);

#endif
