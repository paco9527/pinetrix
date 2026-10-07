#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "log.h"
#include "lv_adapter.h"
#include "render.h"
#include "hal/hal_screen.h"

RENDER get_render_instance(void)
{
    static RENDER_HDL render_inst = {0};
    return (RENDER)(&render_inst);
}

lv_obj_t* render_get_act_scr(void)
{
    RENDER_HDL* r = (RENDER_HDL*)get_render_instance();
    return r->root;
}

RENDER render_init(uint16_t w, uint16_t h)
{
    RENDER_HDL* render_hdl = (RENDER_HDL*)get_render_instance();
    
    if (ptx_hal_screen_init(w, h) != 0) {
        LOG_ERROR("[render] ptx_hal_screen_init failed, aborting");
        exit(1);
    }
    lvgl_core_init(w, h);
    render_hdl->root = lv_scr_act();
    
    return render_hdl;
}

void render_deinit(RENDER hdl)
{
    (void)hdl;
    ptx_hal_screen_deinit();
    LOG_DEBUG("ptx_hal_screen destruct completed\n");
}

int render_hardware_setting(int key, void* value, size_t value_len)
{
    int ret = 0;
    switch(key)
    {
        case HW_BRIGHTNESS:
        {
            ptx_hal_screen_set_brightness(*(uint8_t*)value);
        }
    }
    return ret;
}
