#include "wifi_app.h"
#include <WiFi.h>
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/time_sync.h"
#include "../web/web_server.h"
#include "../hal/ble_uart_service.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_ip = nullptr;
static lv_obj_t *lbl_clients = nullptr;

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
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

static volatile bool web_toggle_requested = false;
static volatile bool ble_toggle_requested = false;
static lv_obj_t *lbl_ble_status = nullptr;

static void toggle_web_cb(lv_event_t *e) {
    (void)e; haptic_click();
    web_toggle_requested = true;
}

static void toggle_ble_cb(lv_event_t *e) {
    (void)e; haptic_click();
    ble_toggle_requested = true;
}

static void ntp_cb(lv_event_t *e) {
    (void)e; haptic_click();
    time_sync_force_retry();
    if (lbl_status) lv_label_set_text(lbl_status, "NTP SYNC REQUESTED...");
}

void wifi_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    int x = SAFE_LEFT + 10;
    int y = SAFE_TOP;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ WiFi / WEB ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 30;
    lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, web_server_is_active() ? "WEB SERVER ON" : "WEB SERVER OFF");
    lv_obj_set_style_text_color(lbl_status, G, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_status, x, y);

    y += 22;
    lbl_ip = lv_label_create(scr);
    if (web_server_is_active()) {
        char b[48]; snprintf(b, sizeof(b), "AP: PipBoy-3000\nIP: %s", web_server_get_ip());
        lv_label_set_text(lbl_ip, b);
    } else {
        lv_label_set_text(lbl_ip, "");
    }
    lv_obj_set_style_text_color(lbl_ip, D, 0);
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_ip, x, y);
    lv_obj_set_width(lbl_ip, 340);
    lv_label_set_long_mode(lbl_ip, LV_LABEL_LONG_WRAP);

    y += 40;
    lbl_clients = lv_label_create(scr);
    lv_label_set_text(lbl_clients, "");
    lv_obj_set_style_text_color(lbl_clients, D, 0);
    lv_obj_set_style_text_font(lbl_clients, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_clients, x, y);

    y += 20;
    int bw = 340, bh = 65;
    make_btn(scr, x, y, bw, bh, LV_SYMBOL_WIFI " WEB SERVER ON / OFF", toggle_web_cb);
    y += bh + 10;
    make_btn(scr, x, y, bw, 50, LV_SYMBOL_BLUETOOTH " Watch Dogs Connect", toggle_ble_cb);

    y += 58;
    lbl_ble_status = lv_label_create(scr);
    lv_label_set_text(lbl_ble_status, ble_uart_is_connected() ? "BLE: CONNECTED" : "BLE: OFF");
    lv_obj_set_style_text_color(lbl_ble_status, D, 0);
    lv_obj_set_style_text_font(lbl_ble_status, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_ble_status, x, y);

    y += 20;
    make_btn(scr, x, y, bw, 45, LV_SYMBOL_REFRESH " NTP SYNC", ntp_cb);

    y += bh + 15;
    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "1. Tap WEB ON/OFF\n2. Connect phone to\n   WiFi: PipBoy-3000\n   Pass: pip12345\n3. Open 192.168.4.1");
    lv_obj_set_style_text_color(hint, D, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(hint, x, y);
    lv_obj_set_width(hint, 340);
}

void wifi_app_update(void) {
    if (!scr) return;

    if (web_toggle_requested) {
        web_toggle_requested = false;
        if (web_server_is_active()) {
            web_server_stop();
            if (lbl_status) lv_label_set_text(lbl_status, "WEB SERVER OFF");
            if (lbl_ip) lv_label_set_text(lbl_ip, "");
            if (lbl_clients) lv_label_set_text(lbl_clients, "");
        } else {
            if (lbl_status) lv_label_set_text(lbl_status, "STARTING AP...");
            lv_timer_handler();
            web_server_init();
            if (lbl_status) lv_label_set_text(lbl_status, "WEB SERVER ON");
            if (lbl_ip) {
                char b[48]; snprintf(b, sizeof(b), "AP: PipBoy-3000\nIP: %s", web_server_get_ip());
                lv_label_set_text(lbl_ip, b);
            }
        }
    }

    if (ble_toggle_requested) {
        ble_toggle_requested = false;
        if (ble_uart_is_active()) {
            ble_uart_stop();
        } else {
            ble_uart_init();
        }
    }

    if (lbl_ble_status) {
        if (!ble_uart_is_active())
            lv_label_set_text(lbl_ble_status, "BLE: OFF");
        else if (ble_uart_is_connected())
            lv_label_set_text(lbl_ble_status, "BLE: CONNECTED");
        else
            lv_label_set_text(lbl_ble_status, "BLE: ADVERTISING...");
    }

    if (lbl_clients && web_server_is_active()) {
        char b[32]; snprintf(b, sizeof(b), "Clients: %d", WiFi.softAPgetStationNum());
        lv_label_set_text(lbl_clients, b);
    }
}

void wifi_app_destroy(void) {

    if (scr) { lv_obj_delete(scr); scr = nullptr; }
}
