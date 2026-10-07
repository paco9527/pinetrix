#ifndef _RENDER
#define _RENDER

#include "ptx_config.h"
#include "lvgl.h"

typedef struct _RENDER_HDL
{
    lv_obj_t* root;
}RENDER_HDL;

typedef void* RENDER;

typedef enum
{
    HW_BRIGHTNESS,
}RENDER_PARAM;

RENDER render_init(uint16_t w, uint16_t h);
void render_deinit(RENDER hdl);
int render_hardware_setting(int key, void* value, size_t value_len);

RENDER get_render_instance(void);

lv_obj_t* render_get_act_scr(void);

#endif
