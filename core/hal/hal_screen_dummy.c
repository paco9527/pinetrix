#include "hal_screen.h"
#include "ptx_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ptx_HalScreen {
	uint16_t width;
	uint16_t height;
	uint32_t* framebuffer;
	int frame_count;
};

ptx_HalScreen* ptx_hal_screen(void)
{
	static ptx_HalScreen instance;
	return &instance;
}

int ptx_hal_screen_init(uint16_t w, uint16_t h)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	hs->width = w;
	hs->height = h;
	hs->framebuffer = calloc(w * h, sizeof(uint32_t));
	hs->frame_count = 0;
	printf("\033[2J\033[H");
	return 0;
}

void ptx_hal_screen_deinit(void)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	free(hs->framebuffer);
	hs->framebuffer = NULL;
	printf("\033[2J\033[H");
}

void ptx_hal_screen_flush(int x1, int y1, int x2, int y2, const void* buf)
{
	ptx_HalScreen* hs = ptx_hal_screen();
	const uint32_t* color_p = (const uint32_t*)buf;
	int x, y;

	for (y = y1; y <= y2; y++) {
		for (x = x1; x <= x2; x++) {
			hs->framebuffer[y * hs->width + x] = *color_p;
			color_p++;
		}
	}

	hs->frame_count++;
	printf("\033[2J\033[H");
	for (y = 0; y < hs->height; y++) {
		for (x = 0; x < hs->width; x++) {
			uint32_t px = hs->framebuffer[y * hs->width + x];
			if (px & 0xFFFFFF) {
				putchar('#');
				// putchar('#');
			} else {
				putchar('-');
				// putchar(' ');
			}
		}
		putchar('\n');
	}
	printf("frame: %-6d\n", hs->frame_count);
	fflush(stdout);
}

void ptx_hal_screen_set_brightness(uint8_t brightness)
{
	(void)brightness;
}

void* ptx_hal_screen_get_led_buffer(void)
{
	return ptx_hal_screen()->framebuffer;
}
