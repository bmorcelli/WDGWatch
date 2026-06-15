#include "app_common.h"
#include "rf_app.h"
#include <LilyGoLib.h>
#include "../config.h"
#include "../hal/rf_service.h"
#include "../hal/haptic.h"

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define R  lv_color_hex(0xFF3B3B)
#define BG lv_color_hex(0x000000)

static lv_obj_t *scr           = nullptr;
static lv_obj_t *lbl_jammer    = nullptr;
static lv_obj_t *lbl_tesla     = nullptr;
static lv_obj_t *btn_jam_stop  = nullptr;

static uint32_t selected_freq = 433920000;

struct DurationPreset { const char *label; uint32_t secs; };
static const DurationPreset DURATIONS[] = {
    { "10s", 10 },
    { "20s", 20 },
    { "30s", 30 },
    { "1m",  60 },
    { "3m",  180 },
    { "5m",  300 }
};
static const int DURATION_COUNT = sizeof(DURATIONS) / sizeof(DurationPreset);
static int selected_duration_idx = -1;
static uint32_t jam_start_ms = 0;
static uint32_t jam_duration_secs = 0;

static lv_obj_t *dur_btns[6] = {};

struct FreqPreset { const char *label; uint32_t freq_hz; };
static const FreqPreset FREQS[] = {
    { "315",  315000000 },
    { "433",  433920000 },
    { "868",  868000000 },
    { "915",  915000000 },
    { "2400", 2400000000UL },
};
static const int FREQ_COUNT = sizeof(FREQS) / sizeof(FreqPreset);

static lv_obj_t *freq_btns[5] = {};

static lv_obj_t* make_btn(lv_obj_t *par, int x, int y, int w, int h,
                           const char *txt, lv_event_cb_t cb, void *user = nullptr) {
    lv_obj_t *btn = lv_button_create(par);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, G, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, G, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, 40, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_center(l);
    return btn;
}

static void freq_btn_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= FREQ_COUNT) return;
    if (rf_jammer_is_active()) return;

    selected_freq = FREQS[idx].freq_hz;
    haptic_click();

    for (int i = 0; i < FREQ_COUNT; i++) {
        if (!freq_btns[i]) continue;
        lv_obj_set_style_border_color(freq_btns[i], (i == idx) ? G : D, 0);
        lv_obj_t *lbl = lv_obj_get_child(freq_btns[i], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, (i == idx) ? G : D, 0);
    }
}

static void dur_btn_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= DURATION_COUNT) return;
    if (rf_jammer_is_active()) return;

    selected_duration_idx = idx;
    haptic_click();

    for (int i = 0; i < DURATION_COUNT; i++) {
        if (!dur_btns[i]) continue;
        lv_obj_set_style_border_color(dur_btns[i], (i == idx) ? G : D, 0);
        lv_obj_t *lbl = lv_obj_get_child(dur_btns[i], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, (i == idx) ? G : D, 0);
    }
}

static void jammer_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (rf_jammer_is_active()) {
        rf_jammer_stop();
    } else {
        if (selected_duration_idx == -1) {
            if (lbl_jammer) {
                lv_label_set_text(lbl_jammer, "ERROR: CHOOSE DURATION");
                lv_obj_set_style_text_color(lbl_jammer, lv_color_hex(0xFF3B3B), 0);
            }
            return;
        }
        jam_duration_secs = DURATIONS[selected_duration_idx].secs;
        jam_start_ms = millis();
        rf_jammer_start(selected_freq);
    }
}

static void tesla_cb(lv_event_t *e) {
    (void)e;
    if (rf_jammer_is_active() || rf_tesla_is_sending()) return;
    haptic_click();
    rf_tesla_send();
}

void rf_app_create(lv_obj_t *parent) {
    
    rf_radio_wake(433.92f);

    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);
    app_add_back_button(scr);

    int x = SAFE_LEFT + 5;
    int y = SAFE_TOP;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ RF TOOLS ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);
    y += 30;

    lv_obj_t *sec1 = lv_label_create(scr);
    lv_label_set_text(sec1, LV_SYMBOL_VOLUME_MAX "  RF JAMMER");
    lv_obj_set_style_text_color(sec1, D, 0);
    lv_obj_set_style_text_font(sec1, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(sec1, x, y);
    y += 22;

    const int FBW = 66;
    const int FBH = 36;
    const int FGAP = 5;
    int fx = x;
    for (int i = 0; i < FREQ_COUNT; i++) {
        char lbl[12];
        snprintf(lbl, sizeof(lbl), "%s\nMHz", FREQS[i].label);
        freq_btns[i] = make_btn(scr, fx, y, FBW, FBH, lbl,
                                freq_btn_cb, (void *)(intptr_t)i);
        if (i == 1) {
            lv_obj_set_style_border_color(freq_btns[i], G, 0);
        } else {
            lv_obj_set_style_border_color(freq_btns[i], D, 0);
            lv_obj_t *l = lv_obj_get_child(freq_btns[i], 0);
            if (l) lv_obj_set_style_text_color(l, D, 0);
        }
        fx += FBW + FGAP;
    }
    y += FBH + 10;

    lv_obj_t *lbl_dur_title = lv_label_create(scr);
    lv_label_set_text(lbl_dur_title, "CHOOSE DURATION:");
    lv_obj_set_style_text_color(lbl_dur_title, D, 0);
    lv_obj_set_style_text_font(lbl_dur_title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_dur_title, x, y);
    y += 18;

    const int DBW = 54;
    const int DBH = 28;
    const int DGAP = 6;
    int dx = x;
    for (int i = 0; i < DURATION_COUNT; i++) {
        dur_btns[i] = make_btn(scr, dx, y, DBW, DBH, DURATIONS[i].label,
                               dur_btn_cb, (void *)(intptr_t)i);
        lv_obj_set_style_border_color(dur_btns[i], D, 0);
        lv_obj_t *l = lv_obj_get_child(dur_btns[i], 0);
        if (l) lv_obj_set_style_text_color(l, D, 0);
        dx += DBW + DGAP;
    }
    y += DBH + 12;

    lbl_jammer = lv_label_create(scr);
    lv_label_set_text(lbl_jammer, "JAMMER: OFF");
    lv_obj_set_style_text_color(lbl_jammer, D, 0);
    lv_obj_set_style_text_font(lbl_jammer, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_jammer, x, y);

    btn_jam_stop = make_btn(scr, x + 200, y - 5, 140, 32,
                            LV_SYMBOL_PLAY "  JAM / STOP", jammer_cb);
    y += 40;

    lv_obj_t *line = lv_obj_create(scr);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, 1);
    lv_obj_set_style_bg_color(line, D, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_pos(line, x, y);
    y += 12;

    lv_obj_t *sec2 = lv_label_create(scr);
    lv_label_set_text(sec2, LV_SYMBOL_CHARGE "  TESLA PORT");
    lv_obj_set_style_text_color(sec2, D, 0);
    lv_obj_set_style_text_font(sec2, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(sec2, x, y);
    y += 22;

    lv_obj_t *tesla_info = lv_label_create(scr);
    lv_label_set_text(tesla_info, "433.92 MHz  |  OOK burst  |  1 pass");
    lv_obj_set_style_text_color(tesla_info, D, 0);
    lv_obj_set_style_text_font(tesla_info, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(tesla_info, x, y);
    y += 20;

    make_btn(scr, x, y, 340, 50,
             LV_SYMBOL_CHARGE "  SEND TESLA SIGNAL", tesla_cb);
    y += 60;

    lbl_tesla = lv_label_create(scr);
    lv_label_set_text(lbl_tesla, "READY");
    lv_obj_set_style_text_color(lbl_tesla, D, 0);
    lv_obj_set_style_text_font(lbl_tesla, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_tesla, x, y);
}

void rf_app_update(void) {
    if (!scr) return;

    if (lbl_jammer) {
        if (rf_jammer_is_active()) {
            uint32_t elapsed_ms = millis() - jam_start_ms;
            uint32_t elapsed_secs = elapsed_ms / 1000;
            if (elapsed_secs >= jam_duration_secs) {
                rf_jammer_stop();
                lv_label_set_text(lbl_jammer, "JAMMER: OFF");
                lv_obj_set_style_text_color(lbl_jammer, D, 0);
            } else {
                uint32_t remaining = jam_duration_secs - elapsed_secs;
                char buf[48];
                snprintf(buf, sizeof(buf), "JAMMING: %ds (%.1f MHz)",
                         remaining, (float)rf_jammer_get_freq() / 1000000.0f);
                lv_label_set_text(lbl_jammer, buf);
                lv_obj_set_style_text_color(lbl_jammer, lv_color_hex(0xFF3B3B), 0);
            }
        } else {
            const char* current_txt = lv_label_get_text(lbl_jammer);
            if (strstr(current_txt, "JAMMING") != nullptr || strstr(current_txt, "ERROR") != nullptr) {
                lv_label_set_text(lbl_jammer, "JAMMER: OFF");
                lv_obj_set_style_text_color(lbl_jammer, D, 0);
            }
        }
    }

    if (lbl_tesla) {
        if (rf_tesla_is_sending()) {
            lv_label_set_text(lbl_tesla, LV_SYMBOL_CHARGE " SENDING...");
            lv_obj_set_style_text_color(lbl_tesla, G, 0);
        } else {
            lv_label_set_text(lbl_tesla, "READY");
            lv_obj_set_style_text_color(lbl_tesla, D, 0);
        }
    }
}

void rf_app_destroy(void) {
    
    rf_radio_sleep();

    for (int i = 0; i < FREQ_COUNT; i++) freq_btns[i] = nullptr;
    for (int i = 0; i < DURATION_COUNT; i++) dur_btns[i] = nullptr;
    lbl_jammer   = nullptr;
    lbl_tesla    = nullptr;
    btn_jam_stop = nullptr;
    selected_duration_idx = -1;
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
}
