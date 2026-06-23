#include "watchface.h"
#include "theme.h"
#define RC_G theme_color_recolor_str
#define RC_D theme_color_dim_recolor_str
#include <cstdio>
#include <math.h>
#include "../apps/gps_app.h"
#include "../web/web_server.h"
#include "../hal/ble_uart_service.h"
#include "../hal/lora_service.h"
#include "../hal/recon_service.h"
#include <LilyGoLib.h>
#include "../hal/haptic.h"
#include "../hal/power_hal.h"
#include "../app_manager.h"
#include "../hal/audio_record.h"
#include "../hal/sound_settings.h"
#include "../apps/app_common.h"

#define LV_SYMBOL_SMILE "\xEF\x84\x98"
#ifdef LV_SYMBOL_BELL
#undef LV_SYMBOL_BELL
#endif
#define LV_SYMBOL_BELL "\xEF\x83\xA3"

#define G  PIPBOY_GREEN_16
#define D  PIPBOY_GREEN_DIM_16
#define DK PIPBOY_DARK_16
#define BG PIPBOY_BG_16
#define X0 SAFE_LEFT
#define Y0 SAFE_TOP
#define XM (SCREEN_WIDTH - SAFE_RIGHT)
#define YM (SCREEN_HEIGHT - SAFE_BOTTOM)
#define SW (XM - X0)
#define CX (SCREEN_WIDTH / 2)
#define CY (SCREEN_HEIGHT / 2)

static lv_obj_t *scr = nullptr;
static lv_obj_t *parent_scr = nullptr;
static WatchfaceStyle current_style = WF_PIPBOY;

static uint8_t c_hour = 0, c_min = 0, c_sec = 0;
static uint8_t c_day = 1, c_month = 1, c_wday = 0;
static uint8_t c_bat = 0;
static bool c_charging = false;
static bool c_ntp = false, c_wifi = false, c_gps = false;
static uint32_t c_steps = 0;

static bool alarm_enabled = false;
static uint8_t alarm_hour = 12;
static uint8_t alarm_min = 0;
static bool alarm_triggered = false;
static bool alarm_active_ringing = false;

static lv_obj_t *lbl_alarm_bell = nullptr;
static lv_obj_t *alarm_win = nullptr;
static lv_obj_t *lbl_alarm_hour = nullptr;
static lv_obj_t *lbl_alarm_min = nullptr;
static lv_obj_t *btn_alarm_toggle = nullptr;
static lv_obj_t *lbl_alarm_toggle = nullptr;

static lv_obj_t *alarm_ring_win = nullptr;
static lv_obj_t *alarm_ring_bell = nullptr;
static lv_obj_t *ring_btns[3] = {nullptr, nullptr, nullptr};
static bool btn_states[3] = {false, false, false};
static lv_timer_t *alarm_timer = nullptr;
static bool bell_flash_state = false;

bool watchface_alarm_is_ringing(void) {
    return alarm_active_ringing;
}

bool watchface_alarm_is_enabled(void) {
    return alarm_enabled;
}

void watchface_alarm_get_time(uint8_t *hour, uint8_t *min) {
    if (hour) *hour = alarm_hour;
    if (min) *min = alarm_min;
}

static void play_alarm_sound(int duration_ms, int freq_hz) {
    instance.powerControl(POWER_SPEAK, true);
    
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
    
    
    instance.player.configureTX(44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    const int sample_rate = 44100;
    #else
    const int sample_rate = 160000;
    #endif

    int total_samples = (sample_rate * duration_ms) / 1000;
    const int chunk_size = 512;
    int16_t *buf = (int16_t*)malloc(chunk_size * 2 * sizeof(int16_t));
    if (!buf) {
        instance.powerControl(POWER_SPEAK, false);
        return;
    }

    
    float f_start = 600.0f;
    float f_end = 1800.0f;
    float T = (float)duration_ms / 1000.0f;
    const float pi = 3.14159265f;

    int sample_idx = 0;
    int samples_written = 0;
    while (samples_written < total_samples) {
        int to_write = total_samples - samples_written;
        if (to_write > chunk_size) to_write = chunk_size;
        float vol_factor = (float)sound_get_volume() / 100.0f;
        for (int i = 0; i < to_write; i++) {
            float t = (float)sample_idx / sample_rate;
            
            float phase = 2.0f * pi * (f_start * t + 0.5f * (f_end - f_start) * t * t / T);
            int16_t val = (int16_t)(sinf(phase) * 25000.0f * vol_factor);
            buf[i * 2]     = val;
            buf[i * 2 + 1] = val;
            sample_idx++;
        }
        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
        instance.player.write(reinterpret_cast<const uint8_t*>(buf), to_write * 2 * sizeof(int16_t));
        #else
        instance.player.write(buf, to_write * 2 * sizeof(int16_t));
        #endif
        samples_written += to_write;
    }
    free(buf);
    instance.powerControl(POWER_SPEAK, false);
}

static void dismiss_alarm_cb(lv_event_t *e) {
    alarm_active_ringing = false;
    if (alarm_ring_win) {
        lv_obj_delete(alarm_ring_win);
        alarm_ring_win = nullptr;
    }
    if (alarm_timer) {
        lv_timer_delete(alarm_timer);
        alarm_timer = nullptr;
    }
    haptic_success();
    app_manager_show(APP_WATCHFACE);
}

static void alarm_timer_cb(lv_timer_t *t) {
    if (!alarm_active_ringing) return;
    haptic_alarm();
    play_alarm_sound(150, 1000);
    
    bell_flash_state = !bell_flash_state;
    if (alarm_ring_bell) {
        lv_obj_set_style_text_color(alarm_ring_bell, bell_flash_state ? G : DK, 0);
    }
}

static void ring_btn_click_cb(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    int idx = -1;
    for (int i = 0; i < 3; i++) {
        if (ring_btns[i] == btn) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return;
    
    btn_states[idx] = !btn_states[idx];
    haptic_click();
    
    if (btn_states[idx]) {
        lv_obj_set_style_bg_color(btn, G, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_color(btn, BG, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    }
    
    if (btn_states[0] && btn_states[1] && btn_states[2]) {
        dismiss_alarm_cb(nullptr);
    }
}

static void trigger_alarm(void) {
    if (power_hal_screen_is_off()) {
        power_hal_screen_toggle();
    }
    
    haptic_alarm();
    alarm_active_ringing = true;
    bell_flash_state = true;
    for (int i = 0; i < 3; i++) btn_states[i] = false;
    
    alarm_ring_win = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(alarm_ring_win);
    lv_obj_set_size(alarm_ring_win, 410, 502);
    lv_obj_set_style_bg_color(alarm_ring_win, BG, 0);
    lv_obj_set_style_bg_opa(alarm_ring_win, LV_OPA_COVER, 0);
    lv_obj_center(alarm_ring_win);
    
    lv_obj_add_flag(alarm_ring_win, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(alarm_ring_win, LV_OBJ_FLAG_SCROLLABLE);
    
    alarm_ring_bell = lv_label_create(alarm_ring_win);
    lv_label_set_text(alarm_ring_bell, "WAKE UP!");
    lv_obj_set_style_text_color(alarm_ring_bell, G, 0);
    lv_obj_set_style_text_font(alarm_ring_bell, &lv_font_montserrat_48, 0);
    lv_obj_align(alarm_ring_bell, LV_ALIGN_CENTER, 0, -80);
    
    lv_obj_t *lbl_title = lv_label_create(alarm_ring_win);
    lv_label_set_text(lbl_title, "DEACTIVATE PROTOCOL");
    lv_obj_set_style_text_color(lbl_title, G, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 10);
    
    int start_x = CX - 85;
    for (int i = 0; i < 3; i++) {
        ring_btns[i] = lv_button_create(alarm_ring_win);
        lv_obj_set_size(ring_btns[i], 50, 50);
        lv_obj_set_pos(ring_btns[i], start_x + (i * 60), 310);
        lv_obj_set_style_bg_color(ring_btns[i], BG, 0);
        lv_obj_set_style_bg_opa(ring_btns[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(ring_btns[i], G, 0);
        lv_obj_set_style_border_width(ring_btns[i], 2, 0);
        style_button_by_position(ring_btns[i], 310, 50);
        lv_obj_add_event_cb(ring_btns[i], ring_btn_click_cb, LV_EVENT_CLICKED, nullptr);
    }
    
    alarm_timer = lv_timer_create(alarm_timer_cb, 1000, nullptr);
}

static void hour_up_cb(lv_event_t *e) {
    alarm_hour = (alarm_hour + 1) % 24;
    char buf[4]; snprintf(buf, sizeof(buf), "%02d", alarm_hour);
    lv_label_set_text(lbl_alarm_hour, buf);
    haptic_click();
}

static void hour_down_cb(lv_event_t *e) {
    alarm_hour = (alarm_hour + 23) % 24;
    char buf[4]; snprintf(buf, sizeof(buf), "%02d", alarm_hour);
    lv_label_set_text(lbl_alarm_hour, buf);
    haptic_click();
}

static void min_up_cb(lv_event_t *e) {
    alarm_min = (alarm_min + 1) % 60;
    char buf[4]; snprintf(buf, sizeof(buf), "%02d", alarm_min);
    lv_label_set_text(lbl_alarm_min, buf);
    haptic_click();
}

static void min_down_cb(lv_event_t *e) {
    alarm_min = (alarm_min + 59) % 60;
    char buf[4]; snprintf(buf, sizeof(buf), "%02d", alarm_min);
    lv_label_set_text(lbl_alarm_min, buf);
    haptic_click();
}

static void update_toggle_button_style(void) {
    if (!btn_alarm_toggle || !lbl_alarm_toggle) return;
    if (alarm_enabled) {
        lv_label_set_text(lbl_alarm_toggle, "ALARM: ON");
        lv_obj_set_style_bg_color(btn_alarm_toggle, G, 0);
        lv_obj_set_style_bg_opa(btn_alarm_toggle, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(lbl_alarm_toggle, BG, 0);
    } else {
        lv_label_set_text(lbl_alarm_toggle, "ALARM: OFF");
        lv_obj_set_style_bg_color(btn_alarm_toggle, BG, 0);
        lv_obj_set_style_bg_opa(btn_alarm_toggle, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(lbl_alarm_toggle, D, 0);
        lv_obj_set_style_border_color(btn_alarm_toggle, D, 0);
    }
}

static void alarm_toggle_cb(lv_event_t *e) {
    alarm_enabled = !alarm_enabled;
    update_toggle_button_style();
    if (lbl_alarm_bell) {
        lv_obj_set_style_text_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
        lv_obj_set_style_border_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
    }
    haptic_click();
}

static void alarm_close_cb(lv_event_t *e) {
    if (alarm_win) {
        lv_obj_delete(alarm_win);
        alarm_win = nullptr;
    }
    haptic_click();
}

static lv_obj_t* make_adjust_btn(lv_obj_t *par, int x, int y, const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(par);
    lv_obj_set_size(btn, 60, 40);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, D, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    style_button_by_position(btn, y, 40);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_center(l);
    return btn;
}

static void alarm_bell_click_cb(lv_event_t *e) {
    haptic_click();
    if (alarm_win) return;
    
    alarm_win = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(alarm_win);
    lv_obj_set_size(alarm_win, 300, 340);
    lv_obj_set_style_bg_color(alarm_win, BG, 0);
    lv_obj_set_style_bg_opa(alarm_win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(alarm_win, G, 0);
    lv_obj_set_style_border_width(alarm_win, 2, 0);
    lv_obj_set_style_radius(alarm_win, 20, 0);
    lv_obj_center(alarm_win);
    
    lv_obj_add_flag(alarm_win, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(alarm_win, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *title = lv_label_create(alarm_win);
    lv_label_set_text(title, "[ ALARM SETTINGS ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    make_adjust_btn(alarm_win, 60, 65, LV_SYMBOL_UP, hour_up_cb);
    
    lbl_alarm_hour = lv_label_create(alarm_win);
    char hbuf[4]; snprintf(hbuf, sizeof(hbuf), "%02d", alarm_hour);
    lv_label_set_text(lbl_alarm_hour, hbuf);
    lv_obj_set_style_text_color(lbl_alarm_hour, G, 0);
    lv_obj_set_style_text_font(lbl_alarm_hour, &lv_font_montserrat_28, 0);
    lv_obj_set_size(lbl_alarm_hour, 60, 40);
    lv_obj_set_style_text_align(lbl_alarm_hour, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_alarm_hour, 60, 115);
    
    make_adjust_btn(alarm_win, 60, 165, LV_SYMBOL_DOWN, hour_down_cb);
    
    lv_obj_t *colon = lv_label_create(alarm_win);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_color(colon, G, 0);
    lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(colon, 145, 115);
    
    make_adjust_btn(alarm_win, 180, 65, LV_SYMBOL_UP, min_up_cb);
    
    lbl_alarm_min = lv_label_create(alarm_win);
    char mbuf[4]; snprintf(mbuf, sizeof(mbuf), "%02d", alarm_min);
    lv_label_set_text(lbl_alarm_min, mbuf);
    lv_obj_set_style_text_color(lbl_alarm_min, G, 0);
    lv_obj_set_style_text_font(lbl_alarm_min, &lv_font_montserrat_28, 0);
    lv_obj_set_size(lbl_alarm_min, 60, 40);
    lv_obj_set_style_text_align(lbl_alarm_min, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_alarm_min, 180, 115);
    
    make_adjust_btn(alarm_win, 180, 165, LV_SYMBOL_DOWN, min_down_cb);
    
    btn_alarm_toggle = lv_button_create(alarm_win);
    lv_obj_set_size(btn_alarm_toggle, 220, 45);
    lv_obj_set_pos(btn_alarm_toggle, 40, 225);
    style_button_by_position(btn_alarm_toggle, 225, 45);
    lv_obj_set_style_border_width(btn_alarm_toggle, 1, 0);
    lv_obj_add_event_cb(btn_alarm_toggle, alarm_toggle_cb, LV_EVENT_CLICKED, nullptr);
    
    lbl_alarm_toggle = lv_label_create(btn_alarm_toggle);
    lv_obj_center(lbl_alarm_toggle);
    lv_obj_set_style_text_font(lbl_alarm_toggle, &lv_font_montserrat_16, 0);
    update_toggle_button_style();
    
    lv_obj_t *btn_close = lv_button_create(alarm_win);
    lv_obj_set_size(btn_close, 220, 40);
    lv_obj_set_pos(btn_close, 40, 280);
    lv_obj_set_style_bg_color(btn_close, BG, 0);
    lv_obj_set_style_border_color(btn_close, G, 0);
    lv_obj_set_style_border_width(btn_close, 1, 0);
    style_button_by_position(btn_close, 280, 40);
    lv_obj_add_event_cb(btn_close, alarm_close_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "SAVE & CLOSE");
    lv_obj_set_style_text_color(lbl_close, G, 0);
    lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_close);
}

static lv_obj_t *lbl_time = nullptr;
static lv_obj_t *lbl_seconds = nullptr;
static lv_obj_t *lbl_temp = nullptr;
static lv_obj_t *lbl_month = nullptr;
static lv_obj_t *lbl_date = nullptr;
static lv_obj_t *lbl_sync = nullptr;
static lv_obj_t *bar_battery = nullptr;
static lv_obj_t *lbl_battery = nullptr;
static lv_obj_t *lbl_steps_v = nullptr;
static lv_obj_t *lbl_gps = nullptr;
static lv_obj_t *day_labels[7] = {};
static lv_obj_t *day_boxes[7] = {};

static lv_obj_t *m_time = nullptr;
static lv_obj_t *m_date = nullptr;
static lv_obj_t *m_bat = nullptr;
static lv_obj_t *m_sync = nullptr;

static lv_obj_t *a_canvas = nullptr;
static lv_obj_t *a_time_lbl = nullptr;
static lv_obj_t *a_date_lbl = nullptr;

static const char *day_names[] = {"MO","TU","WE","TH","FR","SA","SU"};
static const char *month_names[] = {"JAN","FEB","MAR","APR","MAY","JUN",
    "JUL","AUG","SEP","OCT","NOV","DEC"};
static const char *month_full[] = {"JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE",
    "JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER"};

static void create_pipboy(void) {

    lbl_month = lv_label_create(scr);
    lv_label_set_text(lbl_month, "MARCH");
    lv_obj_set_style_text_color(lbl_month, D, 0);
    lv_obj_set_style_text_font(lbl_month, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_month, X0 + 50, Y0);

    lbl_date = lv_label_create(scr);
    lv_label_set_text(lbl_date, "03-26");
    lv_obj_set_style_text_color(lbl_date, D, 0);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_date, XM - 65, Y0);

    int dw = 42, dh = 18, dg = 4;
    int total_w = 7 * dw + 6 * dg;
    int dx0 = X0 + (SW - total_w) / 2;
    for (int i = 0; i < 7; i++) {
        lv_obj_t *b = lv_obj_create(scr); lv_obj_remove_style_all(b);
        lv_obj_set_size(b, dw, dh); lv_obj_set_pos(b, dx0 + i*(dw+dg), Y0+20);
        lv_obj_set_style_border_color(b, D, 0); lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0); lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        day_boxes[i] = b;
        lv_obj_t *lb = lv_label_create(b);
        lv_label_set_text(lb, day_names[i]);
        lv_obj_set_style_text_color(lb, D, 0);
        lv_obj_set_style_text_font(lb, &lv_font_montserrat_14, 0);
        lv_obj_center(lb); day_labels[i] = lb;
    }

    bar_battery = lv_bar_create(scr);
    lv_obj_set_size(bar_battery, 100, 10); lv_obj_set_pos(bar_battery, X0+50, Y0+46);
    lv_bar_set_range(bar_battery, 0, 100); lv_bar_set_value(bar_battery, 80, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_battery, DK, 0); lv_obj_set_style_bg_opa(bar_battery, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar_battery, G, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_battery, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_battery, 0, 0);
    lv_obj_set_style_radius(bar_battery, 0, LV_PART_INDICATOR);

    lbl_battery = lv_label_create(scr);
    lv_label_set_text(lbl_battery, "80%");
    lv_obj_set_style_text_color(lbl_battery, D, 0);
    lv_obj_set_style_text_font(lbl_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_battery, X0 + 155, Y0+44);

    lbl_alarm_bell = lv_label_create(scr);
    lv_label_set_text(lbl_alarm_bell, "ALARM");
    lv_obj_set_style_text_font(lbl_alarm_bell, &lv_font_montserrat_16, 0);
    lv_obj_set_size(lbl_alarm_bell, 85, 30);
    lv_obj_set_style_text_align(lbl_alarm_bell, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_alarm_bell, XM - 120, Y0 + 41);
    lv_obj_add_flag(lbl_alarm_bell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(lbl_alarm_bell, BG, 0);
    lv_obj_set_style_bg_opa(lbl_alarm_bell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
    lv_obj_set_style_border_width(lbl_alarm_bell, 1, 0);
    lv_obj_set_style_radius(lbl_alarm_bell, 10, 0);
    lv_obj_set_style_pad_top(lbl_alarm_bell, 4, 0);
    lv_obj_set_style_text_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
    lv_obj_add_event_cb(lbl_alarm_bell, alarm_bell_click_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *tc = lv_obj_create(scr); lv_obj_remove_style_all(tc);
    lv_obj_set_size(tc, 360, 160); lv_obj_align(tc, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(tc, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(tc, LV_OBJ_FLAG_SCROLLABLE);

    lbl_time = lv_label_create(tc);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_set_style_text_color(lbl_time, G, 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_letter_space(lbl_time, 2, 0);
    lv_obj_center(lbl_time);
    lv_obj_set_style_transform_pivot_x(lbl_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_scale(lbl_time, 640, 0);

    lbl_seconds = lv_label_create(scr);
    lv_label_set_text(lbl_seconds, ":00");
    lv_obj_set_style_text_color(lbl_seconds, D, 0);
    lv_obj_set_style_text_font(lbl_seconds, &lv_font_montserrat_22, 0);
    lv_obj_align(lbl_seconds, LV_ALIGN_CENTER, 70, 55);

    lbl_temp = lv_label_create(scr);
    lv_label_set_text(lbl_temp, "--°C");
    lv_obj_set_style_text_color(lbl_temp, D, 0);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_22, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, -70, 55);

    lbl_steps_v = lv_label_create(scr);
    lv_label_set_text(lbl_steps_v, "STEPS: 0");
    lv_obj_set_style_text_color(lbl_steps_v, D, 0);
    lv_obj_set_style_text_font(lbl_steps_v, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_steps_v, X0+30, YM-80);

    lbl_gps = lv_label_create(scr);
    lv_label_set_text(lbl_gps, LV_SYMBOL_GPS " NO FIX");
    lv_obj_set_style_text_color(lbl_gps, D, 0);
    lv_obj_set_style_text_font(lbl_gps, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_gps, LV_ALIGN_BOTTOM_RIGHT, -SAFE_RIGHT, -80);

    lbl_sync = lv_label_create(scr);
    lv_label_set_recolor(lbl_sync, true);
    lv_label_set_text(lbl_sync, "");
    lv_obj_set_style_text_font(lbl_sync, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_sync, LV_ALIGN_BOTTOM_MID, 0, -(SAFE_BOTTOM+5));
}

static void create_minimal(void) {

    m_time = lv_label_create(scr);
    lv_label_set_text(m_time, "00:00");
    lv_obj_set_style_text_color(m_time, G, 0);
    lv_obj_set_style_text_font(m_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_letter_space(m_time, 4, 0);
    lv_obj_align(m_time, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_transform_pivot_x(m_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(m_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_scale(m_time, 640, 0);

    m_date = lv_label_create(scr);
    lv_label_set_text(m_date, "THURSDAY, MARCH 26");
    lv_obj_set_style_text_color(m_date, D, 0);
    lv_obj_set_style_text_font(m_date, &lv_font_montserrat_20, 0);
    lv_obj_align(m_date, LV_ALIGN_CENTER, 0, 40);

    lbl_temp = lv_label_create(scr);
    lv_label_set_text(lbl_temp, "--°C");
    lv_obj_set_style_text_color(lbl_temp, D, 0);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_LEFT, SAFE_LEFT+5, Y0+40);

    m_bat = lv_label_create(scr);
    lv_label_set_text(m_bat, "80%");
    lv_obj_set_style_text_color(m_bat, D, 0);
    lv_obj_set_style_text_font(m_bat, &lv_font_montserrat_18, 0);
    lv_obj_align(m_bat, LV_ALIGN_TOP_RIGHT, -SAFE_RIGHT-5, Y0);

    lbl_alarm_bell = lv_label_create(scr);
    lv_label_set_text(lbl_alarm_bell, "ALARM");
    lv_obj_set_style_text_font(lbl_alarm_bell, &lv_font_montserrat_16, 0);
    lv_obj_set_size(lbl_alarm_bell, 85, 30);
    lv_obj_set_style_text_align(lbl_alarm_bell, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_alarm_bell, X0 + 5, Y0 + 9);
    lv_obj_add_flag(lbl_alarm_bell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(lbl_alarm_bell, BG, 0);
    lv_obj_set_style_bg_opa(lbl_alarm_bell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
    lv_obj_set_style_border_width(lbl_alarm_bell, 1, 0);
    lv_obj_set_style_radius(lbl_alarm_bell, 10, 0);
    lv_obj_set_style_pad_top(lbl_alarm_bell, 4, 0);
    lv_obj_set_style_text_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
    lv_obj_add_event_cb(lbl_alarm_bell, alarm_bell_click_cb, LV_EVENT_CLICKED, nullptr);

    m_sync = lv_label_create(scr);
    lv_label_set_text(m_sync, "");
    lv_obj_set_style_text_color(m_sync, D, 0);
    lv_obj_set_style_text_font(m_sync, &lv_font_montserrat_16, 0);
    lv_obj_align(m_sync, LV_ALIGN_BOTTOM_MID, 0, -(SAFE_BOTTOM+5));
}

static lv_point_precise_t hour_pts[2], min_pts[2], sec_pts[2];
static lv_obj_t *line_hour = nullptr, *line_min = nullptr, *line_sec = nullptr;

static void draw_hand(lv_obj_t **line_obj, lv_point_precise_t *pts,
                      float angle_deg, int len, int width, lv_color_t color) {
    float rad = (angle_deg - 90.0f) * 3.14159f / 180.0f;
    pts[0].x = CX; pts[0].y = CY;
    pts[1].x = CX + (int)(cosf(rad) * len);
    pts[1].y = CY + (int)(sinf(rad) * len);

    if (!*line_obj) {
        *line_obj = lv_line_create(scr);
        lv_obj_set_style_line_rounded(*line_obj, true, 0);
    }
    lv_line_set_points(*line_obj, pts, 2);
    lv_obj_set_style_line_width(*line_obj, width, 0);
    lv_obj_set_style_line_color(*line_obj, color, 0);
}

static void create_analog(void) {

    for (int i = 0; i < 12; i++) {
        float rad = (i * 30.0f - 90.0f) * 3.14159f / 180.0f;
        int r1 = 170, r2 = (i % 3 == 0) ? 145 : 155;
        lv_point_precise_t pts[2];
        pts[0].x = CX + (int)(cosf(rad)*r1); pts[0].y = CY + (int)(sinf(rad)*r1);
        pts[1].x = CX + (int)(cosf(rad)*r2); pts[1].y = CY + (int)(sinf(rad)*r2);

        static lv_point_precise_t marker_pts[12][2];
        marker_pts[i][0] = pts[0]; marker_pts[i][1] = pts[1];
        lv_obj_t *l = lv_line_create(scr);
        lv_line_set_points(l, marker_pts[i], 2);
        lv_obj_set_style_line_width(l, (i%3==0)?3:1, 0);
        lv_obj_set_style_line_color(l, (i%3==0)?G:D, 0);
    }

    lv_obj_t *dot = lv_obj_create(scr); lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 10, 10); lv_obj_set_pos(dot, CX-5, CY-5);
    lv_obj_set_style_bg_color(dot, G, 0); lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);

    a_time_lbl = lv_label_create(scr);
    lv_label_set_text(a_time_lbl, "00:00:00");
    lv_obj_set_style_text_color(a_time_lbl, D, 0);
    lv_obj_set_style_text_font(a_time_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(a_time_lbl, LV_ALIGN_CENTER, 0, 100);

    lbl_temp = lv_label_create(scr);
    lv_label_set_text(lbl_temp, "--°C");
    lv_obj_set_style_text_color(lbl_temp, D, 0);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, 75);

    a_date_lbl = lv_label_create(scr);
    lv_label_set_text(a_date_lbl, "MAR 26");
    lv_obj_set_style_text_color(a_date_lbl, D, 0);
    lv_obj_set_style_text_font(a_date_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(a_date_lbl, LV_ALIGN_CENTER, 0, 125);

    draw_hand(&line_hour, hour_pts, 0, 100, 5, G);
    draw_hand(&line_min, min_pts, 0, 140, 3, G);
    draw_hand(&line_sec, sec_pts, 0, 155, 1, D);

    lbl_alarm_bell = lv_label_create(scr);
    lv_label_set_text(lbl_alarm_bell, "ALARM");
    lv_obj_set_style_text_font(lbl_alarm_bell, &lv_font_montserrat_16, 0);
    lv_obj_set_size(lbl_alarm_bell, 85, 30);
    lv_obj_set_style_text_align(lbl_alarm_bell, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_alarm_bell, XM - 120, Y0 + 41);
    lv_obj_add_flag(lbl_alarm_bell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(lbl_alarm_bell, BG, 0);
    lv_obj_set_style_bg_opa(lbl_alarm_bell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
    lv_obj_set_style_border_width(lbl_alarm_bell, 1, 0);
    lv_obj_set_style_radius(lbl_alarm_bell, 10, 0);
    lv_obj_set_style_pad_top(lbl_alarm_bell, 4, 0);
    lv_obj_set_style_text_color(lbl_alarm_bell, alarm_enabled ? G : D, 0);
    lv_obj_add_event_cb(lbl_alarm_bell, alarm_bell_click_cb, LV_EVENT_CLICKED, nullptr);
}

static void rebuild_face(void) {
    if (scr) { lv_obj_delete(scr); scr = nullptr; }

    lbl_time = lbl_seconds = lbl_temp = lbl_month = lbl_date = lbl_sync = nullptr;
    bar_battery = lbl_battery = lbl_steps_v = lbl_gps = nullptr;
    m_time = m_date = m_bat = m_sync = nullptr;
    a_time_lbl = a_date_lbl = nullptr;
    line_hour = line_min = line_sec = nullptr;
    lbl_alarm_bell = nullptr;
    for (int i = 0; i < 7; i++) { day_labels[i] = nullptr; day_boxes[i] = nullptr; }

    scr = lv_obj_create(parent_scr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    switch (current_style) {
        case WF_PIPBOY:  create_pipboy(); break;
        case WF_MINIMAL: create_minimal(); break;
        case WF_ANALOG:  create_analog(); break;
        default: create_pipboy(); break;
    }
}

void watchface_create(lv_obj_t *parent) {
    parent_scr = parent;
    rebuild_face();
}

void watchface_next(void) {
    current_style = (WatchfaceStyle)((current_style + 1) % WF_COUNT);
    rebuild_face();
}

void watchface_prev(void) {
    current_style = (WatchfaceStyle)((current_style + WF_COUNT - 1) % WF_COUNT);
    rebuild_face();
}

WatchfaceStyle watchface_get_style(void) { return current_style; }

void watchface_set_time(uint8_t hour, uint8_t min) {
    c_hour = hour; c_min = min;
    
    if (!power_hal_screen_is_off()) {
        char b[8]; snprintf(b, sizeof(b), "%02d:%02d", hour, min);

        if (lbl_time) lv_label_set_text(lbl_time, b);
        if (m_time) lv_label_set_text(m_time, b);

        if (line_hour) {
            float h_angle = (hour % 12) * 30.0f + min * 0.5f;
            float m_angle = min * 6.0f;
            draw_hand(&line_hour, hour_pts, h_angle, 100, 5, G);
            draw_hand(&line_min, min_pts, m_angle, 140, 3, G);
        }
        if (a_time_lbl) {
            char tb[12]; snprintf(tb, sizeof(tb), "%02d:%02d:%02d", hour, min, c_sec);
            lv_label_set_text(a_time_lbl, tb);
        }
    }

    if (alarm_enabled && hour == alarm_hour && min == alarm_min) {
        if (!alarm_triggered && !alarm_active_ringing) {
            alarm_triggered = true;
            trigger_alarm();
        }
    } else {
        alarm_triggered = false;
    }
}

void watchface_set_seconds(uint8_t sec) {
    c_sec = sec;
    if (lbl_seconds) {
        char b[8]; snprintf(b, sizeof(b), ":%02d", sec);
        lv_label_set_text(lbl_seconds, b);
    }
    if (line_sec) {
        float s_angle = sec * 6.0f;
        draw_hand(&line_sec, sec_pts, s_angle, 155, 1, D);
    }
    if (a_time_lbl) {
        char b[12]; snprintf(b, sizeof(b), "%02d:%02d:%02d", c_hour, c_min, sec);
        lv_label_set_text(a_time_lbl, b);
    }
}

static const char *weekday_names[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

void watchface_set_date(uint8_t day, uint8_t month, uint8_t weekday, uint8_t week_num) {
    (void)week_num;
    c_day = day; c_month = month; c_wday = weekday;

    if (lbl_month && month >= 1 && month <= 12)
        lv_label_set_text(lbl_month, month_full[month-1]);
    if (lbl_date) {
        char b[12]; snprintf(b, sizeof(b), "%02d-%02d", month, day);
        lv_label_set_text(lbl_date, b);
    }
    int wd = (weekday == 0) ? 6 : weekday - 1;
    for (int i = 0; i < 7; i++) {
        if (!day_labels[i] || !day_boxes[i]) continue;
        if (i == wd) {
            lv_obj_set_style_text_color(day_labels[i], BG, 0);
            lv_obj_set_style_bg_color(day_boxes[i], G, 0);
            lv_obj_set_style_bg_opa(day_boxes[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_text_color(day_labels[i], D, 0);
            lv_obj_set_style_bg_opa(day_boxes[i], LV_OPA_TRANSP, 0);
        }
    }

    if (m_date && weekday < 7 && month >= 1 && month <= 12) {
        char b[32]; snprintf(b, sizeof(b), "%s, %s %d",
            weekday_names[weekday], month_full[month-1], day);
        lv_label_set_text(m_date, b);
    }

    if (a_date_lbl && month >= 1 && month <= 12) {
        char b[12]; snprintf(b, sizeof(b), "%s %d", month_names[month-1], day);
        lv_label_set_text(a_date_lbl, b);
    }
}

void watchface_set_battery(uint8_t pct, bool charging) {
    c_bat = pct; c_charging = charging;
    if (bar_battery) lv_bar_set_value(bar_battery, pct, LV_ANIM_ON);
    if (lbl_battery) {
        char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
        lv_label_set_text(lbl_battery, b);
    }
    if (m_bat) {
        char b[12]; snprintf(b, sizeof(b), "%s%d%%", charging?LV_SYMBOL_CHARGE:"", pct);
        lv_label_set_text(m_bat, b);
    }
}

void watchface_set_steps(uint32_t steps, uint32_t goal) {
    c_steps = steps;
    if (lbl_steps_v) {
        char b[20]; snprintf(b, sizeof(b), "STEPS: %lu", (unsigned long)steps);
        lv_label_set_text(lbl_steps_v, b);
    }
}

void watchface_set_distance(float km) { (void)km; }

void watchface_set_gps(float lat, float lon, float alt) {
    if (lbl_gps) {
        bool wd = gps_app_is_wardriving_active();
        bool gps_on = gps_app_is_enabled();

        if (!gps_on) {
            lv_label_set_text(lbl_gps, LV_SYMBOL_GPS " OFF");
            lv_obj_set_style_text_color(lbl_gps, D, 0);
            return;
        }

        if (wd) {
            lv_label_set_text(lbl_gps, "wardriving");
            if (isnan(lat) || isnan(lon)) {
                lv_obj_set_style_text_color(lbl_gps, D, 0);
            } else {
                lv_obj_set_style_text_color(lbl_gps, G, 0);
            }
            return;
        }

        char b[64];
        if (isnan(lat) || isnan(lon)) {
            snprintf(b, sizeof(b), LV_SYMBOL_GPS " NO FIX");
            lv_obj_set_style_text_color(lbl_gps, D, 0);
        } else {
            snprintf(b, sizeof(b), LV_SYMBOL_GPS " %.4f,%.4f", lat, lon);
            lv_obj_set_style_text_color(lbl_gps, G, 0);
        }
        lv_label_set_text(lbl_gps, b);
    }
}

void watchface_set_temperature(int16_t temp_c) {
    if (lbl_temp) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d°C", temp_c);
        lv_label_set_text(lbl_temp, buf);
    }
}

void watchface_set_sync_status(bool wifi, bool ntp_ok, bool gps_fix) {
    c_ntp = ntp_ok; c_wifi = wifi; c_gps = gps_fix;
    char b[192];
    bool ws = web_server_is_active();
    bool wdg = ble_uart_is_active();
    bool mc = lora_svc_is_running();
    bool bg = recon_is_bitgotchi_active();
    bool rec = audio_rec_is_recording();

    snprintf(b, sizeof(b),
        "#%s " LV_SYMBOL_WIFI " %s#  #%s " LV_SYMBOL_GPS " %s#  #%s " LV_SYMBOL_KEYBOARD " %s#  #%s " LV_SYMBOL_BLUETOOTH " %s#  #%s " LV_SYMBOL_BULLET " %s#  #%s " LV_SYMBOL_EYE_OPEN " %s#  #%s " LV_SYMBOL_BULLET " REC#",
        wifi ? RC_G : RC_D, wifi ? "ON" : "--",
        gps_fix ? RC_G : RC_D, gps_fix ? "FIX" : "--",
        ws ? RC_G : RC_D, ws ? "ON" : "--",
        wdg ? RC_G : RC_D, wdg ? "ON" : "--",
        mc ? RC_G : RC_D, mc ? "ON" : "--",
        bg ? RC_G : RC_D, bg ? "ON" : "--",
        rec ? "FF3333" : RC_D);

    if (lbl_sync) {
        lv_label_set_text(lbl_sync, b);
    }
    if (m_sync) {
        lv_label_set_text(m_sync, ntp_ok ? "NTP OK" : "");
        lv_obj_set_style_text_color(m_sync, ntp_ok ? G : D, 0);
    }
}

void watchface_update(void) {}
void watchface_destroy(void) {
    if (scr) {
        lv_obj_delete(scr);
        scr = nullptr;
    }
    lbl_time = lbl_seconds = lbl_temp = lbl_month = lbl_date = lbl_sync = nullptr;
    bar_battery = lbl_battery = lbl_steps_v = lbl_gps = nullptr;
    m_time = m_date = m_bat = m_sync = nullptr;
    a_time_lbl = a_date_lbl = nullptr;
    line_hour = line_min = line_sec = nullptr;
    lbl_alarm_bell = nullptr;
    for (int i = 0; i < 7; i++) {
        day_labels[i] = nullptr;
        day_boxes[i] = nullptr;
    }
}
