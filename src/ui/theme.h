#pragma once
#include <lvgl.h>
#include "../config.h"

extern char theme_color_recolor_str[10];
extern char theme_color_dim_recolor_str[10];

void theme_color_init(void);
void theme_color_set_idx(uint8_t idx);
uint8_t theme_color_get_idx(void);

void pipboy_theme_init(void);
void pipboy_theme_reinit(void);
lv_style_t* pipboy_style_default(void);
lv_style_t* pipboy_style_title(void);
lv_style_t* pipboy_style_panel(void);
lv_style_t* pipboy_style_btn(void);
