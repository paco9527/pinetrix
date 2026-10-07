#include "hal_screen.h"
#include "ws2811.h"
#include "ptx_config.h"

#include <string.h>

struct ptx_HalScreen {
	ws2811_t ledstring;
	uint16_t width;
	uint16_t height;
};

ptx_HalScreen* ptx_hal_screen(void)
{
	static ptx_HalScreen instance;
	return &instance;
}

/* S 形走线坐标转换: 第一列从上到下, 延伸到底时连接相邻灯珠, 整体从左往右 */
static uint16_t ptx_hal_pos_convert(uint16_t x, uint16_t y,
				    uint16_t width, uint16_t height)
{
	if (x % 2)
		return (x + 1) * height - 1 - y;
	else
		return x * height + y;
}

int ptx_hal_screen_init(uint16_t w, uint16_t h)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	ws2811_t* lib_setting = &hs->ledstring;

	memset(hs, 0, sizeof(*hs));

	lib_setting->freq = WS2811_TARGET_FREQ;
	lib_setting->dmanum = WS2812_DMA;
	lib_setting->channel[0].gpionum = GPIO_PIN;
	lib_setting->channel[0].invert = 0;
	lib_setting->channel[0].count = w * h;
	lib_setting->channel[0].strip_type = WS2811_STRIP_GRB;
	lib_setting->channel[0].brightness = 32;

	hs->width = w;
	hs->height = h;

	return ws2811_init(&hs->ledstring);
}

void ptx_hal_screen_deinit(void)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	ws2811_fini(&hs->ledstring);
}

void ptx_hal_screen_flush(int x1, int y1, int x2, int y2, const void* buf)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	ws2811_t* lib_setting = &hs->ledstring;
	const uint32_t* color_p = (const uint32_t*)buf;
	int x, y;

	for (y = y1; y <= y2; y++) {
		for (x = x1; x <= x2; x++) {
			uint16_t idx = ptx_hal_pos_convert(x, y,
							    hs->width,
							    hs->height);
			lib_setting->channel[0].leds[idx] = *color_p;
			color_p++;
		}
	}

	ws2811_render(&hs->ledstring);
}

void ptx_hal_screen_set_brightness(uint8_t brightness)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	hs->ledstring.channel[0].brightness = brightness;
}

void* ptx_hal_screen_get_led_buffer(void)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	return hs->ledstring.channel[0].leds;
}
