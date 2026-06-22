#pragma once
#include <lvgl.h>

#include "../config.h"

#define APP_GREEN lv_color_hex(PIPBOY_GREEN)
#define APP_DIM   lv_color_hex(PIPBOY_GREEN_DIM)
#define APP_BG    lv_color_hex(PIPBOY_BG)

static inline void app_back_click_cb(lv_event_t *e) {
    extern void app_manager_back(void);
    app_manager_back();
}

static inline void app_add_back_button(lv_obj_t *scr) {
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 50, 35);
    lv_obj_set_pos(btn, 10, 10);
    lv_obj_set_style_bg_color(btn, lv_color_hex(PIPBOY_BG), 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(PIPBOY_GREEN_DIM), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 17, 0);
    lv_obj_add_event_cb(btn, app_back_click_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(PIPBOY_GREEN), 0);
    lv_obj_center(lbl);
}

static inline void style_button_by_position(lv_obj_t *btn, int y, int h) {
    (void)y; (void)h;
    lv_obj_set_style_radius(btn, 8, 0);
}
