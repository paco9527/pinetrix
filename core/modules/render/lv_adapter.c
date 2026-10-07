#include <stdio.h>
#include <stdint.h>
#include "lvgl.h"
#include "lv_adapter.h"
#include "lv_conf.h"
#include "hal/hal_screen.h"
#include "ptx_config.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t color_buf[MATRIX_WIDTH * MATRIX_HEIGHT];
static lv_disp_drv_t disp_drv;

#if 0
static void print_Gram(lv_color_t *buffer, const lv_area_t * area)
{
    static uint32_t dump_scr[MATRIX_WIDTH*MATRIX_HEIGHT] = {0};
    uint32_t count = 0;
    int x = 0;
    int y = 0;

    for(y = area->y1 ; y <= area->y2 ; y++)
    {
        for(x = area->x1 ; x <= area->x2 ; x++)
        {
            dump_scr[x+y*32] = buffer->full;
            buffer++;
            count++;
        }
    }

    printf("gram start\n");
    for(y = 0 ; y < 8 ; y++)
    {
        for(x = 0 ; x < 32 ; x++)
        {
            if(dump_scr[x+y*32]&0x00ffffff)
                printf("1 ");
            else
                printf("0 ");
        }
        printf("\r\n");
    }
    printf("gram end\n");
}
#endif

static void disp_flush(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p)
{
    ptx_hal_screen_flush(area->x1, area->y1, area->x2, area->y2, color_p);
    lv_disp_flush_ready(disp);
}

void lvgl_core_init(unsigned int w, unsigned int h)
{
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, color_buf, NULL, 32*8);
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = 32;
    disp_drv.ver_res = 8;
    lv_disp_set_rotation(lv_disp_drv_register(&disp_drv), 0);
}
