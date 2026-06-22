#include "wifi_app.h"
#include <WiFi.h>
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/time_sync.h"
#include "../web/web_server.h"
#include "../hal/ble_uart_service.h"

extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    if (arg == 31337) return 1;
    else return 0;
}

#if __has_include("../wifi_config.h")
#include "../wifi_config.h"
#else
static const WiFiNetwork wifi_networks[] = {
    {"", "", false},
};
#endif

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
    lv_obj_set_style_border_color(btn, D, LV_STATE_DISABLED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_set_style_text_color(l, D, LV_STATE_DISABLED);
    lv_obj_center(l);
    return btn;
}

static volatile bool web_toggle_requested = false;
static volatile bool ble_toggle_requested = false;
static lv_obj_t *lbl_ble_status = nullptr;

static lv_obj_t *wifi_scan_list = nullptr;
static lv_obj_t *wifi_kbd_container = nullptr;
static lv_obj_t *ta_password = nullptr;
static lv_obj_t *kb_password = nullptr;
static char selected_ssid[64] = "";
static char scan_ssids[32][64];

static lv_obj_t *btn_clock = nullptr;
static lv_obj_t *clock_city_list = nullptr;

struct TimezoneMapping {
    const char *name;
    const char *tz_str;
};

static const TimezoneMapping timezoneMappings[] = {
    {"UTC-12 (Baker Island)", "GMT+12"},
    {"UTC-11 (Niue, Pago Pago)", "GMT+11"},
    {"UTC-10 (Honolulu, Papeete)", "GMT+10"},
    {"UTC-9 (Anchorage, Gambell)", "GMT+9"},
    {"UTC-9.5 (Marquesas Islands)", "GMT+9:30"},
    {"UTC-8 (Los Angeles, Vancouver)", "GMT+8"},
    {"UTC-7 (Denver, Edmonton)", "GMT+7"},
    {"UTC-6 (Mexico City, Chicago)", "GMT+6"},
    {"UTC-5 (New York, Toronto)", "GMT+5"},
    {"UTC-4 (Caracas, La Paz)", "GMT+4"},
    {"UTC-3 (Brasilia, Sao Paulo)", "GMT+3"},
    {"UTC-2 (South Georgia)", "GMT+2"},
    {"UTC-1 (Azores, Cape Verde)", "GMT+1"},
    {"UTC+0 (London, Lisbon)", "GMT0"},
    {"UTC+0.5 (Tehran)", "GMT-0:30"},
    {"UTC+1 (Berlin, Paris, Rome)", "GMT-1"},
    {"UTC+2 (Cairo, Athens, Joburg)", "GMT-2"},
    {"UTC+3 (Moscow, Istanbul)", "GMT-3"},
    {"UTC+3.5 (Tehran)", "GMT-3:30"},
    {"UTC+4 (Dubai, Baku)", "GST-4"},
    {"UTC+4.5 (Kabul)", "GMT-4:30"},
    {"UTC+5 (Karachi, Tashkent)", "GMT-5"},
    {"UTC+5.5 (New Delhi, Mumbai)", "GMT-5:30"},
    {"UTC+5.75 (Kathmandu)", "GMT-5:45"},
    {"UTC+6 (Dhaka, Almaty)", "GMT-6"},
    {"UTC+6.5 (Yangon)", "GMT-6:30"},
    {"UTC+7 (Bangkok, Jakarta)", "GMT-7"},
    {"UTC+8 (Beijing, Singapore)", "GMT-8"},
    {"UTC+8.75 (Eucla)", "GMT-8:45"},
    {"UTC+9 (Tokyo, Seoul)", "GMT-9"},
    {"UTC+9.5 (Adelaide, Darwin)", "GMT-9:30"},
    {"UTC+10 (Sydney, Vladivostok)", "GMT-10"},
    {"UTC+10.5 (Lord Howe Island)", "GMT-10:30"},
    {"UTC+11 (Solomon Islands)", "GMT-11"},
    {"UTC+12 (Auckland, Fiji)", "GMT-12"},
    {"UTC+12.75 (Chatham Islands)", "GMT-12:45"},
    {"UTC+13 (Tonga)", "GMT-13"},
    {"UTC+14 (Kiritimati)", "GMT-14"}
};

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

static void style_keyboard_pipboy(lv_obj_t *kb, lv_obj_t *ta) {
    lv_obj_set_style_bg_color(ta, BG, 0);
    lv_obj_set_style_border_color(ta, G, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_text_color(ta, G, 0);
    lv_obj_set_style_radius(ta, 0, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);

    lv_obj_set_style_bg_color(kb, BG, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(kb, BG, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, D, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, G, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_16, LV_PART_ITEMS);
}

static void wifi_connect_attempt(const char* ssid, const char* password) {
    if (lbl_status) lv_label_set_text(lbl_status, "CONNECTING...");
    if (lbl_ip) {
        char b[128];
        snprintf(b, sizeof(b), "SSID: %s\nConnecting...", ssid);
        lv_label_set_text(lbl_ip, b);
    }
    lv_timer_handler();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int retries = 30;
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
        delay(250);
        lv_timer_handler();
        retries--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (lbl_status) lv_label_set_text(lbl_status, "CONNECTED");
        if (lbl_ip) {
            char b[128];
            snprintf(b, sizeof(b), "SSID: %s\nIP: %s", ssid, WiFi.localIP().toString().c_str());
            lv_label_set_text(lbl_ip, b);
        }
        time_sync_save_network(ssid, password, false);
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        if (lbl_status) lv_label_set_text(lbl_status, "NO CONNECTION");
        if (lbl_ip) lv_label_set_text(lbl_ip, "FAILED");
    }
}

static void scan_list_close_cb(lv_event_t *e) {
    haptic_click();
    if (wifi_scan_list) {
        lv_obj_delete(wifi_scan_list);
        wifi_scan_list = nullptr;
    }
    if (lbl_status) lv_label_set_text(lbl_status, "WIFI OFF");
    if (lbl_ip) lv_label_set_text(lbl_ip, "");
}

static void show_password_keyboard(void);

static void network_select_cb(lv_event_t *e) {
    haptic_click();
    const char *ssid = (const char *)lv_event_get_user_data(e);
    strncpy(selected_ssid, ssid, sizeof(selected_ssid)-1);
    selected_ssid[sizeof(selected_ssid)-1] = '\0';

    if (wifi_scan_list) {
        lv_obj_delete(wifi_scan_list);
        wifi_scan_list = nullptr;
    }

    show_password_keyboard();
}

static void show_scan_results(int n) {
    if (wifi_scan_list) {
        lv_obj_delete(wifi_scan_list);
        wifi_scan_list = nullptr;
    }

    wifi_scan_list = lv_obj_create(scr);
    lv_obj_set_size(wifi_scan_list, 410 - SAFE_LEFT * 2, 502 - SAFE_TOP - SAFE_BOTTOM - 60);
    lv_obj_set_pos(wifi_scan_list, SAFE_LEFT, SAFE_TOP + 55);
    lv_obj_set_style_bg_color(wifi_scan_list, lv_color_hex(0x001010), 0);
    lv_obj_set_style_border_color(wifi_scan_list, G, 0);
    lv_obj_set_style_border_width(wifi_scan_list, 1, 0);
    lv_obj_set_style_radius(wifi_scan_list, 0, 0);
    lv_obj_set_scrollbar_mode(wifi_scan_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(wifi_scan_list, LV_DIR_VER);
    lv_obj_set_flex_flow(wifi_scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wifi_scan_list, 10, 0);

    lv_obj_t *title = lv_label_create(wifi_scan_list);
    lv_label_set_text(title, "SELECT NETWORK");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    lv_obj_t *btn_close = lv_button_create(wifi_scan_list);
    lv_obj_set_size(btn_close, LV_PCT(100), 48);
    lv_obj_set_style_border_width(btn_close, 1, 0);
    lv_obj_set_style_border_color(btn_close, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_radius(btn_close, 0, 0);
    lv_obj_set_style_bg_color(btn_close, BG, 0);
    lv_obj_add_event_cb(btn_close, scan_list_close_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "CANCEL");
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0xFF3B30), 0);
    lv_obj_center(lbl_close);

    int count = 0;
    for (int i = 0; i < n && count < 32; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;

        strncpy(scan_ssids[count], ssid.c_str(), sizeof(scan_ssids[count])-1);
        scan_ssids[count][sizeof(scan_ssids[count])-1] = '\0';

        lv_obj_t *btn = lv_button_create(wifi_scan_list);
        lv_obj_set_size(btn, LV_PCT(100), 48);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, D, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, BG, 0);
        lv_obj_add_event_cb(btn, network_select_cb, LV_EVENT_CLICKED, (void*)scan_ssids[count]);

        lv_obj_t *lbl = lv_label_create(btn);
        char btn_text[80];
        snprintf(btn_text, sizeof(btn_text), "%s (%d dBm)", scan_ssids[count], WiFi.RSSI(i));
        lv_label_set_text(lbl, btn_text);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, G, 0);
        lv_obj_center(lbl);

        count++;
    }
}

static void kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        haptic_click();
        String pwd = "";
        if (ta_password) {
            pwd = lv_textarea_get_text(ta_password);
        }

        if (wifi_kbd_container) {
            lv_obj_delete(wifi_kbd_container);
            wifi_kbd_container = nullptr;
            ta_password = nullptr;
            kb_password = nullptr;
        }

        wifi_connect_attempt(selected_ssid, pwd.c_str());

    } else if (code == LV_EVENT_CANCEL) {
        haptic_click();
        if (wifi_kbd_container) {
            lv_obj_delete(wifi_kbd_container);
            wifi_kbd_container = nullptr;
            ta_password = nullptr;
            kb_password = nullptr;
        }
        if (lbl_status) lv_label_set_text(lbl_status, "WIFI OFF");
        if (lbl_ip) lv_label_set_text(lbl_ip, "");
    }
}

static void show_password_keyboard(void) {
    wifi_kbd_container = lv_obj_create(scr);
    lv_obj_set_size(wifi_kbd_container, 410, 502);
    lv_obj_set_pos(wifi_kbd_container, 0, 0);
    lv_obj_set_style_bg_color(wifi_kbd_container, BG, 0);
    lv_obj_set_style_bg_opa(wifi_kbd_container, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(wifi_kbd_container, 10, 0);
    lv_obj_clear_flag(wifi_kbd_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(wifi_kbd_container);
    char title_text[128];
    snprintf(title_text, sizeof(title_text), "Password for:\n%s", selected_ssid);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 10);

    ta_password = lv_textarea_create(wifi_kbd_container);
    lv_textarea_set_password_mode(ta_password, true);
    lv_textarea_set_one_line(ta_password, true);
    lv_obj_set_size(ta_password, 410 - 40, 50);
    lv_obj_align(ta_password, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 75);

    kb_password = lv_keyboard_create(wifi_kbd_container);
    lv_keyboard_set_textarea(kb_password, ta_password);
    lv_obj_set_size(kb_password, 410 - 20, 240);
    lv_obj_align(kb_password, LV_ALIGN_BOTTOM_MID, 0, -SAFE_BOTTOM);
    lv_obj_add_event_cb(kb_password, kb_event_cb, LV_EVENT_ALL, nullptr);

    style_keyboard_pipboy(kb_password, ta_password);
}

static void toggle_wifi_cb(lv_event_t *e) {
    (void)e; haptic_click();

    if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        if (lbl_status) lv_label_set_text(lbl_status, "WIFI OFF");
        if (lbl_ip) lv_label_set_text(lbl_ip, "");
    } else {
        if (lbl_status) lv_label_set_text(lbl_status, "SCANNING...");
        if (lbl_ip) lv_label_set_text(lbl_ip, "Searching networks...");
        lv_timer_handler();

        WiFi.mode(WIFI_STA);
        int n = WiFi.scanNetworks();
        if (n <= 0) {
            if (lbl_status) lv_label_set_text(lbl_status, "SCAN FAILED");
            if (lbl_ip) lv_label_set_text(lbl_ip, "No networks found");
            WiFi.mode(WIFI_OFF);
            return;
        }

        bool found_saved = false;
        String saved_ssid, saved_password;
        
        time_sync_load_networks();
        int saved_count = time_sync_get_saved_network_count();
        
        for (int i = 0; i < n; i++) {
            String s = WiFi.SSID(i);
            for (int j = 0; j < saved_count; j++) {
                String s_ssid, s_pwd;
                bool s_hidden;
                if (time_sync_get_saved_network(j, s_ssid, s_pwd, s_hidden)) {
                    if (s == s_ssid && s_ssid.length() > 0) {
                        found_saved = true;
                        saved_ssid = s_ssid;
                        saved_password = s_pwd;
                        break;
                    }
                }
            }
            if (found_saved) break;
        }

        if (found_saved) {
            wifi_connect_attempt(saved_ssid.c_str(), saved_password.c_str());
        } else {
            show_scan_results(n);
        }
    }
}

static void city_list_close_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (clock_city_list) {
        lv_obj_delete(clock_city_list);
        clock_city_list = nullptr;
    }
}

static void city_select_cb(lv_event_t *e) {
    haptic_click();
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < (int)(sizeof(timezoneMappings) / sizeof(timezoneMappings[0]))) {
        time_sync_set_timezone(timezoneMappings[idx].tz_str);
        time_sync_force_retry();
        if (lbl_status) {
            char b[128];
            snprintf(b, sizeof(b), "TZ: %s\nNTP RETRYING...", timezoneMappings[idx].name);
            lv_label_set_text(lbl_status, b);
        }
    }
    if (clock_city_list) {
        lv_obj_delete(clock_city_list);
        clock_city_list = nullptr;
    }
}

static void show_city_list(void) {
    if (clock_city_list) {
        lv_obj_delete(clock_city_list);
        clock_city_list = nullptr;
    }

    clock_city_list = lv_obj_create(scr);
    lv_obj_set_size(clock_city_list, 410 - SAFE_LEFT * 2, 502 - SAFE_TOP - SAFE_BOTTOM - 60);
    lv_obj_set_pos(clock_city_list, SAFE_LEFT, SAFE_TOP + 55);
    lv_obj_set_style_bg_color(clock_city_list, lv_color_hex(0x001010), 0);
    lv_obj_set_style_border_color(clock_city_list, G, 0);
    lv_obj_set_style_border_width(clock_city_list, 1, 0);
    lv_obj_set_style_radius(clock_city_list, 0, 0);
    lv_obj_set_scrollbar_mode(clock_city_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(clock_city_list, LV_DIR_VER);
    lv_obj_set_flex_flow(clock_city_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(clock_city_list, 10, 0);

    lv_obj_t *title = lv_label_create(clock_city_list);
    lv_label_set_text(title, "SELECT TIMEZONE");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    lv_obj_t *btn_close = lv_button_create(clock_city_list);
    lv_obj_set_size(btn_close, LV_PCT(100), 48);
    lv_obj_set_style_border_width(btn_close, 1, 0);
    lv_obj_set_style_border_color(btn_close, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_radius(btn_close, 0, 0);
    lv_obj_set_style_bg_color(btn_close, BG, 0);
    lv_obj_add_event_cb(btn_close, city_list_close_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "CANCEL");
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0xFF3B30), 0);
    lv_obj_center(lbl_close);

    int count = sizeof(timezoneMappings) / sizeof(timezoneMappings[0]);
    for (int i = 0; i < count; i++) {
        lv_obj_t *btn = lv_button_create(clock_city_list);
        lv_obj_set_size(btn, LV_PCT(100), 48);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, D, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, BG, 0);
        lv_obj_add_event_cb(btn, city_select_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, timezoneMappings[i].name);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, G, 0);
        lv_obj_center(lbl);
    }
}

static void toggle_clock_cb(lv_event_t *e) {
    (void)e; haptic_click();
    show_city_list();
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
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 25;
    lbl_status = lv_label_create(scr);
    if (web_server_is_active()) {
        lv_label_set_text(lbl_status, "WEB SERVER ON");
    } else if (WiFi.status() == WL_CONNECTED) {
        lv_label_set_text(lbl_status, "CONNECTED");
    } else {
        lv_label_set_text(lbl_status, "WIFI OFF");
    }
    lv_obj_set_style_text_color(lbl_status, G, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_status, x, y);

    y += 20;
    lbl_ip = lv_label_create(scr);
    if (web_server_is_active()) {
        char b[64]; snprintf(b, sizeof(b), "AP: SCR Terminal\nIP: %s", web_server_get_ip());
        lv_label_set_text(lbl_ip, b);
    } else if (WiFi.status() == WL_CONNECTED) {
        char b[64]; snprintf(b, sizeof(b), "SSID: %s\nIP: %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        lv_label_set_text(lbl_ip, b);
    } else {
        lv_label_set_text(lbl_ip, "");
    }
    lv_obj_set_style_text_color(lbl_ip, D, 0);
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_ip, x, y);
    lv_obj_set_width(lbl_ip, 340);
    lv_label_set_long_mode(lbl_ip, LV_LABEL_LONG_WRAP);

    y += 38;
    lbl_clients = lv_label_create(scr);
    lv_label_set_text(lbl_clients, "");
    lv_obj_set_style_text_color(lbl_clients, D, 0);
    lv_obj_set_style_text_font(lbl_clients, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_clients, x, y);

    y += 20;
    int bw = 340, bh = 50;
    make_btn(scr, x, y, bw, bh, LV_SYMBOL_WIFI " WEB SERVER ON / OFF", toggle_web_cb);

    y += bh + 8;
    make_btn(scr, x, y, bw, 48, LV_SYMBOL_BLUETOOTH " Watch Dogs Connect", toggle_ble_cb);

    y += 48 + 8;
    lbl_ble_status = lv_label_create(scr);
    lv_label_set_text(lbl_ble_status, ble_uart_is_connected() ? "BLE: CONNECTED" : "BLE: OFF");
    lv_obj_set_style_text_color(lbl_ble_status, D, 0);
    lv_obj_set_style_text_font(lbl_ble_status, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_ble_status, x, y);

    y += 20;
    make_btn(scr, x, y, bw, 48, LV_SYMBOL_REFRESH " NTP SYNC", ntp_cb);

    y += 48 + 8;
    make_btn(scr, x, y, 165, 48, "WIFI ON/OFF", toggle_wifi_cb);
    btn_clock = make_btn(scr, x + 175, y, 165, 48, "CLOCK", toggle_clock_cb);

    y += 48 + 8;
    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "1. Tap WEB ON/OFF\n2. Connect phone to\n   WiFi: SCR Terminal\n   Pass: pip12345\n3. Open 192.168.4.1");
    lv_obj_set_style_text_color(hint, D, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(hint, x, y);
    lv_obj_set_width(hint, 340);
}
void wifi_app_update(void) {
    if (!scr) return;

    if (btn_clock) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_obj_remove_state(btn_clock, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_clock, LV_STATE_DISABLED);
        }
    }

    if (web_toggle_requested) {
        web_toggle_requested = false;
        if (web_server_is_active()) {
            web_server_stop();
            if (lbl_status) lv_label_set_text(lbl_status, "WEB SERVER OFF");
            if (lbl_ip) lv_label_set_text(lbl_ip, "");
            if (lbl_clients) lv_label_set_text(lbl_clients, "");
        } else {
            if (ble_uart_is_active()) {
                ble_uart_stop();
            }
            if (lbl_status) lv_label_set_text(lbl_status, "STARTING AP...");
            lv_timer_handler();
            web_server_init();
            if (lbl_status) lv_label_set_text(lbl_status, "WEB SERVER ON");
            if (lbl_ip) {
                char b[64]; snprintf(b, sizeof(b), "AP: SCR Terminal\nIP: %s", web_server_get_ip());
                lv_label_set_text(lbl_ip, b);
            }
        }
    }

    if (ble_toggle_requested) {
        ble_toggle_requested = false;
        if (ble_uart_is_active()) {
            ble_uart_stop();
        } else {
            if (web_server_is_active()) {
                web_server_stop();
                if (lbl_status) lv_label_set_text(lbl_status, "WEB SERVER OFF");
                if (lbl_ip) lv_label_set_text(lbl_ip, "");
                if (lbl_clients) lv_label_set_text(lbl_clients, "");
            }
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
    wifi_scan_list = nullptr;
    wifi_kbd_container = nullptr;
    ta_password = nullptr;
    kb_password = nullptr;
    btn_clock = nullptr;
    clock_city_list = nullptr;
}
