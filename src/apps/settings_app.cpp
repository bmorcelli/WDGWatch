#include "settings_app.h"
#include <WiFi.h>
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/time_sync.h"
#include "../hal/power_hal.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *info_label = nullptr;
static lv_obj_t *wifi_label = nullptr;
static lv_obj_t *ntp_status_label = nullptr;
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

static void haptic_toggle_cb(lv_event_t *e) {
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    haptic_set_enabled(on);
    if (on) haptic_click();
}

static void ntp_sync_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    time_sync_force_retry();
    if (ntp_status_label) {
        lv_label_set_text(ntp_status_label, "NTP SYNC REQUESTED...");
    }
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

    if (wifi_label) {
        if (time_sync_wifi_connected()) {
            char wbuf[64];
            snprintf(wbuf, sizeof(wbuf), "Connected: %s", WiFi.SSID().c_str());
            lv_label_set_text(wifi_label, wbuf);
        } else {
            lv_label_set_text(wifi_label, "Not connected");
        }
    }

    if (ntp_status_label) {
        lv_label_set_text(ntp_status_label,
            time_sync_is_synced() ? "NTP: SYNCED" : "NTP: NOT SYNCED");
    }
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
    lv_obj_set_style_radius(slider, 0, 0);
    lv_obj_set_style_radius(slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_style_pad_bottom(slider, 6, 0);

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
    lv_obj_set_style_radius(sw, 0, 0);
    lv_obj_set_style_radius(sw, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sw, 0, LV_PART_KNOB);
    if (haptic_is_enabled()) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, haptic_toggle_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    make_section_label(cont, "WIFI", 0, 0);

    wifi_label = make_body_label(cont,
        time_sync_wifi_connected() ? WiFi.SSID().c_str() : "Not connected",
        0, 0, CW);
    lv_obj_set_style_pad_bottom(wifi_label, 6, 0);

    lv_obj_t *ntp_row = lv_obj_create(cont);
    lv_obj_remove_style_all(ntp_row);
    lv_obj_set_size(ntp_row, CW, 34);
    lv_obj_set_style_bg_opa(ntp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_bottom(ntp_row, 6, 0);

    ntp_status_label = lv_label_create(ntp_row);
    lv_label_set_text(ntp_status_label,
        time_sync_is_synced() ? "NTP: SYNCED" : "NTP: NOT SYNCED");
    lv_obj_set_style_text_color(ntp_status_label, D, 0);
    lv_obj_set_style_text_font(ntp_status_label, &lv_font_montserrat_18, 0);
    lv_obj_align(ntp_status_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *ntp_btn = lv_btn_create(ntp_row);
    lv_obj_set_size(ntp_btn, 120, 28);
    lv_obj_align(ntp_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(ntp_btn, DK, 0);
    lv_obj_set_style_bg_color(ntp_btn, G, LV_STATE_PRESSED);
    lv_obj_set_style_radius(ntp_btn, 0, 0);
    lv_obj_set_style_border_color(ntp_btn, G, 0);
    lv_obj_set_style_border_width(ntp_btn, 1, 0);
    lv_obj_add_event_cb(ntp_btn, ntp_sync_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *ntp_btn_lbl = lv_label_create(ntp_btn);
    lv_label_set_text(ntp_btn_lbl, "FORCE SYNC");
    lv_obj_set_style_text_color(ntp_btn_lbl, G, 0);
    lv_obj_set_style_text_font(ntp_btn_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(ntp_btn_lbl);

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
    wifi_label = nullptr;
    ntp_status_label = nullptr;
    if (scr) {
        lv_obj_delete(scr);
        scr = nullptr;
    }
}
