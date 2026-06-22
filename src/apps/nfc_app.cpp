#include "nfc_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/nfc_service.h"
#include "app_common.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_uid = nullptr;
static lv_obj_t *lbl_ndef = nullptr;
static lv_obj_t *lbl_saved = nullptr;

#define G  lv_color_hex(PIPBOY_GREEN)
#define D  lv_color_hex(PIPBOY_GREEN_DIM)
#define BG lv_color_hex(PIPBOY_BG)

static lv_obj_t* make_btn(lv_obj_t *par, int x, int y, int w, int h, const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(par);
    lv_obj_set_size(btn, w, h); lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, G, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    style_button_by_position(btn, y, h);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_center(l);
    return btn;
}

static void scan_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_scan(); }
static void stop_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_stop(); }
static void save_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_save(); }
static void select_cb(lv_event_t *e)  { (void)e; haptic_click(); nfc_svc_request_select_next(); }
static void emit_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_emulate(); }
static void del_cb(lv_event_t *e) {
    (void)e; haptic_click();
    int idx = nfc_svc_selected_idx();
    if (idx >= 0) nfc_svc_request_delete(idx);
}

static void update_saved_list(void) {
    if (!lbl_saved) return;
    int cnt = nfc_svc_saved_count();
    int sel = nfc_svc_selected_idx();
    if (cnt == 0) { lv_label_set_text(lbl_saved, "No saved tags"); return; }
    char buf[400] = ""; int pos = 0;
    for (int i = 0; i < cnt && pos < 380; i++) {
        pos += snprintf(buf+pos, sizeof(buf)-pos, "%s%d. %s",
            (i==sel)? "> " : "  ", i+1, nfc_svc_tag_name(i));
        if (i < cnt-1) pos += snprintf(buf+pos, sizeof(buf)-pos, "\n");
    }
    lv_label_set_text(lbl_saved, buf);
}

void nfc_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    int x = SAFE_LEFT + 5, y = SAFE_TOP;
    int bw = 113, bh = 48;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ NFC ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 25;
    lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, nfc_svc_is_scanning() ? "SCANNING..." : "READY");
    lv_obj_set_style_text_color(lbl_status, G, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_status, x, y);
    lv_obj_set_width(lbl_status, 350);

    y += 20;
    lbl_uid = lv_label_create(scr);
    lv_label_set_text(lbl_uid, "UID: --");
    lv_obj_set_style_text_color(lbl_uid, G, 0);
    lv_obj_set_style_text_font(lbl_uid, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_uid, x, y);

    y += 16;
    lbl_ndef = lv_label_create(scr);
    lv_label_set_text(lbl_ndef, "");
    lv_obj_set_style_text_color(lbl_ndef, D, 0);
    lv_obj_set_style_text_font(lbl_ndef, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_ndef, x, y);
    lv_obj_set_width(lbl_ndef, 350);

    y += 25;
    make_btn(scr, x, y, bw, bh, LV_SYMBOL_REFRESH " SCAN", scan_cb);
    make_btn(scr, x+bw+7, y, bw, bh, LV_SYMBOL_SAVE " SAVE", save_cb);
    make_btn(scr, x+2*(bw+7), y, bw, bh, LV_SYMBOL_CLOSE " STOP", stop_cb);

    y += bh + 8;
    make_btn(scr, x, y, bw, bh, LV_SYMBOL_RIGHT " SEL", select_cb);
    make_btn(scr, x+bw+7, y, bw, bh, LV_SYMBOL_UPLOAD " EMIT", emit_cb);
    make_btn(scr, x+2*(bw+7), y, bw, bh, LV_SYMBOL_TRASH " DEL", del_cb);

    y += bh + 10;
    lv_obj_t *st = lv_label_create(scr);
    lv_label_set_text(st, "SAVED:");
    lv_obj_set_style_text_color(st, D, 0);
    lv_obj_set_style_text_font(st, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(st, x, y);

    y += 16;
    lbl_saved = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_saved, G, 0);
    lv_obj_set_style_text_font(lbl_saved, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_saved, x, y);
    lv_obj_set_width(lbl_saved, 350);
    lv_label_set_long_mode(lbl_saved, LV_LABEL_LONG_WRAP);
    update_saved_list();
}

void nfc_app_update(void) {
    if (!scr) return;

    if (nfc_svc_is_scanning() && lbl_status) {
        lv_label_set_text(lbl_status, "SCANNING...");
        lv_obj_set_style_text_color(lbl_status, G, 0);
    } else if (nfc_svc_is_emulating() && lbl_status) {
        lv_label_set_text(lbl_status, "EMULATING...");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF6600), 0);
    }

    if (nfc_svc_tag_detected()) {
        haptic_buzz();
        if (lbl_uid) {
            char b[72]; snprintf(b, sizeof(b), "UID: %s", nfc_svc_last_uid());
            lv_label_set_text(lbl_uid, b);
        }
        if (lbl_ndef) lv_label_set_text(lbl_ndef, nfc_svc_last_ndef());
        if (lbl_status) {
            lv_label_set_text(lbl_status, "TAG FOUND!");
            lv_obj_set_style_text_color(lbl_status, G, 0);
        }
    }

    update_saved_list();
}

void nfc_app_destroy(void) {

    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_status = lbl_uid = lbl_ndef = lbl_saved = nullptr;
}
