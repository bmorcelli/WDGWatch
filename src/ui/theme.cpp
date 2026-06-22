#include "theme.h"
#include <Preferences.h>
#include <cstdio>
#include "../app_manager.h"

uint32_t current_primary_hex = 0x00E5FF;
uint32_t current_dim_hex = 0x007280;
uint32_t current_dark_hex = 0x003840;

char theme_color_recolor_str[10] = "00E5FF";
char theme_color_dim_recolor_str[10] = "007280";

static const uint32_t theme_primaries[] = {0x00E5FF, 0x00FF66, 0xFF9F00, 0xFF3333, 0x3399FF};
static const uint32_t theme_dims[]      = {0x007280, 0x008033, 0x805000, 0x801A1A, 0x1A4D80};
static const uint32_t theme_darks[]     = {0x003840, 0x00401A, 0x402800, 0x400D0D, 0x0D2640};

static uint8_t current_color_idx = 0;

static lv_style_t style_default;
static lv_style_t style_title;
static lv_style_t style_panel;
static lv_style_t style_btn;
static bool initialized = false;

static void update_recolor_strings() {
    snprintf(theme_color_recolor_str, sizeof(theme_color_recolor_str), "%06X", current_primary_hex & 0xFFFFFF);
    snprintf(theme_color_dim_recolor_str, sizeof(theme_color_dim_recolor_str), "%06X", current_dim_hex & 0xFFFFFF);
}

void theme_color_init(void) {
    Preferences prefs;
    prefs.begin("theme", true);
    current_color_idx = prefs.getUChar("color_idx", 0);
    prefs.end();

    if (current_color_idx >= 5) current_color_idx = 0;

    current_primary_hex = theme_primaries[current_color_idx];
    current_dim_hex = theme_dims[current_color_idx];
    current_dark_hex = theme_darks[current_color_idx];
    
    update_recolor_strings();
}

void theme_color_set_idx(uint8_t idx) {
    if (idx >= 5) return;
    current_color_idx = idx;

    current_primary_hex = theme_primaries[idx];
    current_dim_hex = theme_dims[idx];
    current_dark_hex = theme_darks[idx];
    
    update_recolor_strings();

    Preferences prefs;
    prefs.begin("theme", false);
    prefs.putUChar("color_idx", idx);
    prefs.end();

    pipboy_theme_reinit();
    app_manager_theme_changed();
}

uint8_t theme_color_get_idx(void) {
    return current_color_idx;
}

void pipboy_theme_init(void) {
    if (initialized) return;
    initialized = true;

    theme_color_init();

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

void pipboy_theme_reinit(void) {
    lv_style_set_bg_color(&style_default, PIPBOY_BG_16);
    lv_style_set_text_color(&style_default, PIPBOY_GREEN_16);
    lv_style_set_border_color(&style_default, PIPBOY_GREEN_DIM_16);
    lv_style_set_line_color(&style_default, PIPBOY_GREEN_16);

    lv_style_set_text_color(&style_title, PIPBOY_GREEN_16);

    lv_style_set_bg_color(&style_panel, PIPBOY_BG_16);
    lv_style_set_border_color(&style_panel, PIPBOY_GREEN_DIM_16);

    lv_style_set_bg_color(&style_btn, PIPBOY_BG_16);
    lv_style_set_border_color(&style_btn, PIPBOY_GREEN_16);
    lv_style_set_text_color(&style_btn, PIPBOY_GREEN_16);
    
    lv_obj_report_style_change(NULL);
}

lv_style_t* pipboy_style_default(void) { return &style_default; }
lv_style_t* pipboy_style_title(void)   { return &style_title; }
lv_style_t* pipboy_style_panel(void)   { return &style_panel; }
lv_style_t* pipboy_style_btn(void)     { return &style_btn; }
