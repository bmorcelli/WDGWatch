#include "theme.h"

static lv_style_t style_default;
static lv_style_t style_title;
static lv_style_t style_panel;
static lv_style_t style_btn;
static bool initialized = false;

void pipboy_theme_init(void) {
    if (initialized) return;
    initialized = true;

    lv_style_init(&style_default);
    lv_style_set_bg_color(&style_default, PIPBOY_BG_16);
    lv_style_set_text_color(&style_default, PIPBOY_GREEN_16);
    lv_style_set_border_color(&style_default, PIPBOY_GREEN_DIM_16);
    lv_style_set_line_color(&style_default, PIPBOY_GREEN_16);

    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, PIPBOY_GREEN_16);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_24);

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, PIPBOY_BG_16);
    lv_style_set_bg_opa(&style_panel, LV_OPA_COVER);
    lv_style_set_border_color(&style_panel, PIPBOY_GREEN_DIM_16);
    lv_style_set_border_width(&style_panel, 1);
    lv_style_set_radius(&style_panel, 2);
    lv_style_set_pad_all(&style_panel, 4);

    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, PIPBOY_BG_16);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_border_color(&style_btn, PIPBOY_GREEN_16);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_text_color(&style_btn, PIPBOY_GREEN_16);
    lv_style_set_radius(&style_btn, 3);
    lv_style_set_pad_ver(&style_btn, 4);
    lv_style_set_pad_hor(&style_btn, 8);
}

lv_style_t* pipboy_style_default(void) { return &style_default; }
lv_style_t* pipboy_style_title(void)   { return &style_title; }
lv_style_t* pipboy_style_panel(void)   { return &style_panel; }
lv_style_t* pipboy_style_btn(void)     { return &style_btn; }
