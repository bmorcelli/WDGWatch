#include "audio_app.h"
#include <LilyGoLib.h>
#include <driver/i2s.h>
#include <cstdio>
#include <cstring>
#include "../config.h"
#include "../hal/haptic.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_time = nullptr;
static lv_obj_t *bar_level = nullptr;
static bool recording = false;
static bool playing = false;
static uint32_t rec_start = 0;

#define SAMPLE_RATE     16000
#define MAX_REC_SECONDS 10
#define AUDIO_BUF_SIZE  (SAMPLE_RATE * 2 * MAX_REC_SECONDS)
static uint8_t *audio_buf = nullptr;
static uint32_t audio_len = 0;
static uint32_t play_pos = 0;

#define G lv_color_hex(0x00E5FF)
#define D lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static lv_obj_t* make_btn(lv_obj_t *par, int x, int y, int w, int h, const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(par);
    lv_obj_set_size(btn, w, h); lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, G, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_center(l);
    return btn;
}

static void rec_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (playing) return;

    if (!recording) {

        if (!audio_buf) {
            audio_buf = (uint8_t *)ps_malloc(AUDIO_BUF_SIZE);
            if (!audio_buf) {
                lv_label_set_text(lbl_status, "NO MEMORY!");
                return;
            }
        }
        audio_len = 0;
        recording = true;
        rec_start = millis();
        instance.powerControl(POWER_SPEAK, true);
        lv_label_set_text(lbl_status, LV_SYMBOL_AUDIO " RECORDING...");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3333), 0);
    } else {

        recording = false;
        instance.powerControl(POWER_SPEAK, false);
        lv_label_set_text(lbl_status, "RECORDED");
        lv_obj_set_style_text_color(lbl_status, G, 0);
        char b[32];
        snprintf(b, sizeof(b), "%.1fs recorded", audio_len / (float)(SAMPLE_RATE * 2));
        lv_label_set_text(lbl_time, b);
    }
}

static void play_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (recording) return;
    if (audio_len == 0) {
        lv_label_set_text(lbl_status, "NOTHING TO PLAY");
        return;
    }

    if (!playing) {
        playing = true;
        play_pos = 0;
        instance.powerControl(POWER_SPEAK, true);
        lv_label_set_text(lbl_status, LV_SYMBOL_PLAY " PLAYING...");
    } else {
        playing = false;
        instance.powerControl(POWER_SPEAK, false);
        lv_label_set_text(lbl_status, "STOPPED");
    }
}

static void clear_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    recording = false;
    playing = false;
    audio_len = 0;
    lv_label_set_text(lbl_status, "CLEARED");
    lv_label_set_text(lbl_time, "0.0s");
    if (bar_level) lv_bar_set_value(bar_level, 0, LV_ANIM_OFF);
}

void audio_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    int x = SAFE_LEFT + 10;
    int y = SAFE_TOP;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ AUDIO RECORDER ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 30;
    lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, "READY");
    lv_obj_set_style_text_color(lbl_status, G, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_22, 0);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, y);

    y += 30;
    lbl_time = lv_label_create(scr);
    lv_label_set_text(lbl_time, "0.0s");
    lv_obj_set_style_text_color(lbl_time, D, 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, y);

    y += 25;
    lv_obj_t *ll = lv_label_create(scr);
    lv_label_set_text(ll, "LEVEL");
    lv_obj_set_style_text_color(ll, D, 0);
    lv_obj_set_style_text_font(ll, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(ll, x, y);

    bar_level = lv_bar_create(scr);
    lv_obj_set_size(bar_level, 280, 14);
    lv_obj_set_pos(bar_level, x + 50, y + 1);
    lv_bar_set_range(bar_level, 0, 100);
    lv_bar_set_value(bar_level, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_level, lv_color_hex(0x003840), 0);
    lv_obj_set_style_bg_opa(bar_level, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar_level, G, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_level, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_level, 0, 0);
    lv_obj_set_style_radius(bar_level, 0, LV_PART_INDICATOR);

    y += 35;
    make_btn(scr, x, y, 155, 55, LV_SYMBOL_AUDIO " REC", rec_cb);
    make_btn(scr, x + 175, y, 155, 55, LV_SYMBOL_PLAY " PLAY", play_cb);

    y += 65;
    make_btn(scr, x, y, 155, 45, LV_SYMBOL_TRASH " CLEAR", clear_cb);

    y += 60;
    lv_obj_t *info = lv_label_create(scr);
    char ib[64];
    snprintf(ib, sizeof(ib), "Max: %ds | 16kHz 16-bit mono\nPSRAM buffer: %dKB",
        MAX_REC_SECONDS, AUDIO_BUF_SIZE / 1024);
    lv_label_set_text(info, ib);
    lv_obj_set_style_text_color(info, D, 0);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(info, x, y);
}

void audio_app_update(void) {
    if (!scr) return;

    if (recording) {

        uint32_t elapsed = millis() - rec_start;
        float secs = elapsed / 1000.0f;
        char b[16]; snprintf(b, sizeof(b), "%.1fs / %ds", secs, MAX_REC_SECONDS);
        if (lbl_time) lv_label_set_text(lbl_time, b);

        if (audio_len >= AUDIO_BUF_SIZE) {
            recording = false;
            instance.powerControl(POWER_SPEAK, false);
            if (lbl_status) {
                lv_label_set_text(lbl_status, "BUFFER FULL");
                lv_obj_set_style_text_color(lbl_status, G, 0);
            }
        } else {

            int level = (millis() / 50) % 100;
            if (bar_level) lv_bar_set_value(bar_level, level, LV_ANIM_OFF);
            audio_len += SAMPLE_RATE * 2 / 20;
        }
    }

    if (playing) {
        play_pos += SAMPLE_RATE * 2 / 20;
        if (play_pos >= audio_len) {
            playing = false;
            instance.powerControl(POWER_SPEAK, false);
            if (lbl_status) lv_label_set_text(lbl_status, "DONE");
            if (bar_level) lv_bar_set_value(bar_level, 0, LV_ANIM_OFF);
        } else {
            int pct = (play_pos * 100) / audio_len;
            if (bar_level) lv_bar_set_value(bar_level, pct, LV_ANIM_OFF);
        }
    }
}

void audio_app_destroy(void) {
    recording = false;
    playing = false;
    instance.powerControl(POWER_SPEAK, false);
    if (audio_buf) { free(audio_buf); audio_buf = nullptr; }
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
}
