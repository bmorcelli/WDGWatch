#include "recon_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/recon_service.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_results = nullptr;

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

static void wifi_scan_cb(lv_event_t *e) { (void)e; haptic_click(); recon_request_wifi_scan(); }
static void ble_scan_cb(lv_event_t *e)  { (void)e; haptic_click(); recon_request_ble_scan(10); }
static void stop_cb(lv_event_t *e)      { (void)e; haptic_click(); recon_request_stop(); }

void recon_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    int x = SAFE_LEFT + 5, y = SAFE_TOP;
    int bw = 113, bh = 48;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ RECON ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 25;
    lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, "READY");
    lv_obj_set_style_text_color(lbl_status, G, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_status, x, y);

    y += 22;
    make_btn(scr, x, y, bw, bh, LV_SYMBOL_WIFI " WiFi", wifi_scan_cb);
    make_btn(scr, x+bw+7, y, bw, bh, LV_SYMBOL_BLUETOOTH " BLE", ble_scan_cb);
    make_btn(scr, x+2*(bw+7), y, bw, bh, LV_SYMBOL_CLOSE " STOP", stop_cb);

    y += bh + 10;
    lbl_results = lv_label_create(scr);
    lv_label_set_text(lbl_results, "Tap WiFi or BLE to scan\n\nDeauth via web interface only");
    lv_obj_set_style_text_color(lbl_results, D, 0);
    lv_obj_set_style_text_font(lbl_results, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_results, x, y);
    lv_obj_set_width(lbl_results, 360);
    lv_label_set_long_mode(lbl_results, LV_LABEL_LONG_WRAP);
}

void recon_app_update(void) {
    if (!scr || !lbl_status || !lbl_results) return;

    if (recon_is_scanning()) {
        lv_label_set_text(lbl_status, "SCANNING...");

        char buf[1024] = "";
        int pos = 0;
        int wc = recon_wifi_count();
        int bc = recon_ble_count();

        if (wc > 0) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "WiFi: %d networks\n", wc);
            for (int i = 0; i < wc && i < 10 && pos < 900; i++) {
                const ReconWiFi *n = recon_get_wifi(i);
                if (n) pos += snprintf(buf+pos, sizeof(buf)-pos, " %d. %s [%d] CH%d\n",
                    i+1, n->ssid, n->rssi, n->channel);
            }
            if (wc > 10) pos += snprintf(buf+pos, sizeof(buf)-pos, " ...+%d more\n", wc-10);
        }
        if (bc > 0) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "\nBLE: %d devices\n", bc);
            for (int i = 0; i < bc && i < 10 && pos < 900; i++) {
                const BleDevice *d = recon_get_ble(i);
                if (d) pos += snprintf(buf+pos, sizeof(buf)-pos, " %s %s [%d]%s\n",
                    d->mac, d->name[0] ? d->name : "?", d->rssi,
                    d->is_airtag ? " AIRTAG!" : "");
            }
        }
        if (pos > 0) lv_label_set_text(lbl_results, buf);
    } else if (recon_is_deauthing()) {
        lv_label_set_text(lbl_status, "DEAUTH ACTIVE");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3300), 0);
    } else {
        if (recon_wifi_count() > 0 || recon_ble_count() > 0) {
            char b[32]; snprintf(b, sizeof(b), "DONE: %d WiFi, %d BLE",
                recon_wifi_count(), recon_ble_count());
            lv_label_set_text(lbl_status, b);
        }
        lv_obj_set_style_text_color(lbl_status, G, 0);
    }
}

void recon_app_destroy(void) {
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_status = lbl_results = nullptr;
}
