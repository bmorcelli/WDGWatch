#pragma once
#include <lvgl.h>

#define APP_GREEN lv_color_hex(0x00E5FF)
#define APP_DIM   lv_color_hex(0x007280)
#define APP_BG    lv_color_hex(0x000000)

static inline void app_add_back_button(lv_obj_t *scr) {
    (void)scr;
}
