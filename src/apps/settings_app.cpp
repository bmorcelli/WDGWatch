#include "settings_app.h"
#include <WiFi.h>
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/sound_settings.h"
#include "../hal/time_sync.h"
#include "../hal/power_hal.h"
#include "app_common.h"
#include "../ui/theme.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *info_label = nullptr;
static lv_timer_t *refresh_timer = nullptr;
static uint32_t last_brightness_change = 0;
static uint32_t boot_time_ms = 0;

#define G   lv_color_hex(PIPBOY_GREEN)
#define D   lv_color_hex(PIPBOY_GREEN_DIM)
#define DK  lv_color_hex(PIPBOY_GREEN_DARK)
#define BG  lv_color_hex(PIPBOY_BG)

#define CX      SAFE_LEFT
#define CY      SAFE_TOP
#define CW      (SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT)
#define CH      (SCREEN_HEIGHT - SAFE_TOP - SAFE_BOTTOM)

static void brightness_cb(lv_event_t *e) {
    uint32_t now = millis();
    if (now - last_brightness_change < 50) return;
    last_brightness_change = now;

    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    instance.setBrightness(val);
}

static void volume_cb(lv_event_t *e) {
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    sound_set_volume(val);
}

static void haptic_toggle_cb(lv_event_t *e) {
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    haptic_set_enabled(on);
    if (on) haptic_click();
}

static void time_adj_btn_cb(lv_event_t *e) {
    int adj = (int)(intptr_t)lv_event_get_user_data(e);
    haptic_click();
    time_sync_adjust_offset(adj);

    lv_obj_t *parent = lv_obj_get_parent(scr);
    settings_app_destroy();
    settings_app_create(parent);
}

static void color_btn_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    haptic_click();
    theme_color_set_idx(idx);

    lv_obj_t *parent = lv_obj_get_parent(scr);
    settings_app_destroy();
    settings_app_create(parent);
}

static void format_uptime(uint32_t ms, char *buf, size_t len) {
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;
    secs %= 60;
    mins %= 60;
    snprintf(buf, len, "%luh %lum %lus", (unsigned long)hrs,
             (unsigned long)mins, (unsigned long)secs);
}

static void refresh_info_cb(lv_timer_t *t) {
    (void)t;
    if (!info_label) return;

    char uptime_str[32];
    format_uptime(millis() - boot_time_ms, uptime_str, sizeof(uptime_str));

    float batt_v = power_hal_battery_voltage();
    int batt_pct = power_hal_battery_percent();

    char info[320];
    snprintf(info, sizeof(info),
        "Heap: %d KB free\n"
        "PSRAM: %d / %d KB\n"
        "Battery: %.2fV (%d%%)\n"
        "Uptime: %s",
        ESP.getFreeHeap() / 1024,
        ESP.getFreePsram() / 1024,
        ESP.getPsramSize() / 1024,
        batt_v, batt_pct,
        uptime_str
    );
    lv_label_set_text(info_label, info);
}

static lv_obj_t *make_section_label(lv_obj_t *parent, const char *text,
                                     lv_coord_t x, lv_coord_t y) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, D, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

static lv_obj_t *make_body_label(lv_obj_t *parent, const char *text,
                                  lv_coord_t x, lv_coord_t y, lv_coord_t w) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, G, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_width(lbl, w);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    return lbl;
}

void settings_app_create(lv_obj_t *parent) {
    if (boot_time_ms == 0) boot_time_ms = millis();

    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, CW, CH);
    lv_obj_set_pos(cont, CX, CY);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 4, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "[ SETTINGS ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_width(title, CW);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_bottom(title, 6, 0);

    make_section_label(cont, "BRIGHTNESS", 0, 0);

    lv_obj_t *slider = lv_slider_create(cont);
    lv_obj_set_size(slider, CW - 10, 22);
    lv_slider_set_range(slider, 10, 255);
    lv_slider_set_value(slider, PIPBOY_DEFAULT_BRIGHTNESS, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, DK, 0);
    lv_obj_set_style_bg_color(slider, G, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, G, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 11, 0);
    lv_obj_set_style_radius(slider, 11, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_style_pad_bottom(slider, 6, 0);

    make_section_label(cont, "VOLUME", 0, 0);

    lv_obj_t *vol_slider = lv_slider_create(cont);
    lv_obj_set_size(vol_slider, CW - 10, 22);
    lv_slider_set_range(vol_slider, 0, 100);
    lv_slider_set_value(vol_slider, sound_get_volume(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(vol_slider, DK, 0);
    lv_obj_set_style_bg_color(vol_slider, G, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(vol_slider, G, LV_PART_KNOB);
    lv_obj_set_style_radius(vol_slider, 11, 0);
    lv_obj_set_style_radius(vol_slider, 11, LV_PART_INDICATOR);
    lv_obj_set_style_radius(vol_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(vol_slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(vol_slider, volume_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_style_pad_bottom(vol_slider, 6, 0);

    lv_obj_t *haptic_row = lv_obj_create(cont);
    lv_obj_remove_style_all(haptic_row);
    lv_obj_set_size(haptic_row, CW, 30);
    lv_obj_set_style_bg_opa(haptic_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_bottom(haptic_row, 6, 0);

    lv_obj_t *haptic_lbl = lv_label_create(haptic_row);
    lv_label_set_text(haptic_lbl, "HAPTIC FEEDBACK");
    lv_obj_set_style_text_color(haptic_lbl, D, 0);
    lv_obj_set_style_text_font(haptic_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(haptic_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(haptic_row);
    lv_obj_set_size(sw, 50, 24);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_set_style_bg_color(sw, DK, 0);
    lv_obj_set_style_bg_color(sw, G, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, DK, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, G, LV_PART_KNOB);
    lv_obj_set_style_radius(sw, 12, 0);
    lv_obj_set_style_radius(sw, 12, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    if (haptic_is_enabled()) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, haptic_toggle_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    make_section_label(cont, "THEME COLOR", 0, 0);

    lv_obj_t *color_row = lv_obj_create(cont);
    lv_obj_remove_style_all(color_row);
    lv_obj_set_size(color_row, CW, 36);
    lv_obj_set_style_bg_opa(color_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(color_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(color_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(color_row, 6, 0);

    static const uint32_t colors[] = {0x00E5FF, 0x00FF66, 0xFF9F00, 0xFF3333, 0x3399FF};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *btn = lv_button_create(color_row);
        lv_obj_set_size(btn, 32, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        if (theme_color_get_idx() == i) {
            lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFFFF), 0);
            lv_obj_set_style_border_width(btn, 2, 0);
        } else {
            lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
        }
        lv_obj_add_event_cb(btn, color_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    make_section_label(cont, "TIME CALIBRATE", 0, 0);

    lv_obj_t *time_row = lv_obj_create(cont);
    lv_obj_remove_style_all(time_row);
    lv_obj_set_size(time_row, CW, 45);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(time_row, 6, 0);

    lv_obj_t *btn_minus = lv_button_create(time_row);
    lv_obj_set_size(btn_minus, 85, 32);
    lv_obj_set_style_bg_color(btn_minus, DK, 0);
    lv_obj_set_style_border_color(btn_minus, G, 0);
    lv_obj_set_style_border_width(btn_minus, 1, 0);
    lv_obj_set_style_radius(btn_minus, 6, 0);
    lv_obj_add_event_cb(btn_minus, time_adj_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);
    lv_obj_t *lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "-1 HOUR");
    lv_obj_set_style_text_color(lbl_minus, G, 0);
    lv_obj_set_style_text_font(lbl_minus, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_minus);

    lv_obj_t *lbl_tz = lv_label_create(time_row);
    String current_tz = time_sync_get_timezone();
    lv_label_set_text(lbl_tz, current_tz.c_str());
    lv_obj_set_style_text_color(lbl_tz, G, 0);
    lv_obj_set_style_text_font(lbl_tz, &lv_font_montserrat_16, 0);

    lv_obj_t *btn_plus = lv_button_create(time_row);
    lv_obj_set_size(btn_plus, 85, 32);
    lv_obj_set_style_bg_color(btn_plus, DK, 0);
    lv_obj_set_style_border_color(btn_plus, G, 0);
    lv_obj_set_style_border_width(btn_plus, 1, 0);
    lv_obj_set_style_radius(btn_plus, 6, 0);
    lv_obj_add_event_cb(btn_plus, time_adj_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    lv_obj_t *lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, "+1 HOUR");
    lv_obj_set_style_text_color(lbl_plus, G, 0);
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_plus);

    make_section_label(cont, "SYSTEM INFO", 0, 0);

    info_label = make_body_label(cont, "", 0, 0, CW);

    refresh_info_cb(nullptr);
    refresh_timer = lv_timer_create(refresh_info_cb, 2000, nullptr);
}

void settings_app_destroy(void) {
    if (refresh_timer) {
        lv_timer_delete(refresh_timer);
        refresh_timer = nullptr;
    }
    info_label = nullptr;
    if (scr) {
        lv_obj_delete(scr);
        scr = nullptr;
    }
}
