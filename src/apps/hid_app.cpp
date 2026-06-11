#include "app_common.h"
#include "hid_app.h"
#include <LilyGoLib.h>
#include <SD.h>
#include "../config.h"
#include "../hal/hid_service.h"
#include "../hal/haptic.h"

#define G       lv_color_hex(0x00E5FF)
#define D       lv_color_hex(0x007280)
#define RED lv_color_hex(0xFF3B3B)
#define BG  lv_color_hex(0x000000)

#define SD_SCRIPT_DIR "/badusb"

static lv_obj_t *scr           = nullptr;
static lv_obj_t *script_list   = nullptr;
static lv_obj_t *lbl_status    = nullptr;
static lv_obj_t *lbl_conn      = nullptr;
static lv_obj_t *lbl_airmouse  = nullptr;
static lv_obj_t *btn_title     = nullptr;
static lv_obj_t *lbl_title     = nullptr;
static lv_obj_t *btn_layout    = nullptr;
static lv_obj_t *btn_badusb    = nullptr;
static lv_obj_t *btn_badble    = nullptr;
static lv_obj_t *btn_airmouse  = nullptr;
static lv_obj_t *btn_mouse_left  = nullptr;
static lv_obj_t *btn_mouse_right = nullptr;
static lv_obj_t *scroll_area     = nullptr;

static bool show_badusb_list   = false;
static bool show_badble_list   = false;
static bool show_layout_list   = false;

#define MAX_SCRIPTS 16
static char script_names[MAX_SCRIPTS][64];
static int  script_count = 0;
static bool is_ble_mode  = false;

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

static lv_obj_t* section_label(lv_obj_t *par, int x, int y, const char *txt) {
    lv_obj_t *l = lv_label_create(par);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, D, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t* divider(lv_obj_t *par, int y) {
    lv_obj_t *line = lv_obj_create(par);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, 1);
    lv_obj_set_style_bg_color(line, D, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_pos(line, SAFE_LEFT, y);
    return line;
}

static void load_scripts_from_sd(void) {
    script_count = 0;
    if (!SD.exists(SD_SCRIPT_DIR)) {
        Serial.println("[HID] /badusb not found on SD");
        return;
    }
    File dir = SD.open(SD_SCRIPT_DIR);
    if (!dir || !dir.isDirectory()) return;

    File f = dir.openNextFile();
    while (f && script_count < MAX_SCRIPTS) {
        if (!f.isDirectory()) {
            String name = f.name();
            if (name.endsWith(".txt") || name.endsWith(".ducky") || name.endsWith(".duck")) {
                strncpy(script_names[script_count], name.c_str(), 63);
                script_count++;
            }
        }
        f = dir.openNextFile();
    }
    dir.close();
}

static void destroy_list_popup(void) {
    if (script_list) {
        lv_obj_delete(script_list);
        script_list = nullptr;
    }
    show_badusb_list = false;
    show_badble_list = false;
    show_layout_list = false;
}

static void layout_select_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= KB_LAYOUT_COUNT) return;

    hid_svc_set_layout((KeyboardLayoutId)idx);
    haptic_click();

    if (btn_layout) {
        lv_obj_t *lbl = lv_obj_get_child(btn_layout, 0);
        if (lbl) {
            char buf[32];
            snprintf(buf, sizeof(buf), "KEYBOARD: %s", hid_svc_get_layout_name((KeyboardLayoutId)idx));
            lv_label_set_text(lbl, buf);
        }
    }

    destroy_list_popup();
}

static void show_layout_selector(void) {
    if (script_list) destroy_list_popup();

    script_list = lv_obj_create(scr);
    lv_obj_set_size(script_list, SCREEN_WIDTH - SAFE_LEFT * 2, SCREEN_HEIGHT - SAFE_TOP - SAFE_BOTTOM - 60);
    lv_obj_set_pos(script_list, SAFE_LEFT, SAFE_TOP + 55);
    lv_obj_set_style_bg_color(script_list, lv_color_hex(0x001010), 0);
    lv_obj_set_style_border_color(script_list, G, 0);
    lv_obj_set_style_border_width(script_list, 1, 0);
    lv_obj_set_style_radius(script_list, 0, 0);
    lv_obj_set_scrollbar_mode(script_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(script_list, LV_DIR_VER);
    lv_obj_set_flex_flow(script_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(script_list, 4, 0);

    lv_obj_t *title = lv_label_create(script_list);
    lv_label_set_text(title, "SELECT KEYBOARD LAYOUT");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    for (int i = 0; i < KB_LAYOUT_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(script_list);
        lv_obj_set_size(btn, LV_PCT(100), 36);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_add_event_cb(btn, layout_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, hid_svc_get_layout_name((KeyboardLayoutId)i));
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_center(lbl);

        if ((KeyboardLayoutId)i == hid_svc_get_layout()) {
            lv_obj_set_style_bg_color(btn, G, 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(btn, G, 0);
            lv_obj_set_style_text_color(lbl, BG, 0);
        } else {
            lv_obj_set_style_bg_color(btn, BG, 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(btn, D, 0);
            lv_obj_set_style_text_color(lbl, G, 0);
        }
    }

    lv_obj_t *close_btn = lv_button_create(script_list);
    lv_obj_set_size(close_btn, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x1A0000), 0);
    lv_obj_set_style_border_color(close_btn, D, 0);
    lv_obj_set_style_border_width(close_btn, 1, 0);
    lv_obj_set_style_radius(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, [](lv_event_t *e) { (void)e; destroy_list_popup(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cl = lv_label_create(close_btn);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE "  CANCEL");
    lv_obj_set_style_text_color(cl, D, 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_16, 0);
    lv_obj_center(cl);

    show_layout_list = true;
}

static void script_file_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= script_count) return;

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", SD_SCRIPT_DIR, script_names[idx]);

    destroy_list_popup();
    haptic_click();

    if (!hid_svc_is_active() && !hid_svc_is_usb_connected()) {
        hid_svc_start();
    }

    hid_svc_run_script(full_path, is_ble_mode);

    if (lbl_status) {
        char buf[80];
        snprintf(buf, sizeof(buf), "Running: %s", script_names[idx]);
        lv_label_set_text(lbl_status, buf);
        lv_obj_set_style_text_color(lbl_status, G, 0);
    }
}

static void show_script_list(bool ble_mode) {
    if (script_list) destroy_list_popup();

    is_ble_mode = ble_mode;
    load_scripts_from_sd();

    script_list = lv_obj_create(scr);
    lv_obj_set_size(script_list, SCREEN_WIDTH - SAFE_LEFT * 2, SCREEN_HEIGHT - SAFE_TOP - SAFE_BOTTOM - 60);
    lv_obj_set_pos(script_list, SAFE_LEFT, SAFE_TOP + 55);
    lv_obj_set_style_bg_color(script_list, lv_color_hex(0x001010), 0);
    lv_obj_set_style_border_color(script_list, G, 0);
    lv_obj_set_style_border_width(script_list, 1, 0);
    lv_obj_set_style_radius(script_list, 0, 0);
    lv_obj_set_scrollbar_mode(script_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(script_list, LV_DIR_VER);
    lv_obj_set_flex_flow(script_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(script_list, 4, 0);

    lv_obj_t *title = lv_label_create(script_list);
    lv_label_set_text(title, ble_mode ? "SELECT SCRIPT — BAD BLE" : "SELECT SCRIPT — BAD USB");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    if (script_count == 0) {
        lv_obj_t *empty = lv_label_create(script_list);
        lv_label_set_text(empty, "No .txt files found\non SD /badusb/");
        lv_obj_set_style_text_color(empty, D, 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
    } else {
        for (int i = 0; i < script_count; i++) {
            lv_obj_t *btn = lv_button_create(script_list);
            lv_obj_set_size(btn, LV_PCT(100), 36);
            lv_obj_set_style_bg_color(btn, BG, 0);
            lv_obj_set_style_border_color(btn, D, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_radius(btn, 0, 0);
            lv_obj_add_event_cb(btn, script_file_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, script_names[i]);
            lv_obj_set_style_text_color(lbl, G, 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_width(lbl, LV_PCT(90));
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);
        }
    }

    lv_obj_t *close_btn = lv_button_create(script_list);
    lv_obj_set_size(close_btn, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x1A0000), 0);
    lv_obj_set_style_border_color(close_btn, D, 0);
    lv_obj_set_style_border_width(close_btn, 1, 0);
    lv_obj_set_style_radius(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, [](lv_event_t *e) { (void)e; destroy_list_popup(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cl = lv_label_create(close_btn);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE "  CANCEL");
    lv_obj_set_style_text_color(cl, D, 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_16, 0);
    lv_obj_center(cl);

    if (ble_mode) show_badble_list = true;
    else          show_badusb_list = true;
}

static void title_click_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (hid_svc_is_active()) {
        hid_svc_stop();
    } else {
        hid_svc_start();
    }
}

static void layout_click_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    show_layout_selector();
}

static void badusb_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    show_script_list(false);
}

static void badble_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    show_script_list(true);
}

static void vol_up_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    hid_media_vol_up();
}

static void vol_down_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    hid_media_vol_down();
}

static void screenshot_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    hid_media_screenshot();
}

static void mouse_left_cb(lv_event_t *e) {
    (void)e; haptic_click();
    hid_mouse_click(0x01);
}

static void mouse_right_cb(lv_event_t *e) {
    (void)e; haptic_click();
    hid_mouse_click(0x02);
}

static void scroll_area_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        haptic_click();
        hid_mouse_scroll(1);
    } else if (code == LV_EVENT_LONG_PRESSED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        haptic_click();
        hid_mouse_scroll(-1);
    }
}

static void airmouse_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (hid_airmouse_is_active()) {
        hid_airmouse_stop();
    } else {
        hid_airmouse_start();
    }
}

void hid_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);
    app_add_back_button(scr);

    int x  = SAFE_LEFT + 5;
    int cw = SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT - 10;
    int y  = SAFE_TOP;

    btn_title = lv_button_create(scr);
    lv_obj_set_size(btn_title, 200, 32);
    lv_obj_set_pos(btn_title, (SCREEN_WIDTH - 200) / 2, y);
    lv_obj_set_style_bg_color(btn_title, BG, 0);
    lv_obj_set_style_border_color(btn_title, G, 0);
    lv_obj_set_style_border_width(btn_title, 1, 0);
    lv_obj_set_style_radius(btn_title, 0, 0);
    lv_obj_add_event_cb(btn_title, title_click_cb, LV_EVENT_CLICKED, nullptr);

    lbl_title = lv_label_create(btn_title);
    lv_label_set_text(lbl_title, "[ HID CONTROLLER ]");
    lv_obj_set_style_text_color(lbl_title, G, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_title);
    y += 38;

    lbl_conn = lv_label_create(scr);
    lv_label_set_text(lbl_conn, LV_SYMBOL_BLUETOOTH " NOT CONNECTED");
    lv_obj_set_style_text_color(lbl_conn, D, 0);
    lv_obj_set_style_text_font(lbl_conn, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_conn, LV_ALIGN_TOP_MID, 0, y);
    y += 18;

    divider(scr, y); y += 8;

    section_label(scr, x, y, LV_SYMBOL_USB "  BAD USB / BAD BLE");

    char kb_buf[32];
    snprintf(kb_buf, sizeof(kb_buf), "KEYBOARD: %s", hid_svc_get_layout_name(hid_svc_get_layout()));
    btn_layout = make_btn(scr, x + cw - 140, y - 4, 140, 26, kb_buf, layout_click_cb);
    lv_obj_set_style_text_font(lv_obj_get_child(btn_layout, 0), &lv_font_montserrat_14, 0);
    y += 24;

    btn_badusb = make_btn(scr, x, y, cw / 2 - 4, 40,
             LV_SYMBOL_USB "\nBAD USB", badusb_cb);
    btn_badble = make_btn(scr, x + cw / 2 + 4, y, cw / 2 - 4, 40,
             LV_SYMBOL_BLUETOOTH "\nBAD BLE", badble_cb);
    y += 50;

    lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, "SD: /badusb/*.txt");
    lv_obj_set_style_text_color(lbl_status, D, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_status, x, y);
    y += 18;

    divider(scr, y); y += 8;
    section_label(scr, x, y, LV_SYMBOL_AUDIO "  MEDIA CONTROL");
    y += 20;

    int bw = (cw - 8) / 3;
    make_btn(scr, x,            y, bw, 40, LV_SYMBOL_VOLUME_MAX "\nVol +", vol_up_cb);
    make_btn(scr, x + bw + 4,  y, bw, 40, LV_SYMBOL_MUTE "\nVol -",       vol_down_cb);
    make_btn(scr, x + (bw+4)*2, y, bw, 40, LV_SYMBOL_IMAGE "\nSS",        screenshot_cb);
    y += 50;

    divider(scr, y); y += 8;
    section_label(scr, x, y, LV_SYMBOL_SETTINGS "  AIR MOUSE");
    y += 20;

    btn_airmouse = make_btn(scr, x, y, cw, 40,
             LV_SYMBOL_EYE_OPEN "  AIR MOUSE — START / STOP", airmouse_cb);
    y += 45;

    btn_mouse_left = make_btn(scr, x, y, 95, 45, "LEFT", mouse_left_cb);

    scroll_area = lv_obj_create(scr);
    lv_obj_set_size(scroll_area, 130, 45);
    lv_obj_set_pos(scroll_area, x + 95 + 10, y);
    lv_obj_set_style_bg_color(scroll_area, BG, 0);
    lv_obj_set_style_border_color(scroll_area, G, 0);
    lv_obj_set_style_border_width(scroll_area, 1, 0);
    lv_obj_set_style_radius(scroll_area, 0, 0);
    lv_obj_set_style_bg_color(scroll_area, G, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(scroll_area, 40, LV_STATE_PRESSED);
    lv_obj_add_flag(scroll_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(scroll_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scroll_area, scroll_area_cb, LV_EVENT_ALL, nullptr);

    lv_obj_t *lbl_scroll = lv_label_create(scroll_area);
    lv_label_set_text(lbl_scroll, "SCROLL");
    lv_obj_set_style_text_color(lbl_scroll, G, 0);
    lv_obj_set_style_text_font(lbl_scroll, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_scroll);

    btn_mouse_right = make_btn(scr, x + 95 + 10 + 130 + 10, y, 95, 45, "RIGHT", mouse_right_cb);

    lv_obj_set_style_bg_color(btn_mouse_left, BG, LV_STATE_DISABLED);
    lv_obj_set_style_border_color(btn_mouse_left, D, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(lv_obj_get_child(btn_mouse_left, 0), D, LV_STATE_DISABLED);

    lv_obj_set_style_bg_color(scroll_area, BG, LV_STATE_DISABLED);
    lv_obj_set_style_border_color(scroll_area, D, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(lbl_scroll, D, LV_STATE_DISABLED);

    lv_obj_set_style_bg_color(btn_mouse_right, BG, LV_STATE_DISABLED);
    lv_obj_set_style_border_color(btn_mouse_right, D, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(lv_obj_get_child(btn_mouse_right, 0), D, LV_STATE_DISABLED);

    y += 50;

    lbl_airmouse = lv_label_create(scr);
    lv_label_set_text(lbl_airmouse, "Gyro mouse  |  OFF");
    lv_obj_set_style_text_color(lbl_airmouse, D, 0);
    lv_obj_set_style_text_font(lbl_airmouse, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_airmouse, x, y);
}

void hid_app_update(void) {
    if (!scr) return;

    bool usb_conn = hid_svc_is_usb_connected();

    if (usb_conn) {

        if (btn_title) {
            lv_obj_set_style_bg_color(btn_title, D, 0);
            lv_obj_set_style_bg_opa(btn_title, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(btn_title, D, 0);
        }
        if (lbl_title) {
            lv_obj_set_style_text_color(lbl_title, BG, 0);
        }
        if (lbl_conn) {
            lv_label_set_text(lbl_conn, LV_SYMBOL_USB " USB CONNECTED");
            lv_obj_set_style_text_color(lbl_conn, D, 0);
        }
    } else {

        if (hid_svc_is_connected() || hid_svc_is_active()) {
            if (btn_title) {
                lv_obj_set_style_bg_color(btn_title, G, 0);
                lv_obj_set_style_bg_opa(btn_title, LV_OPA_COVER, 0);
                lv_obj_set_style_border_color(btn_title, G, 0);
            }
            if (lbl_title) {
                lv_obj_set_style_text_color(lbl_title, BG, 0);
            }
        } else {
            if (btn_title) {
                lv_obj_set_style_bg_color(btn_title, BG, 0);
                lv_obj_set_style_bg_opa(btn_title, LV_OPA_COVER, 0);
                lv_obj_set_style_border_color(btn_title, G, 0);
            }
            if (lbl_title) {
                lv_obj_set_style_text_color(lbl_title, G, 0);
            }
        }
        if (lbl_conn) {
            if (hid_svc_is_connected()) {
                lv_label_set_text(lbl_conn, LV_SYMBOL_BLUETOOTH " BLE CONNECTED");
                lv_obj_set_style_text_color(lbl_conn, G, 0);
            } else if (hid_svc_is_active()) {
                lv_label_set_text(lbl_conn, LV_SYMBOL_BLUETOOTH " ADVERTISING...");
                lv_obj_set_style_text_color(lbl_conn, G, 0);
            } else {
                lv_label_set_text(lbl_conn, LV_SYMBOL_BLUETOOTH " NOT CONNECTED");
                lv_obj_set_style_text_color(lbl_conn, D, 0);
            }
        }
    }

    if (lbl_status) {
        if (hid_svc_is_running_script()) {
            lv_label_set_text(lbl_status, LV_SYMBOL_PLAY " SCRIPT RUNNING...");
            lv_obj_set_style_text_color(lbl_status, RED, 0);
        } else {
            static bool was_running = false;
            if (was_running) {
                lv_label_set_text(lbl_status, "Script finished.");
                lv_obj_set_style_text_color(lbl_status, G, 0);
            }
            was_running = hid_svc_is_running_script();
        }
    }

    if (btn_badusb) {
        lv_obj_t *lbl = lv_obj_get_child(btn_badusb, 0);
        if (hid_svc_is_running_script() && !is_ble_mode) {
            lv_obj_set_style_bg_color(btn_badusb, RED, 0);
            lv_obj_set_style_bg_opa(btn_badusb, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, BG, 0);
        } else {
            lv_obj_set_style_bg_color(btn_badusb, BG, 0);
            lv_obj_set_style_bg_opa(btn_badusb, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, G, 0);
        }
    }
    if (btn_badble) {
        lv_obj_t *lbl = lv_obj_get_child(btn_badble, 0);
        if (hid_svc_is_running_script() && is_ble_mode) {
            lv_obj_set_style_bg_color(btn_badble, RED, 0);
            lv_obj_set_style_bg_opa(btn_badble, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, BG, 0);
        } else {
            lv_obj_set_style_bg_color(btn_badble, BG, 0);
            lv_obj_set_style_bg_opa(btn_badble, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, G, 0);
        }
    }

    if (lbl_airmouse) {
        if (hid_airmouse_is_active()) {
            lv_label_set_text(lbl_airmouse, LV_SYMBOL_EYE_OPEN " Gyro mouse  |  ACTIVE");
            lv_obj_set_style_text_color(lbl_airmouse, G, 0);
        } else {
            lv_label_set_text(lbl_airmouse, "Gyro mouse  |  OFF");
            lv_obj_set_style_text_color(lbl_airmouse, D, 0);
        }
    }
    if (btn_airmouse) {
        lv_obj_t *lbl = lv_obj_get_child(btn_airmouse, 0);
        if (hid_airmouse_is_active()) {
            lv_obj_set_style_bg_color(btn_airmouse, G, 0);
            lv_obj_set_style_bg_opa(btn_airmouse, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, BG, 0);
        } else {
            lv_obj_set_style_bg_color(btn_airmouse, BG, 0);
            lv_obj_set_style_bg_opa(btn_airmouse, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, G, 0);
        }
    }

    bool airmouse_on = hid_airmouse_is_active();
    if (btn_mouse_left && btn_mouse_right && scroll_area) {
        if (airmouse_on) {
            lv_obj_remove_state(btn_mouse_left, LV_STATE_DISABLED);
            lv_obj_remove_state(btn_mouse_right, LV_STATE_DISABLED);
            lv_obj_remove_state(scroll_area, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_mouse_left, LV_STATE_DISABLED);
            lv_obj_add_state(btn_mouse_right, LV_STATE_DISABLED);
            lv_obj_add_state(scroll_area, LV_STATE_DISABLED);
        }
    }
}

void hid_app_destroy(void) {
    if (hid_airmouse_is_active()) hid_airmouse_stop();
    destroy_list_popup();
    lbl_status    = nullptr;
    lbl_conn      = nullptr;
    lbl_airmouse  = nullptr;
    btn_title     = nullptr;
    lbl_title     = nullptr;
    btn_layout    = nullptr;
    btn_badusb    = nullptr;
    btn_badble    = nullptr;
    btn_airmouse  = nullptr;
    btn_mouse_left = nullptr;
    btn_mouse_right = nullptr;
    scroll_area   = nullptr;
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
}
