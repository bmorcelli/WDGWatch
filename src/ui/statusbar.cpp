#include "statusbar.h"
#include <cstdio>

static lv_obj_t *bar_container = nullptr;
static lv_obj_t *lbl_bat = nullptr;
static lv_obj_t *lbl_ble = nullptr;
static lv_obj_t *lbl_wifi = nullptr;
static lv_obj_t *lbl_gps = nullptr;
static lv_obj_t *lbl_lora = nullptr;

void statusbar_create(lv_obj_t *parent) {
    bar_container = lv_obj_create(parent);
    lv_obj_remove_style_all(bar_container);
    lv_obj_set_size(bar_container, SCREEN_WIDTH, 20);
    lv_obj_set_pos(bar_container, 0, 0);
    lv_obj_set_style_bg_color(bar_container, PIPBOY_BG_16, 0);
    lv_obj_set_style_bg_opa(bar_container, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(bar_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(bar_container, 10, 0);
    lv_obj_set_style_pad_gap(bar_container, 6, 0);

    lbl_lora = lv_label_create(bar_container);
    lv_label_set_text(lbl_lora, "LR");
    lv_obj_set_style_text_color(lbl_lora, PIPBOY_GREEN_DIM_16, 0);
    lv_obj_set_style_text_font(lbl_lora, &lv_font_montserrat_14, 0);

    lbl_gps = lv_label_create(bar_container);
    lv_label_set_text(lbl_gps, LV_SYMBOL_GPS);
    lv_obj_set_style_text_color(lbl_gps, PIPBOY_GREEN_DIM_16, 0);
    lv_obj_set_style_text_font(lbl_gps, &lv_font_montserrat_14, 0);

    lbl_wifi = lv_label_create(bar_container);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi, PIPBOY_GREEN_DIM_16, 0);
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_14, 0);

    lbl_ble = lv_label_create(bar_container);
    lv_label_set_text(lbl_ble, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(lbl_ble, PIPBOY_GREEN_DIM_16, 0);
    lv_obj_set_style_text_font(lbl_ble, &lv_font_montserrat_14, 0);

    lbl_bat = lv_label_create(bar_container);
    lv_label_set_text(lbl_bat, LV_SYMBOL_BATTERY_FULL " 100%");
    lv_obj_set_style_text_color(lbl_bat, PIPBOY_GREEN_16, 0);
    lv_obj_set_style_text_font(lbl_bat, &lv_font_montserrat_14, 0);
}

void statusbar_update_battery(uint8_t percent, bool charging) {
    if (!lbl_bat) return;
    char buf[20];
    const char *icon = LV_SYMBOL_BATTERY_FULL;
    if (percent < 15) icon = LV_SYMBOL_BATTERY_EMPTY;
    else if (percent < 40) icon = LV_SYMBOL_BATTERY_1;
    else if (percent < 60) icon = LV_SYMBOL_BATTERY_2;
    else if (percent < 80) icon = LV_SYMBOL_BATTERY_3;

    if (charging) {
        snprintf(buf, sizeof(buf), "%s" LV_SYMBOL_CHARGE "%d%%", icon, percent);
    } else {
        snprintf(buf, sizeof(buf), "%s %d%%", icon, percent);
    }
    lv_label_set_text(lbl_bat, buf);
}

void statusbar_update_ble(bool connected) {
    if (!lbl_ble) return;
    lv_obj_set_style_text_color(lbl_ble, connected ? PIPBOY_GREEN_16 : PIPBOY_GREEN_DIM_16, 0);
}

void statusbar_update_wifi(bool connected) {
    if (!lbl_wifi) return;
    lv_obj_set_style_text_color(lbl_wifi, connected ? PIPBOY_GREEN_16 : PIPBOY_GREEN_DIM_16, 0);
}

void statusbar_update_gps(bool fix) {
    if (!lbl_gps) return;
    lv_obj_set_style_text_color(lbl_gps, fix ? PIPBOY_GREEN_16 : PIPBOY_GREEN_DIM_16, 0);
}

void statusbar_update_lora(bool active) {
    if (!lbl_lora) return;
    lv_obj_set_style_text_color(lbl_lora, active ? PIPBOY_GREEN_16 : PIPBOY_GREEN_DIM_16, 0);
}
