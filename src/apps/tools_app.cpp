#include "tools_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"

static lv_obj_t *scr = nullptr;

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

#define SA_X      SAFE_LEFT
#define SA_Y      SAFE_TOP
#define SA_W      (SCREEN_WIDTH  - SAFE_LEFT - SAFE_RIGHT)
#define SA_H      (SCREEN_HEIGHT - SAFE_TOP  - SAFE_BOTTOM)

static lv_obj_t  *lbl_stopwatch   = nullptr;
static uint32_t   sw_start        = 0;
static uint32_t   sw_elapsed      = 0;
static bool       sw_running      = false;
static lv_timer_t *sw_timer       = nullptr;

static lv_obj_t  *lbl_timer       = nullptr;
static lv_obj_t  *lbl_timer_status = nullptr;
static lv_obj_t  *btn_timer_start = nullptr;
static lv_obj_t  *btn_timer_reset = nullptr;
static lv_timer_t *ct_timer       = nullptr;
static uint32_t   ct_duration_ms  = 60000;
static uint32_t   ct_start        = 0;
static bool       ct_running      = false;
static bool       ct_finished     = false;
static bool       flashlight_on   = false;

static const uint32_t ct_presets[] = { 1, 3, 5, 10, 15 };
#define CT_PRESET_COUNT 5
static int ct_preset_idx = 0;

static lv_obj_t* make_btn(lv_obj_t *par, int w, int h,
                           const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(par);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, G, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
    return btn;
}

static lv_obj_t* make_section_label(lv_obj_t *par, const char *txt) {
    lv_obj_t *l = lv_label_create(par);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, D, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    return l;
}

static void format_mmss(char *buf, size_t sz, uint32_t total_ms) {
    uint32_t secs = total_ms / 1000;
    uint32_t m = secs / 60;
    uint32_t s = secs % 60;
    snprintf(buf, sz, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

static void flashlight_cb(lv_event_t *e) {
    (void)e;
    flashlight_on = !flashlight_on;
    if (flashlight_on) {
        instance.setBrightness(PIPBOY_MAX_BRIGHTNESS);
        lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    } else {
        instance.setBrightness(PIPBOY_DEFAULT_BRIGHTNESS);
        lv_obj_set_style_bg_color(scr, BG, 0);
    }
}

static void sw_update_label(void) {
    if (!lbl_stopwatch) return;
    uint32_t elapsed = sw_elapsed;
    if (sw_running) elapsed += millis() - sw_start;
    uint32_t mins = elapsed / 60000;
    uint32_t secs = (elapsed / 1000) % 60;
    uint32_t cs   = (elapsed / 10) % 100;
    char b[16];
    snprintf(b, sizeof(b), "%02lu:%02lu.%02lu",
             (unsigned long)mins, (unsigned long)secs, (unsigned long)cs);
    lv_label_set_text(lbl_stopwatch, b);
}

static void sw_tick(lv_timer_t *t) {
    (void)t;
    if (sw_running) sw_update_label();
}

static void stopwatch_cb(lv_event_t *e) {
    (void)e;
    if (sw_running) {
        sw_elapsed += millis() - sw_start;
        sw_running = false;
    } else {
        sw_start = millis();
        sw_running = true;
    }
}

static void stopwatch_reset_cb(lv_event_t *e) {
    (void)e;
    sw_running = false;
    sw_start   = 0;
    sw_elapsed = 0;
    if (lbl_stopwatch) lv_label_set_text(lbl_stopwatch, "00:00.00");
}

static void ct_update_preset_label(void) {
    if (!lbl_timer) return;
    char b[16];
    format_mmss(b, sizeof(b), ct_presets[ct_preset_idx] * 60000UL);
    lv_label_set_text(lbl_timer, b);
}

static void ct_update_running_label(void) {
    if (!lbl_timer) return;
    uint32_t elapsed = millis() - ct_start;
    if (elapsed >= ct_duration_ms) {
        lv_label_set_text(lbl_timer, "00:00");
        return;
    }
    uint32_t remain = ct_duration_ms - elapsed;
    char b[16];
    format_mmss(b, sizeof(b), remain);
    lv_label_set_text(lbl_timer, b);
}

static void ct_tick(lv_timer_t *t) {
    (void)t;
    if (!ct_running) return;
    uint32_t elapsed = millis() - ct_start;
    if (elapsed >= ct_duration_ms) {
        ct_running  = false;
        ct_finished = true;
        lv_label_set_text(lbl_timer, "00:00");
        if (lbl_timer_status) lv_label_set_text(lbl_timer_status, "DONE!");
        haptic_alarm();
    } else {
        ct_update_running_label();
    }
}

static void ct_preset_cb(lv_event_t *e) {
    (void)e;
    if (ct_running) return;
    ct_preset_idx = (ct_preset_idx + 1) % CT_PRESET_COUNT;
    ct_duration_ms = ct_presets[ct_preset_idx] * 60000UL;
    ct_finished = false;
    ct_update_preset_label();
    if (lbl_timer_status) lv_label_set_text(lbl_timer_status, "");
}

static void ct_start_cb(lv_event_t *e) {
    (void)e;
    if (ct_running) {

        ct_running = false;
        if (lbl_timer_status) lv_label_set_text(lbl_timer_status, "PAUSED");
    } else if (!ct_finished) {
        ct_start   = millis();
        ct_running = true;
        if (lbl_timer_status) lv_label_set_text(lbl_timer_status, "RUNNING");
    }
}

static void ct_reset_cb(lv_event_t *e) {
    (void)e;
    ct_running  = false;
    ct_finished = false;
    ct_update_preset_label();
    if (lbl_timer_status) lv_label_set_text(lbl_timer_status, "");
}

void tools_app_create(lv_obj_t *parent) {

    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_pos(scr, SA_X, SA_Y);
    lv_obj_set_size(scr, SA_W, SA_H);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_row(scr, 0, 0);
    lv_obj_set_style_pad_column(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scr, LV_DIR_VER);

    const int CW = SA_W;

    int y = 0;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ TOOLS ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(title, CW / 2 - 50, y);
    y += 30;

    lv_obj_t *fl_lbl = make_section_label(scr, "FLASHLIGHT");
    lv_obj_set_pos(fl_lbl, 0, y);
    y += 20;

    lv_obj_t *fl_btn = make_btn(scr, CW / 2, 44,
                                LV_SYMBOL_EYE_OPEN "  TOGGLE", flashlight_cb);
    lv_obj_set_pos(fl_btn, CW / 4 - CW / 8, y);
    y += 54;

    lv_obj_t *sw_sec = make_section_label(scr, "STOPWATCH");
    lv_obj_set_pos(sw_sec, 0, y);
    y += 22;

    lbl_stopwatch = lv_label_create(scr);
    lv_label_set_text(lbl_stopwatch, "00:00.00");
    lv_obj_set_style_text_color(lbl_stopwatch, G, 0);
    lv_obj_set_style_text_font(lbl_stopwatch, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(lbl_stopwatch, 0, y);
    y += 56;

    lv_obj_t *sw_btn1 = make_btn(scr, 140, 40, "START / STOP", stopwatch_cb);
    lv_obj_set_pos(sw_btn1, 0, y);
    lv_obj_t *sw_btn2 = make_btn(scr, 100, 40, "RESET", stopwatch_reset_cb);
    lv_obj_set_pos(sw_btn2, 150, y);
    y += 52;

    lv_obj_t *ct_sec = make_section_label(scr, "COUNTDOWN TIMER");
    lv_obj_set_pos(ct_sec, 0, y);
    y += 22;

    lv_obj_t *preset_btn = make_btn(scr, 120, 40, "SET TIME", ct_preset_cb);
    lv_obj_set_pos(preset_btn, 0, y);
    y += 48;

    lbl_timer = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_timer, G, 0);
    lv_obj_set_style_text_font(lbl_timer, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(lbl_timer, 0, y);
    ct_update_preset_label();
    y += 56;

    lbl_timer_status = lv_label_create(scr);
    lv_label_set_text(lbl_timer_status, "");
    lv_obj_set_style_text_color(lbl_timer_status, D, 0);
    lv_obj_set_style_text_font(lbl_timer_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_timer_status, 0, y);
    y += 22;

    btn_timer_start = make_btn(scr, 140, 40, "START / STOP", ct_start_cb);
    lv_obj_set_pos(btn_timer_start, 0, y);
    btn_timer_reset = make_btn(scr, 100, 40, "RESET", ct_reset_cb);
    lv_obj_set_pos(btn_timer_reset, 150, y);
    y += 50;

    sw_timer = lv_timer_create(sw_tick, 50, nullptr);
    ct_timer = lv_timer_create(ct_tick, 250, nullptr);
}

void tools_app_destroy(void) {

    if (flashlight_on) {
        instance.setBrightness(PIPBOY_DEFAULT_BRIGHTNESS);
        flashlight_on = false;
    }
    sw_running  = false;
    sw_elapsed  = 0;
    ct_running  = false;
    ct_finished = false;
    if (sw_timer) { lv_timer_delete(sw_timer); sw_timer = nullptr; }
    if (ct_timer) { lv_timer_delete(ct_timer); ct_timer = nullptr; }
    lbl_stopwatch    = nullptr;
    lbl_timer        = nullptr;
    lbl_timer_status = nullptr;
    btn_timer_start  = nullptr;
    btn_timer_reset  = nullptr;
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
}
