#include "nfc_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/nfc_service.h"
#include "app_common.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *tabview = nullptr;

static lv_obj_t *lbl_scan_status = nullptr;
static lv_obj_t *lbl_uid = nullptr;
static lv_obj_t *lbl_tag_type = nullptr;
static lv_obj_t *lbl_ndef = nullptr;
static lv_obj_t *lbl_dict_progress = nullptr;
static lv_obj_t *bar_dict = nullptr;

static lv_obj_t *lbl_saved_list = nullptr;

static lv_obj_t *lbl_emu_status = nullptr;
static lv_obj_t *lbl_emu_selected = nullptr;

static lv_obj_t *lbl_emv_status = nullptr;
static lv_obj_t *lbl_emv_pan = nullptr;
static lv_obj_t *lbl_emv_exp = nullptr;
static lv_obj_t *lbl_emv_brand = nullptr;

#define G  lv_color_hex(PIPBOY_GREEN)
#define D  lv_color_hex(PIPBOY_GREEN_DIM)
#define BG lv_color_hex(PIPBOY_BG)

static lv_obj_t* make_btn(lv_obj_t *par, int x, int y, int w, int h, const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(par);
    lv_obj_set_size(btn, w, h); lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, G, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
    return btn;
}

static void scan_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_scan(); }
static void stop_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_stop(); }
static void save_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_save(); }
static void dict_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_dict_attack(); }
static void select_cb(lv_event_t *e)  { (void)e; haptic_click(); nfc_svc_request_select_next(); }
static void emit_cb(lv_event_t *e)    { (void)e; haptic_click(); nfc_svc_request_emulate(); }
static void emv_cb(lv_event_t *e)     { (void)e; haptic_click(); nfc_svc_request_emv_read(); }
static void del_cb(lv_event_t *e) {
    (void)e; haptic_click();
    int idx = nfc_svc_selected_idx();
    if (idx >= 0) nfc_svc_request_delete(idx);
}
static void export_cb(lv_event_t *e)  { (void)e; haptic_click(); nfc_svc_request_export(); }

static void style_tabview(lv_obj_t *tv) {
    lv_obj_set_style_bg_color(tv, BG, 0);
    lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, 0);

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(tab_bar, BG, 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tab_bar, D, 0);
    lv_obj_set_style_border_width(tab_bar, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_bar, D, 0);
    lv_obj_set_style_text_color(tab_bar, G, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_bar, G, LV_PART_ITEMS | LV_STATE_CHECKED);
    
    
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_18, 0);
    lv_obj_set_style_pad_hor(tab_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(tab_bar, 2, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(tab_bar, 0, 0);
}

static void style_tab_content(lv_obj_t *tab) {
    lv_obj_set_style_bg_color(tab, BG, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tab, 5, 0);
    lv_obj_set_style_pad_gap(tab, 4, 0);
}

static void create_tab_scan(lv_obj_t *tab) {
    style_tab_content(tab);
    int y = 0;
    int bw = 108, bh = 42;

    lbl_scan_status = lv_label_create(tab);
    lv_label_set_text(lbl_scan_status, nfc_svc_is_scanning() ? "SCANNING..." : "READY");
    lv_obj_set_style_text_color(lbl_scan_status, G, 0);
    lv_obj_set_style_text_font(lbl_scan_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_scan_status, 0, y);
    lv_obj_set_width(lbl_scan_status, 350);

    y += 22;
    lbl_tag_type = lv_label_create(tab);
    lv_label_set_text(lbl_tag_type, "TYPE: --");
    lv_obj_set_style_text_color(lbl_tag_type, D, 0);
    lv_obj_set_style_text_font(lbl_tag_type, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_tag_type, 0, y);

    y += 16;
    lbl_uid = lv_label_create(tab);
    lv_label_set_text(lbl_uid, "UID: --");
    lv_obj_set_style_text_color(lbl_uid, G, 0);
    lv_obj_set_style_text_font(lbl_uid, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_uid, 0, y);

    y += 16;
    lbl_ndef = lv_label_create(tab);
    lv_label_set_text(lbl_ndef, "");
    lv_obj_set_style_text_color(lbl_ndef, D, 0);
    lv_obj_set_style_text_font(lbl_ndef, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_ndef, 0, y);
    lv_obj_set_width(lbl_ndef, 340);

    y += 22;
    make_btn(tab, 0, y, bw, bh, LV_SYMBOL_REFRESH " SCAN", scan_cb);
    make_btn(tab, bw+5, y, bw, bh, LV_SYMBOL_SAVE " SAVE", save_cb);
    make_btn(tab, 2*(bw+5), y, bw, bh, LV_SYMBOL_CLOSE " STOP", stop_cb);

    y += bh + 5;
    make_btn(tab, 0, y, 170, bh, LV_SYMBOL_SETTINGS " DICT ATTACK", dict_cb);
    make_btn(tab, 175, y, 170, bh, LV_SYMBOL_DOWNLOAD " EXPORT", export_cb);

    y += bh + 8;
    lbl_dict_progress = lv_label_create(tab);
    lv_label_set_text(lbl_dict_progress, "");
    lv_obj_set_style_text_color(lbl_dict_progress, D, 0);
    lv_obj_set_style_text_font(lbl_dict_progress, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_dict_progress, 0, y);
    lv_obj_set_width(lbl_dict_progress, 340);

    y += 18;
    bar_dict = lv_bar_create(tab);
    lv_obj_set_size(bar_dict, 340, 12);
    lv_obj_set_pos(bar_dict, 0, y);
    lv_bar_set_range(bar_dict, 0, 100);
    lv_bar_set_value(bar_dict, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_dict, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_color(bar_dict, G, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_dict, 4, 0);
    lv_obj_set_style_radius(bar_dict, 4, LV_PART_INDICATOR);
    lv_obj_add_flag(bar_dict, LV_OBJ_FLAG_HIDDEN);
}

static void create_tab_saved(lv_obj_t *tab) {
    style_tab_content(tab);

    lbl_saved_list = lv_label_create(tab);
    lv_obj_set_style_text_color(lbl_saved_list, G, 0);
    lv_obj_set_style_text_font(lbl_saved_list, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_saved_list, 0, 0);
    lv_obj_set_width(lbl_saved_list, 340);
    lv_label_set_long_mode(lbl_saved_list, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_saved_list, "No saved tags");

    int y = 260;
    int bw = 108, bh = 42;
    make_btn(tab, 0, y, bw, bh, LV_SYMBOL_RIGHT " SEL", select_cb);
    make_btn(tab, bw+5, y, bw, bh, LV_SYMBOL_TRASH " DEL", del_cb);
    make_btn(tab, 2*(bw+5), y, bw, bh, LV_SYMBOL_DOWNLOAD " EXP", export_cb);
}

static void create_tab_emulate(lv_obj_t *tab) {
    style_tab_content(tab);

    lbl_emu_selected = lv_label_create(tab);
    lv_label_set_text(lbl_emu_selected, "SELECT A TAG FIRST");
    lv_obj_set_style_text_color(lbl_emu_selected, G, 0);
    lv_obj_set_style_text_font(lbl_emu_selected, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_emu_selected, 0, 0);
    lv_obj_set_width(lbl_emu_selected, 340);
    lv_label_set_long_mode(lbl_emu_selected, LV_LABEL_LONG_WRAP);

    lbl_emu_status = lv_label_create(tab);
    lv_label_set_text(lbl_emu_status, "IDLE");
    lv_obj_set_style_text_color(lbl_emu_status, D, 0);
    lv_obj_set_style_text_font(lbl_emu_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_emu_status, 0, 50);

    int y = 100;
    make_btn(tab, 0, y, 170, 50, LV_SYMBOL_UPLOAD " EMULATE", emit_cb);
    make_btn(tab, 175, y, 170, 50, LV_SYMBOL_CLOSE " STOP", stop_cb);

    make_btn(tab, 0, y + 60, 170, 50, LV_SYMBOL_LEFT " PREV", select_cb);
    make_btn(tab, 175, y + 60, 170, 50, LV_SYMBOL_RIGHT " NEXT", select_cb);
}

static void create_tab_emv(lv_obj_t *tab) {
    style_tab_content(tab);

    lbl_emv_status = lv_label_create(tab);
    lv_label_set_text(lbl_emv_status, "TAP BANK CARD");
    lv_obj_set_style_text_color(lbl_emv_status, G, 0);
    lv_obj_set_style_text_font(lbl_emv_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_emv_status, 0, 0);

    int y = 40;
    lbl_emv_brand = lv_label_create(tab);
    lv_label_set_text(lbl_emv_brand, "");
    lv_obj_set_style_text_color(lbl_emv_brand, G, 0);
    lv_obj_set_style_text_font(lbl_emv_brand, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl_emv_brand, 0, y);

    y += 30;
    lbl_emv_pan = lv_label_create(tab);
    lv_label_set_text(lbl_emv_pan, "");
    lv_obj_set_style_text_color(lbl_emv_pan, G, 0);
    lv_obj_set_style_text_font(lbl_emv_pan, &lv_font_montserrat_22, 0);
    lv_obj_set_pos(lbl_emv_pan, 0, y);

    y += 30;
    lbl_emv_exp = lv_label_create(tab);
    lv_label_set_text(lbl_emv_exp, "");
    lv_obj_set_style_text_color(lbl_emv_exp, D, 0);
    lv_obj_set_style_text_font(lbl_emv_exp, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_emv_exp, 0, y);

    int by = 180;
    make_btn(tab, 0, by, 170, 50, LV_SYMBOL_REFRESH " READ", emv_cb);
    make_btn(tab, 175, by, 170, 50, LV_SYMBOL_CLOSE " STOP", stop_cb);
}

void nfc_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_size(tabview, 32);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_obj_set_size(tabview, 390, 470);
    lv_obj_align(tabview, LV_ALIGN_CENTER, 0, 10);
    style_tabview(tabview);

    lv_obj_t *tab_scan = lv_tabview_add_tab(tabview, "SCAN");
    lv_obj_t *tab_saved = lv_tabview_add_tab(tabview, "SAVED");
    lv_obj_t *tab_emu = lv_tabview_add_tab(tabview, "EMUL");
    lv_obj_t *tab_emv = lv_tabview_add_tab(tabview, "EMV");

    create_tab_scan(tab_scan);
    create_tab_saved(tab_saved);
    create_tab_emulate(tab_emu);
    create_tab_emv(tab_emv);
}

static void update_saved_list(void) {
    if (!lbl_saved_list) return;
    int cnt = nfc_svc_saved_count();
    int sel = nfc_svc_selected_idx();
    if (cnt == 0) { lv_label_set_text(lbl_saved_list, "No saved tags"); return; }
    char buf[600] = ""; int pos = 0;
    for (int i = 0; i < cnt && pos < 580; i++) {
        pos += snprintf(buf+pos, sizeof(buf)-pos, "%s%d. %s\n",
            (i==sel)? "> " : "  ", i+1, nfc_svc_tag_name(i));
    }
    lv_label_set_text(lbl_saved_list, buf);
}

void nfc_app_update(void) {
    if (!scr) return;

    
    if (lbl_scan_status) {
        if (nfc_svc_is_scanning()) {
            lv_label_set_text(lbl_scan_status, "SCANNING...");
            lv_obj_set_style_text_color(lbl_scan_status, G, 0);
        } else if (nfc_svc_is_dict_attacking()) {
            lv_label_set_text(lbl_scan_status, "DICT ATTACK...");
            lv_obj_set_style_text_color(lbl_scan_status, G, 0);
        } else if (nfc_svc_is_emv_reading()) {
            lv_label_set_text(lbl_scan_status, "EMV READING...");
            lv_obj_set_style_text_color(lbl_scan_status, G, 0);
        } else {
            lv_label_set_text(lbl_scan_status, "READY");
            lv_obj_set_style_text_color(lbl_scan_status, D, 0);
        }
    }

    if (nfc_svc_tag_detected()) {
        haptic_buzz();
        if (lbl_uid) {
            char b[72]; snprintf(b, sizeof(b), "UID: %s", nfc_svc_last_uid());
            lv_label_set_text(lbl_uid, b);
        }
        if (lbl_tag_type) {
            char b[48]; snprintf(b, sizeof(b), "TYPE: %s", nfc_svc_last_type());
            lv_label_set_text(lbl_tag_type, b);
        }
        if (lbl_ndef) lv_label_set_text(lbl_ndef, nfc_svc_last_ndef());
        if (lbl_scan_status) {
            lv_label_set_text(lbl_scan_status, "TAG FOUND!");
            lv_obj_set_style_text_color(lbl_scan_status, G, 0);
        }
    }

    
    if (nfc_svc_is_dict_attacking()) {
        if (lbl_dict_progress) {
            char b[80]; snprintf(b, sizeof(b), "%s | S:%d/%d",
                nfc_svc_dict_status_text(),
                nfc_svc_dict_sectors_found(),
                nfc_svc_dict_sectors_total());
            lv_label_set_text(lbl_dict_progress, b);
        }
        if (bar_dict) {
            lv_obj_remove_flag(bar_dict, LV_OBJ_FLAG_HIDDEN);
            int total = nfc_svc_dict_keys_total();
            int tried = nfc_svc_dict_keys_tried();
            lv_bar_set_value(bar_dict, total > 0 ? (tried * 100 / total) : 0, LV_ANIM_ON);
        }
    } else {
        if (bar_dict) lv_obj_add_flag(bar_dict, LV_OBJ_FLAG_HIDDEN);
    }

    
    update_saved_list();

    
    if (lbl_emu_status) {
        if (nfc_svc_is_emulating()) {
            lv_label_set_text(lbl_emu_status, "BROADCASTING...");
            lv_obj_set_style_text_color(lbl_emu_status, lv_color_hex(0xFF6600), 0);
        } else {
            lv_label_set_text(lbl_emu_status, "IDLE");
            lv_obj_set_style_text_color(lbl_emu_status, D, 0);
        }
    }
    if (lbl_emu_selected) {
        int sel = nfc_svc_selected_idx();
        int cnt = nfc_svc_saved_count();
        if (sel >= 0 && sel < cnt) {
            char b[64]; snprintf(b, sizeof(b), "SELECTED: %s", nfc_svc_tag_name(sel));
            lv_label_set_text(lbl_emu_selected, b);
        } else {
            lv_label_set_text(lbl_emu_selected, "SELECT A TAG FIRST");
        }
    }

    
    if (nfc_svc_emv_found()) {
        if (lbl_emv_status) lv_label_set_text(lbl_emv_status, "CARD READ OK");
        if (lbl_emv_brand) lv_label_set_text(lbl_emv_brand, nfc_svc_emv_brand());
        if (lbl_emv_pan) lv_label_set_text(lbl_emv_pan, nfc_svc_emv_pan());
        if (lbl_emv_exp) {
            char b[32]; snprintf(b, sizeof(b), "EXP: %s", nfc_svc_emv_expiry());
            lv_label_set_text(lbl_emv_exp, b);
        }
    } else if (nfc_svc_is_emv_reading()) {
        if (lbl_emv_status) lv_label_set_text(lbl_emv_status, "HOLD CARD NEAR...");
    }
}

void nfc_app_destroy(void) {
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    tabview = nullptr;
    lbl_scan_status = lbl_uid = lbl_tag_type = lbl_ndef = nullptr;
    lbl_dict_progress = nullptr;
    bar_dict = nullptr;
    lbl_saved_list = nullptr;
    lbl_emu_status = lbl_emu_selected = nullptr;
    lbl_emv_status = lbl_emv_pan = lbl_emv_exp = lbl_emv_brand = nullptr;
}
