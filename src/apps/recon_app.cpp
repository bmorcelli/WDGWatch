#include "recon_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include <vector>
#include <string>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/recon_service.h"
#include <SD.h>

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_results = nullptr;

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static char et_sel_ssid[33] = "";
static char et_sel_bssid[18] = "";
static int et_sel_channel = 1;

static char beacon_names[BEACON_SSID_COUNT][BEACON_SSID_LEN];

static lv_obj_t* create_modal(const char* title_text) {
    lv_obj_t* modal = lv_obj_create(scr);
    lv_obj_set_size(modal, 390, 440);
    lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(modal, BG, 0);
    lv_obj_set_style_border_color(modal, G, 0);
    lv_obj_set_style_border_width(modal, 2, 0);
    lv_obj_set_style_radius(modal, 0, 0);
    
    lv_obj_t* lbl = lv_label_create(modal);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_color(lbl, G, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, -10);
    
    return modal;
}

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
static bool arp_scan_was_running = false;
static void stop_cb(lv_event_t *e) {
    (void)e; haptic_click();
    arp_scan_was_running = false;
    recon_request_stop();
}

static void ip_select_cb(lv_event_t *e) {
    haptic_click();
    const char* ip = (const char*)lv_event_get_user_data(e);
    recon_request_ip_sniff(ip);
    lv_obj_t* item = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* modal = lv_obj_get_parent(lv_obj_get_parent(item));
    lv_obj_delete(modal);
}

static void close_modal_cb(lv_event_t *e) {
    haptic_click();
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* modal = lv_obj_get_parent(lv_obj_get_parent(btn));
    lv_obj_delete(modal);
}

static void arp_btn_cb(lv_event_t *e) {
    if (e) haptic_click();
    if (recon_is_arp_scanning()) {
        if (lbl_status) lv_label_set_text(lbl_status, "ARP SCAN IN PROGRESS");
        lv_obj_set_style_text_color(lbl_status, G, 0);
        return;
    }
    int ac = recon_arp_count();
    if (ac == 0) {
        if (lbl_status) lv_label_set_text(lbl_status, "ARP SCANNING...");
        lv_obj_set_style_text_color(lbl_status, G, 0);
        recon_request_arp_scan();
        return;
    }
    lv_obj_t* modal = create_modal("NETWORK HOSTS");
    lv_obj_t* list = lv_list_create(modal);
    lv_obj_set_size(list, 350, 350);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    // Add close button at the top
    lv_obj_t* close_btn = lv_list_add_button(list, nullptr, "[ BACK / CLOSE ]");
    lv_obj_set_style_bg_color(close_btn, BG, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(close_btn, 0), lv_color_hex(0xFF3300), 0);
    lv_obj_add_event_cb(close_btn, close_modal_cb, LV_EVENT_CLICKED, nullptr);

    for (int i = 0; i < ac; i++) {
        const ArpDevice* d = recon_get_arp_device(i);
        if (!d) continue;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s [%s]", d->ip, d->vendor);
        lv_obj_t* btn = lv_list_add_button(list, nullptr, buf);
        lv_obj_set_style_bg_color(btn, BG, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), G, 0);
        lv_obj_add_event_cb(btn, ip_select_cb, LV_EVENT_CLICKED, (void*)d->ip);
    }
}

static void deauth_target_selected_cb(lv_event_t *e) {
    haptic_click();
    lv_obj_t* item = (lv_obj_t*)lv_event_get_target(e);
    const char* bssid = (const char*)lv_event_get_user_data(e);
    int channel = (intptr_t)lv_obj_get_user_data(item);

    recon_request_deauth(bssid, channel);

    
    lv_obj_t* modal = lv_obj_get_parent(lv_obj_get_parent(item));
    lv_obj_delete(modal);
}

static void deauth_btn_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    int wc = recon_wifi_count();
    if (wc == 0) {
        lv_label_set_text(lbl_status, "SCAN WIFI FIRST!");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3300), 0);
        return;
    }

    lv_obj_t* modal = create_modal("SELECT TARGET AP");
    lv_obj_t* list = lv_list_create(modal);
    lv_obj_set_size(list, 350, 350);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    for (int i = 0; i < wc; i++) {
        const ReconWiFi* net = recon_get_wifi(i);
        if (!net) continue;
        char buf[80];
        snprintf(buf, sizeof(buf), "%s (CH%d, %ddBm)", net->ssid, net->channel, net->rssi);
        
        lv_obj_t* btn = lv_list_add_button(list, nullptr, buf);
        lv_obj_set_style_bg_color(btn, BG, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), G, 0);
        lv_obj_set_user_data(btn, (void*)(intptr_t)net->channel);
        lv_obj_add_event_cb(btn, deauth_target_selected_cb, LV_EVENT_CLICKED, (void*)net->bssid);
    }
}

static void evil_twin_html_selected_cb(lv_event_t *e) {
    haptic_click();
    lv_obj_t* item = (lv_obj_t*)lv_event_get_target(e);
    const char* path = (const char*)lv_event_get_user_data(e);

    recon_request_evil_twin_full(et_sel_ssid, et_sel_channel, et_sel_bssid, path);

    
    lv_obj_t* modal = lv_obj_get_parent(lv_obj_get_parent(item));
    lv_obj_delete(modal);
}

static void evil_twin_target_selected_cb(lv_event_t *e) {
    haptic_click();
    lv_obj_t* item = (lv_obj_t*)lv_event_get_target(e);
    const ReconWiFi* net = (const ReconWiFi*)lv_event_get_user_data(e);

    strncpy(et_sel_ssid, net->ssid, sizeof(et_sel_ssid)-1);
    strncpy(et_sel_bssid, net->bssid, sizeof(et_sel_bssid)-1);
    et_sel_channel = net->channel;

    
    lv_obj_t* first_modal = lv_obj_get_parent(lv_obj_get_parent(item));
    lv_obj_delete(first_modal);

    
    lv_obj_t* modal = create_modal("SELECT WEB TEMPLATE");
    lv_obj_t* list = lv_list_create(modal);
    lv_obj_set_size(list, 350, 350);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    bool added_any = false;
    File root = SD.open("/html");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                std::string fname = file.name();
                if (fname.find(".html") != std::string::npos || fname.find(".htm") != std::string::npos) {
                    char* full_path = (char*)malloc(128);
                    snprintf(full_path, 128, "/html/%s", file.name());

                    lv_obj_t* btn = lv_list_add_button(list, nullptr, file.name());
                    lv_obj_set_style_bg_color(btn, BG, 0);
                    lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), G, 0);
                    lv_obj_add_event_cb(btn, evil_twin_html_selected_cb, LV_EVENT_CLICKED, (void*)full_path);
                    added_any = true;
                }
            }
            file = root.openNextFile();
        }
        root.close();
    }

    
    lv_obj_t* btn_def = lv_list_add_button(list, nullptr, "[Default Google Page]");
    lv_obj_set_style_bg_color(btn_def, BG, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(btn_def, 0), G, 0);
    lv_obj_add_event_cb(btn_def, evil_twin_html_selected_cb, LV_EVENT_CLICKED, (void*)"");
}

static void evil_twin_btn_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    int wc = recon_wifi_count();
    if (wc == 0) {
        lv_label_set_text(lbl_status, "SCAN WIFI FIRST!");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3300), 0);
        return;
    }

    lv_obj_t* modal = create_modal("SELECT TARGET AP");
    lv_obj_t* list = lv_list_create(modal);
    lv_obj_set_size(list, 350, 350);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    for (int i = 0; i < wc; i++) {
        const ReconWiFi* net = recon_get_wifi(i);
        if (!net) continue;
        char buf[80];
        snprintf(buf, sizeof(buf), "%s (CH%d)", net->ssid, net->channel);
        
        lv_obj_t* btn = lv_list_add_button(list, nullptr, buf);
        lv_obj_set_style_bg_color(btn, BG, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), G, 0);
        lv_obj_add_event_cb(btn, evil_twin_target_selected_cb, LV_EVENT_CLICKED, (void*)net);
    }
}

static lv_obj_t* kbd_container = nullptr;
static lv_obj_t* ta_input = nullptr;
static lv_obj_t* original_ta = nullptr;

static void kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        haptic_click();
        if (original_ta && ta_input) {
            const char* txt = lv_textarea_get_text(ta_input);
            lv_textarea_set_text(original_ta, txt);
            int idx = (intptr_t)lv_obj_get_user_data(original_ta);
            strncpy(beacon_names[idx], txt, BEACON_SSID_LEN - 1);
            beacon_names[idx][BEACON_SSID_LEN - 1] = '\0';
        }
        if (kbd_container) {
            lv_obj_delete(kbd_container);
            kbd_container = nullptr;
        }
        ta_input = nullptr;
        original_ta = nullptr;
    } else if (code == LV_EVENT_CANCEL) {
        haptic_click();
        if (kbd_container) {
            lv_obj_delete(kbd_container);
            kbd_container = nullptr;
        }
        ta_input = nullptr;
        original_ta = nullptr;
    }
}

static void ta_clicked_cb(lv_event_t *e) {
    haptic_click();
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    original_ta = ta;
    int idx = (intptr_t)lv_obj_get_user_data(ta);

    if (kbd_container) {
        lv_obj_delete(kbd_container);
        kbd_container = nullptr;
    }

    
    kbd_container = lv_obj_create(scr);
    lv_obj_set_size(kbd_container, 410, 502);
    lv_obj_set_pos(kbd_container, 0, 0);
    lv_obj_set_style_bg_color(kbd_container, BG, 0);
    lv_obj_set_style_bg_opa(kbd_container, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(kbd_container, 10, 0);
    lv_obj_clear_flag(kbd_container, LV_OBJ_FLAG_SCROLLABLE);

    
    lv_obj_t *title = lv_label_create(kbd_container);
    char title_text[64];
    snprintf(title_text, sizeof(title_text), "Enter SSID %d:", idx + 1);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 10);

    
    ta_input = lv_textarea_create(kbd_container);
    lv_textarea_set_one_line(ta_input, true);
    lv_textarea_set_max_length(ta_input, 32);
    lv_obj_set_size(ta_input, 410 - 40, 50);
    lv_obj_align(ta_input, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 50);
    lv_textarea_set_text(ta_input, beacon_names[idx]);

    
    lv_obj_set_style_bg_color(ta_input, BG, 0);
    lv_obj_set_style_text_color(ta_input, G, 0);
    lv_obj_set_style_border_color(ta_input, G, 0);
    lv_obj_set_style_border_width(ta_input, 1, 0);
    lv_obj_set_style_radius(ta_input, 0, 0);
    lv_obj_set_style_text_font(ta_input, &lv_font_montserrat_18, 0);

    
    lv_obj_t *kb = lv_keyboard_create(kbd_container);
    lv_keyboard_set_textarea(kb, ta_input);
    lv_obj_set_size(kb, 410 - 20, 240);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -SAFE_BOTTOM);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, nullptr);

    
    lv_obj_set_style_bg_color(kb, BG, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(kb, BG, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, G, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, D, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_16, LV_PART_ITEMS);
}

static void beacon_start_cb(lv_event_t *e) {
    haptic_click();
    lv_obj_t* item = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* modal = lv_obj_get_parent(item);

    
    
    int count = 0;
    char temp_ssids[BEACON_SSID_COUNT][BEACON_SSID_LEN];

    for (int i = 0; i < BEACON_SSID_COUNT; i++) {
        if (strlen(beacon_names[i]) > 0) {
            strncpy(temp_ssids[count], beacon_names[i], BEACON_SSID_LEN - 1);
            temp_ssids[count][BEACON_SSID_LEN - 1] = '\0';
            count++;
        }
    }

    if (kbd_container) {
        lv_obj_delete(kbd_container);
        kbd_container = nullptr;
        ta_input = nullptr;
        original_ta = nullptr;
    }

    if (count > 0) {
        recon_request_beacon_spam(temp_ssids, count);
        if (lbl_status) lv_label_set_text(lbl_status, "BEACON SPAMMING...");
    } else {
        if (lbl_status) lv_label_set_text(lbl_status, "EMPTY SSIDs");
    }
    lv_obj_delete(modal);
}

static void beacon_btn_cb(lv_event_t *e) {
    (void)e;
    haptic_click();

    lv_obj_t* modal = create_modal("CONFIG BEACON SPAM");
    
    
    lv_obj_t* btn_start = make_btn(modal, 10, 30, 350, 40, "START BEACON SPAM", beacon_start_cb);
    lv_obj_align(btn_start, LV_ALIGN_TOP_MID, 0, 20);

    
    lv_obj_t* list = lv_list_create(modal);
    lv_obj_set_size(list, 350, 310);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, BG, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    for (int i = 0; i < BEACON_SSID_COUNT; i++) {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 340, 50);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE); 

        lv_obj_t* ta = lv_textarea_create(row);
        lv_obj_set_size(ta, 330, 42);
        lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_max_length(ta, 32);
        lv_textarea_set_placeholder_text(ta, ("SSID " + std::to_string(i + 1)).c_str());
        lv_textarea_set_text(ta, beacon_names[i]);
        lv_obj_set_user_data(ta, (void*)(intptr_t)i);
        lv_obj_add_event_cb(ta, ta_clicked_cb, LV_EVENT_CLICKED, nullptr);

        
        lv_obj_set_style_bg_color(ta, BG, 0);
        lv_obj_set_style_text_color(ta, G, 0);
        lv_obj_set_style_border_color(ta, D, 0);
        lv_obj_set_style_border_width(ta, 1, 0);
        lv_obj_set_style_radius(ta, 0, 0);
        lv_obj_set_style_text_color(ta, D, LV_PART_TEXTAREA_PLACEHOLDER);
    }
}

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

    y += bh + 7;
    
    make_btn(scr, x, y, bw, bh, "DEAUTH", deauth_btn_cb);
    make_btn(scr, x+bw+7, y, bw, bh, "BEACON", beacon_btn_cb);
    make_btn(scr, x+2*(bw+7), y, bw, bh, "EVIL T", evil_twin_btn_cb);

    y += bh + 7;
    make_btn(scr, x, y, bw*2+7, bh, "ARP", arp_btn_cb);

    y += bh + 15;
    lbl_results = lv_label_create(scr);
    lv_label_set_recolor(lbl_results, true);
    lv_label_set_text(lbl_results, "Tap WiFi or BLE to scan\n\nConfigure attacks via bottom row");
    lv_obj_set_style_text_color(lbl_results, D, 0);
    lv_obj_set_style_text_font(lbl_results, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_results, x, y);
    lv_obj_set_width(lbl_results, 360);
    lv_label_set_long_mode(lbl_results, LV_LABEL_LONG_WRAP);
}

void recon_app_update(void) {
    if (!scr || !lbl_status || !lbl_results) return;

    if (recon_is_scanning() || recon_is_arp_scanning()) {
        const char* arp_msg = recon_is_arp_waiting_wifi() ? "ARP: Connecting..." : "ARP SCANNING...";
        lv_label_set_text(lbl_status, recon_is_arp_scanning() ? arp_msg : "SCANNING...");
        lv_obj_set_style_text_color(lbl_status, G, 0);
    } else if (recon_is_deauthing()) {
        lv_label_set_text(lbl_status, "DEAUTH ACTIVE");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3300), 0);
        return;
    } else if (recon_is_beacon_spamming()) {
        char b[48];
        snprintf(b, sizeof(b), "BEACON SPAMMING: %d SSIDs", recon_beacon_active_count());
        lv_label_set_text(lbl_status, b);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF9900), 0);
    } else if (recon_is_ip_sniffing()) {
        char b[48];
        snprintf(b, sizeof(b), "SNIFFING: %s", recon_sniff_target_ip());
        lv_label_set_text(lbl_status, b);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF9900), 0);
    } else if (recon_is_evil_twin()) {
        lv_label_set_text(lbl_status, "EVIL TWIN ACTIVE");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3300), 0);
    } else {
        if (recon_wifi_count() > 0 || recon_ble_count() > 0) {
            char b[32]; snprintf(b, sizeof(b), "DONE: %d WiFi, %d BLE",
                recon_wifi_count(), recon_ble_count());
            lv_label_set_text(lbl_status, b);
        } else {
            lv_label_set_text(lbl_status, "READY");
        }
        lv_obj_set_style_text_color(lbl_status, G, 0);
    }

    char buf[1024] = "";
    int pos = 0;
    int wc = recon_wifi_count();
    int bc = recon_ble_count();

    if (recon_is_arp_scanning()) {
        arp_scan_was_running = true;
    } else if (arp_scan_was_running) {
        arp_scan_was_running = false;
        int ac = recon_arp_count();
        if (ac > 0) {
            arp_btn_cb(nullptr);
        } else {
            if (lbl_status) lv_label_set_text(lbl_status, "ARP: NO HOSTS FOUND");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF9900), 0);
        }
    }

    if (recon_is_ip_sniffing()) {
        int uc = recon_sniff_unique_ip_count();
        pos += snprintf(buf+pos, sizeof(buf)-pos, "[ SNIFF: %s ]\n", recon_sniff_target_ip());
        pos += snprintf(buf+pos, sizeof(buf)-pos, "PCAP -> SD:/traffic/\n\n");
        if (uc == 0) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "Waiting for packets...\n");
        } else {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "Remote IPs (%d):\n", uc);
            for (int i = 0; i < uc && pos < 900; i++) {
                pos += snprintf(buf+pos, sizeof(buf)-pos, "  %s\n", recon_sniff_get_ip(i));
            }
        }
    } else if (recon_is_arp_scanning()) {
        pos += snprintf(buf+pos, sizeof(buf)-pos, "[ ARP SCAN ]\n");
        if (recon_is_arp_waiting_wifi()) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "Connecting to WiFi...\n");
        } else {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "Sending ARP requests...\n%d/254\nHosts found: %d\n", recon_arp_scan_progress(), recon_arp_count());
        }
    } else if (recon_is_evil_twin()) {
        pos += snprintf(buf+pos, sizeof(buf)-pos, "[ EVIL TWIN LOG ]\n");
        if (recon_et_has_new_cred()) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "#00E5FF Captured: %s#\n", recon_et_last_cred());
        } else if (strlen(recon_et_last_cred()) > 0) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "Last captured: %s\n", recon_et_last_cred());
        } else {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "Awaiting credentials on sahte AP...\nLogs saved to /evil.csv\n");
        }
    } else if (recon_is_beacon_spamming()) {
        pos += snprintf(buf+pos, sizeof(buf)-pos, "[ BEACON SPAM LOG ]\nSSIDs broadcasting:\n");
        for (int i = 0; i < recon_beacon_active_count() && i < 8; i++) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, " -> %s\n", beacon_names[i]);
        }
    } else {
        if (wc > 0) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "WiFi: %d networks\n", wc);
            for (int i = 0; i < wc && i < 10 && pos < 900; i++) {
                const ReconWiFi *n = recon_get_wifi(i);
                if (!n) continue;
                if (n->is_camera) {
                    pos += snprintf(buf+pos, sizeof(buf)-pos,
                        "#FF9900 %d. %s [%d] CH%d (CAM!)#\n",
                        i+1, n->ssid, n->rssi, n->channel);
                } else {
                    pos += snprintf(buf+pos, sizeof(buf)-pos,
                        " %d. %s [%d] CH%d\n",
                        i+1, n->ssid, n->rssi, n->channel);
                }
            }
            if (wc > 10) pos += snprintf(buf+pos, sizeof(buf)-pos, " ...+%d more\n", wc-10);
        }
        if (bc > 0) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "\nBLE: %d devices\n", bc);
            for (int i = 0; i < bc && i < 10 && pos < 900; i++) {
                const BleDevice *d = recon_get_ble(i);
                if (d) {
                    if (d->is_airtag) {
                        pos += snprintf(buf+pos, sizeof(buf)-pos, "#00E5FF  %s %s [%d] (airtag)#\n",
                            d->mac, d->name[0] ? d->name : "?", d->rssi);
                    } else if (d->is_flipper) {
                        pos += snprintf(buf+pos, sizeof(buf)-pos, "#00E5FF  %s %s [%d] (flipper)#\n",
                            d->mac, d->name[0] ? d->name : "?", d->rssi);
                    } else {
                        pos += snprintf(buf+pos, sizeof(buf)-pos, "  %s %s [%d]\n",
                            d->mac, d->name[0] ? d->name : "?", d->rssi);
                    }
                }
            }
        }
    }
    if (pos > 0) lv_label_set_text(lbl_results, buf);
}

void recon_app_destroy(void) {
    recon_request_stop();
    if (kbd_container) {
        lv_obj_delete(kbd_container);
        kbd_container = nullptr;
    }
    ta_input = nullptr;
    original_ta = nullptr;
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_status = lbl_results = nullptr;
}
