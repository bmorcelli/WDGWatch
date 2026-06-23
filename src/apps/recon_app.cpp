#include "recon_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include <vector>
#include <string>
#include "../config.h"
#include "../ui/theme.h"
#include "../hal/haptic.h"
#include "../hal/recon_service.h"
#include "../hal/time_sync.h"
#include <SD.h>
#include "../hal/audio_record.h"
#include <cmath>
#include <cstring>
#include "app_common.h"
#include <Preferences.h>
#include <WiFi.h>
#include "../web/web_server.h"

static void led_restore_wifi_state();

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_results = nullptr;

#define G  lv_color_hex(PIPBOY_GREEN)
#define D  lv_color_hex(PIPBOY_GREEN_DIM)
#define BG lv_color_hex(PIPBOY_BG)
#define RC_G theme_color_recolor_str
#define RC_D theme_color_dim_recolor_str

static char et_sel_ssid[33] = "";
static char et_sel_bssid[18] = "";
static int et_sel_channel = 1;

static char beacon_names[BEACON_SSID_COUNT][BEACON_SSID_LEN];

struct AirportPreset {
    const char* name;
    const char* tz_match;
    double lat;
    double lon;
};

static const AirportPreset airport_db[] = {
    {"London (LHR)",         "GMT0",   51.4700,  -0.4543},
    {"Dublin (DUB)",         "GMT0",   53.4213,  -6.2700},
    {"Lisbon (LIS)",         "GMT0",   38.7742,  -9.1342},
    {"Paris (CDG)",          "GMT-1",  49.0097,   2.5479},
    {"Berlin (BER)",         "GMT-1",  52.3667,  13.5033},
    {"Rome (FCO)",           "GMT-1",  41.8003,  12.2389},
    {"Amsterdam (AMS)",      "GMT-1",  52.3086,   4.7639},
    {"Madrid (MAD)",         "GMT-1",  40.4719,  -3.5626},
    {"Athens (ATH)",         "GMT-2",  37.9356,  23.9484},
    {"Cairo (CAI)",          "GMT-2",  30.1219,  31.4056},
    {"Johannesburg (JNB)",   "GMT-2", -26.1392,  28.2460},
    {"Istanbul (IST)",       "GMT-3",  41.2631,  28.7412},
    {"Ankara (ESB)",         "GMT-3",  40.1281,  32.9951},
    {"Izmir (ADB)",          "GMT-3",  38.2924,  27.1570},
    {"Moscow (SVO)",         "GMT-3",  55.9726,  37.4146},
    {"Riyadh (RUH)",         "GMT-3",  24.9576,  46.6988},
    {"Dubai (DXB)",          "GST-4",  25.2532,  55.3657},
    {"Abu Dhabi (AUH)",      "GST-4",  24.4330,  54.6511},
    {"Baku (GYD)",           "GST-4",  40.4675,  50.0467},
    {"Karachi (KHI)",        "GMT-5",  24.9065,  67.1608},
    {"Lahore (LHE)",         "GMT-5",  31.5216,  74.4036},
    {"Mumbai (BOM)",         "GMT-5:30", 19.0896, 72.8656},
    {"Delhi (DEL)",          "GMT-5:30", 28.5665, 77.1031},
    {"Dhaka (DAC)",          "GMT-6",  23.8433,  90.3978},
    {"Bangkok (BKK)",        "GMT-7",  13.6811, 100.7472},
    {"Jakarta (CGK)",        "GMT-7",  -6.1256, 106.6559},
    {"Singapore (SIN)",      "GMT-8",   1.3644, 103.9915},
    {"Beijing (PEK)",        "GMT-8",  40.0799, 116.5846},
    {"Hong Kong (HKG)",      "GMT-8",  22.3080, 113.9185},
    {"Tokyo (HND)",          "GMT-9",  35.5494, 139.7798},
    {"Seoul (ICN)",          "GMT-9",  37.4602, 126.4407},
    {"Sydney (SYD)",         "GMT-10", -33.9461, 151.1772},
    {"Auckland (AKL)",       "GMT-12", -37.0082, 174.7917},
    {"New York (JFK)",       "GMT+5",  40.6398, -73.7789},
    {"Los Angeles (LAX)",    "GMT+8",  33.9416, -118.4085},
    {"Chicago (ORD)",        "GMT+6",  41.9742, -87.9073},
    {"Miami (MIA)",          "GMT+5",  25.7959, -80.2870},
    {"Toronto (YYZ)",        "GMT+5",  43.6777, -79.6248},
    {"Sao Paulo (GRU)",      "GMT+3", -23.4356, -46.4731},
    {"Mexico City (MEX)",    "GMT+6",  19.4363, -99.0721},
};

static const int AIRPORT_DB_COUNT = (int)(sizeof(airport_db) / sizeof(airport_db[0]));

static lv_obj_t* adsb_overlay    = nullptr;
static lv_obj_t* adsb_data_panel = nullptr;
static lv_obj_t* adsb_lbl_page   = nullptr;
static lv_obj_t* adsb_lbl_title  = nullptr;
static lv_obj_t* adsb_lbl_data   = nullptr;
#define ADSB_SWEEP_TRAILS 5
static lv_obj_t* adsb_sweep_lines[ADSB_SWEEP_TRAILS] = {};
static lv_point_precise_t adsb_sweep_trail_pts[ADSB_SWEEP_TRAILS][2];
static lv_obj_t* adsb_ac_dot     = nullptr;
static lv_timer_t* adsb_radar_timer  = nullptr;
static lv_timer_t* adsb_update_timer = nullptr;
static int  adsb_current_page = 0;
static float adsb_sweep_angle = 0.0f;
static char adsb_airport_label[32] = "";

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
    style_button_by_position(btn, y, h);
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
    audio_rec_stop();
}

static void adsb_destroy_overlay(void) {
    if (adsb_radar_timer)  { lv_timer_delete(adsb_radar_timer);  adsb_radar_timer  = nullptr; }
    if (adsb_update_timer) { lv_timer_delete(adsb_update_timer); adsb_update_timer = nullptr; }
    if (adsb_overlay)      { lv_obj_delete(adsb_overlay);        adsb_overlay      = nullptr; }
    adsb_data_panel = adsb_lbl_page = adsb_lbl_title = adsb_lbl_data = adsb_ac_dot = nullptr;
    for (int i = 0; i < ADSB_SWEEP_TRAILS; i++) {
        adsb_sweep_lines[i] = nullptr;
    }
}

static void adsb_close_cb(lv_event_t *e) {
    (void)e; haptic_click();
    recon_request_stop();
    adsb_destroy_overlay();
}

static void adsb_prev_cb(lv_event_t *e) { (void)e; haptic_click(); adsb_current_page = (adsb_current_page + 3) % 4; }
static void adsb_next_cb(lv_event_t *e) { (void)e; haptic_click(); adsb_current_page = (adsb_current_page + 1) % 4; }

static void adsb_render_page(void) {
    if (!adsb_lbl_data || !adsb_lbl_page || !adsb_lbl_title) return;
    char page_txt[12]; snprintf(page_txt, sizeof(page_txt), "PAGE %d / 4", adsb_current_page + 1);
    lv_label_set_text(adsb_lbl_page, page_txt);
    char title_buf[48]; snprintf(title_buf, sizeof(title_buf), "[ ADS-B ] %s", adsb_airport_label);
    lv_label_set_text(adsb_lbl_title, title_buf);

    if (!recon_adsb_has_aircraft()) {
        if (adsb_ac_dot) lv_obj_add_flag(adsb_ac_dot, LV_OBJ_FLAG_HIDDEN);
        char sb[96]; snprintf(sb, sizeof(sb), "#%s %s#", RC_D, recon_get_adsb_status());
        lv_label_set_recolor(adsb_lbl_data, true);
        lv_label_set_text(adsb_lbl_data, sb); return;
    }
    const AdsbAircraft* ac = recon_get_adsb_aircraft();

    if (adsb_ac_dot) {
        float pixel_dist = (ac->distance / 50.0f) * 105.0f;
        if (pixel_dist > 105.0f) pixel_dist = 105.0f;
        
        float angle_rad = (ac->bearing - 90.0f) * (float)M_PI / 180.0f;
        int ac_x = 205 + (int)(pixel_dist * cosf(angle_rad));
        int ac_y = 178 + (int)(pixel_dist * sinf(angle_rad));
        
        lv_obj_set_pos(adsb_ac_dot, ac_x - 4, ac_y - 4);
        
        bool emerg_sq = (strcmp(ac->squawk,"7500")==0 || strcmp(ac->squawk,"7600")==0 || strcmp(ac->squawk,"7700")==0);
        lv_obj_set_style_bg_color(adsb_ac_dot, emerg_sq ? lv_color_hex(0xFF3300) : lv_color_hex(0xFF9900), 0); 
    }

    bool emerg = (strcmp(ac->squawk,"7500")==0 || strcmp(ac->squawk,"7600")==0 || strcmp(ac->squawk,"7700")==0);
    lv_obj_set_style_text_color(adsb_lbl_title, emerg ? lv_color_hex(0xFF3300) : G, 0);
    char buf[256]; lv_label_set_recolor(adsb_lbl_data, true);
    switch (adsb_current_page) {
        case 0: snprintf(buf,sizeof(buf),"#%s FLT:# %s\n#%s REG:# %s\n#%s TYPE:# %s\n#%s ROUTE:# %s\n#%s DIST:# %.1f km",RC_G,ac->flight,RC_G,ac->reg,RC_G,ac->type,RC_G,ac->route,RC_G,ac->distance); break;
        case 1: snprintf(buf,sizeof(buf),"#%s ALT:# %d ft\n#%s SPD:# %.0f kt\n#%s V/S:# %+d ft/m\n#%s HDG:# %.0f deg",RC_G,ac->alt_baro,RC_G,(double)ac->gs,RC_G,ac->baro_rate,RC_G,(double)ac->true_heading); break;
        case 2: snprintf(buf,sizeof(buf),"#%s SQUAWK:# %s\n#%s EMERG:# %s",RC_G,ac->squawk,RC_G,emerg?"#FF3300 EMERGENCY#":ac->emergency); break;
        default: snprintf(buf,sizeof(buf),"#%s --- RAW ---#\n#%s FLT:# %-10s\n#%s SQK:# %s\n#%s ALT:# %d\n#%s GS:#  %.0f\n#%s HDG:# %.0f",RC_D,RC_G,ac->flight,RC_G,ac->squawk,RC_G,ac->alt_baro,RC_G,(double)ac->gs,RC_G,(double)ac->true_heading); break;
    }
    lv_label_set_text(adsb_lbl_data, buf);
}

static void adsb_radar_cb(lv_timer_t*) {
    
    for (int i = ADSB_SWEEP_TRAILS - 1; i > 0; i--) {
        adsb_sweep_trail_pts[i][0] = adsb_sweep_trail_pts[i-1][0];
        adsb_sweep_trail_pts[i][1] = adsb_sweep_trail_pts[i-1][1];
        if (adsb_sweep_lines[i]) {
            lv_line_set_points(adsb_sweep_lines[i], adsb_sweep_trail_pts[i], 2);
        }
    }

    
    adsb_sweep_angle += 4.0f; if (adsb_sweep_angle >= 360.0f) adsb_sweep_angle -= 360.0f;
    static const int CX=205, CY=178, R=105;
    float rad = adsb_sweep_angle * (float)M_PI / 180.0f;
    adsb_sweep_trail_pts[0][0].x = CX; adsb_sweep_trail_pts[0][0].y = CY;
    adsb_sweep_trail_pts[0][1].x = (lv_value_precise_t)(CX + R * cosf(rad));
    adsb_sweep_trail_pts[0][1].y = (lv_value_precise_t)(CY + R * sinf(rad));
    if (adsb_sweep_lines[0]) {
        lv_line_set_points(adsb_sweep_lines[0], adsb_sweep_trail_pts[0], 2);
    }

    
    static int blink_cnt = 0;
    blink_cnt++;
    if (adsb_ac_dot) {
        if (recon_adsb_has_aircraft()) {
            if (blink_cnt % 5 == 0) {
                if (lv_obj_has_flag(adsb_ac_dot, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_clear_flag(adsb_ac_dot, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(adsb_ac_dot, LV_OBJ_FLAG_HIDDEN);
                }
            }
        } else {
            lv_obj_add_flag(adsb_ac_dot, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void adsb_update_cb(lv_timer_t*) { adsb_render_page(); }

static void adsb_open_overlay(const char* airport_name, double lat, double lon) {
    adsb_destroy_overlay();
    adsb_current_page = 0; adsb_sweep_angle = 0.0f;
    strncpy(adsb_airport_label, airport_name, sizeof(adsb_airport_label)-1);

    adsb_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(adsb_overlay);
    lv_obj_set_size(adsb_overlay, 410, 502);
    lv_obj_set_pos(adsb_overlay, 0, 0);
    lv_obj_set_style_bg_color(adsb_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(adsb_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(adsb_overlay, LV_OBJ_FLAG_SCROLLABLE);

    static const int CX=205, CY=178, R=105;

    adsb_lbl_title = lv_label_create(adsb_overlay);
    lv_obj_set_style_text_font(adsb_lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(adsb_lbl_title, G, 0);
    lv_obj_align(adsb_lbl_title, LV_ALIGN_TOP_MID, 0, SAFE_TOP);

    lv_obj_t* btn_close = lv_button_create(adsb_overlay);
    lv_obj_set_size(btn_close, 100, 42);
    lv_obj_set_pos(btn_close, SAFE_LEFT+5, SAFE_TOP-2);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(btn_close, lv_color_hex(0xFF3300), 0);
    lv_obj_set_style_border_width(btn_close, 1, 0);
    lv_obj_set_style_radius(btn_close, 0, 0);
    lv_obj_add_event_cb(btn_close, adsb_close_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* cl = lv_label_create(btn_close);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE " CLOSE");
    lv_obj_set_style_text_color(cl, lv_color_hex(0xFF3300), 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_16, 0);
    lv_obj_center(cl);

    for (int i = 1; i <= 3; i++) {
        int rr = (R * i) / 3;
        lv_obj_t* ring = lv_arc_create(adsb_overlay);
        lv_arc_set_bg_angles(ring, 0, 360);
        lv_arc_set_value(ring, 360);
        lv_obj_set_size(ring, rr*2, rr*2);
        lv_obj_set_pos(ring, CX-rr, CY-rr);
        lv_obj_set_style_arc_color(ring, lv_color_hex(0x003030), LV_PART_MAIN);
        lv_obj_set_style_arc_color(ring, lv_color_hex(0x000000), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(ring, 1, LV_PART_MAIN);
        lv_obj_set_style_arc_width(ring, 0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_opa(ring, LV_OPA_TRANSP, LV_PART_KNOB); 
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    }

    static lv_point_precise_t cv[2] = {{CX,CY-R},{CX,CY+R}};
    lv_obj_t* lv = lv_line_create(adsb_overlay);
    lv_line_set_points(lv, cv, 2);
    lv_obj_set_style_line_color(lv, lv_color_hex(0x003030), 0);
    lv_obj_set_style_line_width(lv, 1, 0);

    static lv_point_precise_t ch[2] = {{CX-R,CY},{CX+R,CY}};
    lv_obj_t* lh = lv_line_create(adsb_overlay);
    lv_line_set_points(lh, ch, 2);
    lv_obj_set_style_line_color(lh, lv_color_hex(0x003030), 0);
    lv_obj_set_style_line_width(lh, 1, 0);

    uint8_t opacities[] = {200, 130, 80, 40, 20};
    for (int i = ADSB_SWEEP_TRAILS - 1; i >= 0; i--) {
        adsb_sweep_lines[i] = lv_line_create(adsb_overlay);
        adsb_sweep_trail_pts[i][0] = {CX, CY};
        adsb_sweep_trail_pts[i][1] = {CX, CY};
        lv_line_set_points(adsb_sweep_lines[i], adsb_sweep_trail_pts[i], 2);
        lv_obj_set_style_line_color(adsb_sweep_lines[i], G, 0);
        lv_obj_set_style_line_width(adsb_sweep_lines[i], (i == 0) ? 2 : 1, 0);
        lv_obj_set_style_line_opa(adsb_sweep_lines[i], opacities[i], 0);
    }

    lv_obj_t* dot = lv_obj_create(adsb_overlay);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_pos(dot, CX-3, CY-3);
    lv_obj_set_style_bg_color(dot, G, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);

    adsb_ac_dot = lv_obj_create(adsb_overlay);
    lv_obj_set_size(adsb_ac_dot, 8, 8);
    lv_obj_set_style_bg_color(adsb_ac_dot, G, 0);
    lv_obj_set_style_radius(adsb_ac_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(adsb_ac_dot, 0, 0);
    lv_obj_clear_flag(adsb_ac_dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(adsb_ac_dot, LV_OBJ_FLAG_HIDDEN);

    static lv_point_precise_t sp[2] = {{SAFE_LEFT,300},{(lv_value_precise_t)(410-SAFE_LEFT),300}};
    lv_obj_t* sep = lv_line_create(adsb_overlay);
    lv_line_set_points(sep, sp, 2);
    lv_obj_set_style_line_color(sep, D, 0);
    lv_obj_set_style_line_width(sep, 1, 0);

    adsb_data_panel = lv_obj_create(adsb_overlay);
    lv_obj_remove_style_all(adsb_data_panel);
    lv_obj_set_size(adsb_data_panel, 380, 140);
    lv_obj_set_pos(adsb_data_panel, SAFE_LEFT+5, 305);
    lv_obj_set_style_bg_opa(adsb_data_panel, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(adsb_data_panel, LV_OBJ_FLAG_SCROLLABLE);

    adsb_lbl_data = lv_label_create(adsb_data_panel);
    lv_obj_set_width(adsb_lbl_data, 370);
    lv_obj_set_pos(adsb_lbl_data, 0, 0);
    lv_obj_set_style_text_font(adsb_lbl_data, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(adsb_lbl_data, G, 0);
    lv_label_set_long_mode(adsb_lbl_data, LV_LABEL_LONG_WRAP);
    lv_label_set_text(adsb_lbl_data, "Connecting...");

    int nav_y = 455;
    lv_obj_t* bp = lv_button_create(adsb_overlay);
    lv_obj_set_size(bp, 110, 42); lv_obj_set_pos(bp, SAFE_LEFT+5, nav_y);
    lv_obj_set_style_bg_color(bp, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(bp, D, 0);
    lv_obj_set_style_border_width(bp, 1, 0); lv_obj_set_style_radius(bp, 0, 0);
    lv_obj_add_event_cb(bp, adsb_prev_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bpl = lv_label_create(bp); lv_label_set_text(bpl, "< PREV");
    lv_obj_set_style_text_color(bpl, D, 0);
    lv_obj_set_style_text_font(bpl, &lv_font_montserrat_16, 0); lv_obj_center(bpl);

    adsb_lbl_page = lv_label_create(adsb_overlay);
    lv_label_set_text(adsb_lbl_page, "PAGE 1 / 4");
    lv_obj_set_style_text_color(adsb_lbl_page, D, 0);
    lv_obj_set_style_text_font(adsb_lbl_page, &lv_font_montserrat_16, 0);
    lv_obj_align(adsb_lbl_page, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t* bn = lv_button_create(adsb_overlay);
    lv_obj_set_size(bn, 110, 42); lv_obj_set_pos(bn, 410-SAFE_LEFT-5-110, nav_y);
    lv_obj_set_style_bg_color(bn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(bn, D, 0);
    lv_obj_set_style_border_width(bn, 1, 0); lv_obj_set_style_radius(bn, 0, 0);
    lv_obj_add_event_cb(bn, adsb_next_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bnl = lv_label_create(bn); lv_label_set_text(bnl, "NEXT >");
    lv_obj_set_style_text_color(bnl, D, 0);
    lv_obj_set_style_text_font(bnl, &lv_font_montserrat_16, 0); lv_obj_center(bnl);

    adsb_radar_timer  = lv_timer_create(adsb_radar_cb,  50,  nullptr);
    adsb_update_timer = lv_timer_create(adsb_update_cb, 500, nullptr);
    recon_request_adsb_track(lat, lon, airport_name);
    adsb_render_page();
}

static lv_obj_t* adsb_airport_modal = nullptr;
static lv_obj_t* adsb_coords_modal = nullptr;
static lv_obj_t* ta_custom_lat = nullptr;
static lv_obj_t* ta_custom_lon = nullptr;
static lv_obj_t* kb_coords = nullptr;

static double custom_lat = 40.7128;
static double custom_lon = -74.0060;

static void load_custom_coords() {
    Preferences prefs;
    prefs.begin("adsb_custom", true);
    custom_lat = prefs.getDouble("lat", 40.7128);
    custom_lon = prefs.getDouble("lon", -74.0060);
    prefs.end();
}

static void save_custom_coords(double lat, double lon) {
    custom_lat = lat;
    custom_lon = lon;
    Preferences prefs;
    prefs.begin("adsb_custom", false);
    prefs.putDouble("lat", lat);
    prefs.putDouble("lon", lon);
    prefs.end();
}

static void adsb_airport_select_cb(lv_event_t *e) {
    haptic_click();
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (adsb_airport_modal) { lv_obj_delete(adsb_airport_modal); adsb_airport_modal = nullptr; }
    if (idx >= 0 && idx < AIRPORT_DB_COUNT)
        adsb_open_overlay(airport_db[idx].name, airport_db[idx].lat, airport_db[idx].lon);
}

static void adsb_modal_close_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (adsb_airport_modal) { lv_obj_delete(adsb_airport_modal); adsb_airport_modal = nullptr; }
}

static void ta_coords_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        if(kb_coords != nullptr) {
            lv_keyboard_set_textarea(kb_coords, ta);
        }
    }
}

static void adsb_btn_cb(lv_event_t *e);

static void coords_cancel_btn_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (adsb_coords_modal) {
        lv_obj_delete(adsb_coords_modal);
        adsb_coords_modal = nullptr;
    }
    adsb_btn_cb(nullptr);
}

static void coords_save_btn_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (!ta_custom_lat || !ta_custom_lon) return;
    const char* lat_str = lv_textarea_get_text(ta_custom_lat);
    const char* lon_str = lv_textarea_get_text(ta_custom_lon);
    double lat = atof(lat_str);
    double lon = atof(lon_str);
    save_custom_coords(lat, lon);
    
    if (adsb_coords_modal) {
        lv_obj_delete(adsb_coords_modal);
        adsb_coords_modal = nullptr;
    }
    
    adsb_open_overlay("CUSTOM", lat, lon);
}

static void adsb_coords_btn_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (adsb_airport_modal) {
        lv_obj_delete(adsb_airport_modal);
        adsb_airport_modal = nullptr;
    }

    adsb_coords_modal = lv_obj_create(scr);
    lv_obj_set_size(adsb_coords_modal, 410, 502);
    lv_obj_set_pos(adsb_coords_modal, 0, 0);
    lv_obj_set_style_bg_color(adsb_coords_modal, BG, 0);
    lv_obj_set_style_bg_opa(adsb_coords_modal, LV_OPA_COVER, 0);
    lv_obj_clear_flag(adsb_coords_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(adsb_coords_modal);
    lv_label_set_text(title, "[ ENTER COORDINATES ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP);

    
    lv_obj_t *lbl_lat = lv_label_create(adsb_coords_modal);
    lv_label_set_text(lbl_lat, "LATITUDE:");
    lv_obj_set_style_text_color(lbl_lat, D, 0);
    lv_obj_set_style_text_font(lbl_lat, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_lat, LV_ALIGN_TOP_LEFT, SAFE_LEFT + 20, SAFE_TOP + 25);

    
    ta_custom_lat = lv_textarea_create(adsb_coords_modal);
    lv_textarea_set_one_line(ta_custom_lat, true);
    lv_obj_set_size(ta_custom_lat, 370, 42);
    lv_obj_align(ta_custom_lat, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 45);
    lv_obj_set_style_bg_color(ta_custom_lat, BG, 0);
    lv_obj_set_style_border_color(ta_custom_lat, G, 0);
    lv_obj_set_style_border_width(ta_custom_lat, 1, 0);
    lv_obj_set_style_radius(ta_custom_lat, 8, 0);
    lv_obj_set_style_text_color(ta_custom_lat, G, 0);
    lv_obj_clear_flag(ta_custom_lat, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ta_custom_lat, ta_coords_event_cb, LV_EVENT_ALL, nullptr);

    char lat_buf[24];
    snprintf(lat_buf, sizeof(lat_buf), "%.6f", custom_lat);
    lv_textarea_set_text(ta_custom_lat, lat_buf);

    
    lv_obj_t *lbl_lon = lv_label_create(adsb_coords_modal);
    lv_label_set_text(lbl_lon, "LONGITUDE:");
    lv_obj_set_style_text_color(lbl_lon, D, 0);
    lv_obj_set_style_text_font(lbl_lon, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_lon, LV_ALIGN_TOP_LEFT, SAFE_LEFT + 20, SAFE_TOP + 92);

    
    ta_custom_lon = lv_textarea_create(adsb_coords_modal);
    lv_textarea_set_one_line(ta_custom_lon, true);
    lv_obj_set_size(ta_custom_lon, 370, 42);
    lv_obj_align(ta_custom_lon, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 112);
    lv_obj_set_style_bg_color(ta_custom_lon, BG, 0);
    lv_obj_set_style_border_color(ta_custom_lon, G, 0);
    lv_obj_set_style_border_width(ta_custom_lon, 1, 0);
    lv_obj_set_style_radius(ta_custom_lon, 8, 0);
    lv_obj_set_style_text_color(ta_custom_lon, G, 0);
    lv_obj_clear_flag(ta_custom_lon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ta_custom_lon, ta_coords_event_cb, LV_EVENT_ALL, nullptr);

    char lon_buf[24];
    snprintf(lon_buf, sizeof(lon_buf), "%.6f", custom_lon);
    lv_textarea_set_text(ta_custom_lon, lon_buf);

    
    lv_obj_t *btn_cancel = lv_button_create(adsb_coords_modal);
    lv_obj_set_size(btn_cancel, 165, 40);
    lv_obj_set_pos(btn_cancel, 25, SAFE_TOP + 165);
    lv_obj_set_style_bg_color(btn_cancel, BG, 0);
    lv_obj_set_style_border_color(btn_cancel, lv_color_hex(0xFF3300), 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, coords_cancel_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_c = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_c, "CANCEL");
    lv_obj_set_style_text_color(lbl_c, lv_color_hex(0xFF3300), 0);
    lv_obj_center(lbl_c);

    lv_obj_t *btn_save = lv_button_create(adsb_coords_modal);
    lv_obj_set_size(btn_save, 165, 40);
    lv_obj_set_pos(btn_save, 220, SAFE_TOP + 165);
    lv_obj_set_style_bg_color(btn_save, BG, 0);
    lv_obj_set_style_border_color(btn_save, G, 0);
    lv_obj_set_style_border_width(btn_save, 1, 0);
    lv_obj_set_style_radius(btn_save, 8, 0);
    lv_obj_add_event_cb(btn_save, coords_save_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_s = lv_label_create(btn_save);
    lv_label_set_text(lbl_s, "SAVE & TRACK");
    lv_obj_set_style_text_color(lbl_s, G, 0);
    lv_obj_center(lbl_s);

    kb_coords = lv_keyboard_create(adsb_coords_modal);
    
    static const char * kb_map[] = {
        "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
        "4", "5", "6", ".", "\n",
        "7", "8", "9", ",", "\n",
        "-", "0", LV_SYMBOL_CLOSE, LV_SYMBOL_OK, ""
    };

    static const lv_buttonmatrix_ctrl_t kb_ctrl[] = {
        (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)(LV_BUTTONMATRIX_CTRL_CHECKED | 1),
        (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1,
        (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1,
        (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)1, (lv_buttonmatrix_ctrl_t)(LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_CHECKED | 1), (lv_buttonmatrix_ctrl_t)(LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_CHECKED | 1)
    };

    lv_keyboard_set_map(kb_coords, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
    lv_keyboard_set_mode(kb_coords, LV_KEYBOARD_MODE_USER_1);

    lv_keyboard_set_textarea(kb_coords, ta_custom_lat);
    lv_obj_set_size(kb_coords, 410, 220);
    lv_obj_align(kb_coords, LV_ALIGN_BOTTOM_MID, 0, 0);

    
    lv_obj_set_style_bg_color(kb_coords, BG, 0);
    lv_obj_set_style_bg_opa(kb_coords, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(kb_coords, BG, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb_coords, G, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb_coords, D, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb_coords, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb_coords, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb_coords, &lv_font_montserrat_16, LV_PART_ITEMS);

    lv_obj_add_event_cb(kb_coords, [](lv_event_t* ev) {
        lv_event_code_t c = lv_event_get_code(ev);
        if (c == LV_EVENT_READY) {
            coords_save_btn_cb(nullptr);
        } else if (c == LV_EVENT_CANCEL) {
            coords_cancel_btn_cb(nullptr);
        }
    }, LV_EVENT_ALL, nullptr);
}

static void adsb_btn_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (adsb_airport_modal) { lv_obj_delete(adsb_airport_modal); adsb_airport_modal = nullptr; }

    String tz = time_sync_get_timezone();

    adsb_airport_modal = lv_obj_create(scr);
    lv_obj_set_size(adsb_airport_modal, 390, 440);
    lv_obj_align(adsb_airport_modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(adsb_airport_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(adsb_airport_modal, G, 0);
    lv_obj_set_style_border_width(adsb_airport_modal, 2, 0);
    lv_obj_set_style_radius(adsb_airport_modal, 20, 0);

    lv_obj_t* hdr = lv_label_create(adsb_airport_modal);
    lv_label_set_text(hdr, "SELECT AIRPORT");
    lv_obj_set_style_text_color(hdr, G, 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_18, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* list = lv_list_create(adsb_airport_modal);
    lv_obj_set_size(list, 370, 330);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(list, 0, 0);

    lv_obj_t* cancel = lv_list_add_button(list, nullptr, "[ CANCEL ]");
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(cancel, 0), lv_color_hex(0xFF3300), 0);
    lv_obj_add_event_cb(cancel, adsb_modal_close_cb, LV_EVENT_CLICKED, nullptr);

    int matched = 0;
    for (int i = 0; i < AIRPORT_DB_COUNT; i++) {
        if (tz == airport_db[i].tz_match) {
            lv_obj_t* b = lv_list_add_button(list, LV_SYMBOL_RIGHT, airport_db[i].name);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x000000), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 1), G, 0);
            lv_obj_set_style_text_font(lv_obj_get_child(b, 1), &lv_font_montserrat_16, 0);
            lv_obj_add_event_cb(b, adsb_airport_select_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            matched++;
        }
    }
    if (matched == 0) {
        for (int i = 0; i < AIRPORT_DB_COUNT; i++) {
            lv_obj_t* b = lv_list_add_button(list, LV_SYMBOL_RIGHT, airport_db[i].name);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x000000), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 1), D, 0);
            lv_obj_set_style_text_font(lv_obj_get_child(b, 1), &lv_font_montserrat_16, 0);
            lv_obj_add_event_cb(b, adsb_airport_select_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    }

    lv_obj_t* btn_coords = lv_button_create(adsb_airport_modal);
    lv_obj_set_size(btn_coords, 240, 40);
    lv_obj_align(btn_coords, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(btn_coords, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(btn_coords, D, 0);
    lv_obj_set_style_border_width(btn_coords, 1, 0);
    lv_obj_set_style_radius(btn_coords, 8, 0);
    lv_obj_add_event_cb(btn_coords, adsb_coords_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_move_to_index(btn_coords, -1);

    lv_obj_t* lbl_coords = lv_label_create(btn_coords);
    lv_label_set_text(lbl_coords, "ENTER COORDINATES");
    lv_obj_set_style_text_color(lbl_coords, G, 0);
    lv_obj_center(lbl_coords);
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

static void rec_btn_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (audio_rec_is_recording()) {
        audio_rec_stop();
        if (lbl_status) {
            lv_label_set_text(lbl_status, "REC STOPPED");
            lv_obj_set_style_text_color(lbl_status, G, 0);
        }
    } else {
        int idx = 1;
        char path[64];
        while (idx < 1000) {
            snprintf(path, sizeof(path), "/rec/recrd_%d.wav", idx);
            if (!SD.exists(path)) {
                break;
            }
            idx++;
        }
        
        if (audio_rec_start(path)) {
            if (lbl_status) {
                char status_buf[64];
                snprintf(status_buf, sizeof(status_buf), "REC: recrd_%d.wav", idx);
                lv_label_set_text(lbl_status, status_buf);
                lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF0000), 0);
            }
        } else {
            if (lbl_status) {
                lv_label_set_text(lbl_status, "REC ERROR (NO SD?)");
                lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF0000), 0);
            }
        }
    }
}

#undef G
#include <NimBLEDevice.h>
#include <mbedtls/aes.h>
#define G  lv_color_hex(PIPBOY_GREEN)

#define WHISPER_FP_UUID    0xFE2C
#define WHISPER_KBP_UUID   "fe2c1234-8366-4814-8eb0-01de32100bea"
#define WHISPER_MAX_DEV    20
#define WHISPER_LOG_PATH   "/whisper/audit_log.json"

struct WhisperDevice {
    char name[32];
    char address[18];
    char model_id[8];
    int  rssi;
    bool vulnerable;
};

static WhisperDevice whisper_devs[WHISPER_MAX_DEV];
static int           whisper_dev_count  = 0;
static bool          whisper_scanning   = false;
static lv_obj_t*     whisper_modal      = nullptr;
static lv_obj_t*     whisper_list       = nullptr;
static lv_obj_t*     whisper_log_label  = nullptr;
static lv_obj_t*     whisper_status_lbl = nullptr;

static void whisper_sd_init() {
    if (!SD.exists("/whisper")) SD.mkdir("/whisper");
    if (!SD.exists(WHISPER_LOG_PATH)) {
        File f = SD.open(WHISPER_LOG_PATH, FILE_WRITE);
        if (f) { f.print("["); f.close(); }
    }
}

static void whisper_sd_log(const WhisperDevice& d) {
    File f = SD.open(WHISPER_LOG_PATH, FILE_APPEND);
    if (!f) return;
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"name\":\"%s\",\"mac\":\"%s\",\"model_id\":\"%s\","
        "\"rssi\":%d,\"vulnerable\":%s},\n",
        d.name, d.address, d.model_id, d.rssi,
        d.vulnerable ? "true" : "false");
    f.print(buf);
    f.close();
}

static void whisper_append_log(const char* msg) {
    if (!whisper_log_label) return;
    const char* cur = lv_label_get_text(whisper_log_label);
    size_t cur_len = cur ? strlen(cur) : 0;
    size_t msg_len = strlen(msg);
    char* next = (char*)malloc(cur_len + msg_len + 3);
    if (!next) return;
    snprintf(next, cur_len + msg_len + 3, "%s\n%s", cur ? cur : "", msg);
    lv_label_set_text(whisper_log_label, next);
    free(next);
}

static void whisper_test_device(int idx) {
    if (idx < 0 || idx >= whisper_dev_count) return;
    WhisperDevice& dev = whisper_devs[idx];

    char msg[64];
    snprintf(msg, sizeof(msg), "[>] Testing: %s", dev.name);
    whisper_append_log(msg);

    NimBLEClient* pClient = NimBLEDevice::createClient();
    if (!pClient) { whisper_append_log("[-] Client create failed"); return; }
    pClient->setConnectTimeout(8);

    bool connected = false;
    for (int i = 0; i < 3 && !connected; i++) {
        
        NimBLEAddress addr(std::string(dev.address), BLE_ADDR_PUBLIC);
        if (pClient->connect(addr)) connected = pClient->isConnected();
        if (!connected) delay(500);
    }
    if (!connected) {
        whisper_append_log("[-] Connection failed");
        NimBLEDevice::deleteClient(pClient);
        return;
    }
    whisper_append_log("[+] Connected");

    if (!pClient->discoverAttributes()) {
        whisper_append_log("[-] Attr discovery failed");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return;
    }

    NimBLERemoteService* pSvc = pClient->getService(NimBLEUUID((uint16_t)WHISPER_FP_UUID));
    if (!pSvc) {
        whisper_append_log("[-] FP service not found");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return;
    }
    NimBLERemoteCharacteristic* pChar = pSvc->getCharacteristic(NimBLEUUID(WHISPER_KBP_UUID));
    if (!pChar) {
        whisper_append_log("[-] KBP char not found");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return;
    }
    whisper_append_log("[+] KBP char found");

    
    
    uint8_t aesKey[16];
    for (int i = 0; i < 16; i++) aesKey[i] = (uint8_t)esp_random();

    
    uint8_t raw[16] = {};
    raw[0] = 0x00; raw[1] = 0x00;
    const char* addrStr = dev.address;
    int byteIdx = 0;
    for (int i = 0; addrStr[i] && byteIdx < 6; i++) {
        if (addrStr[i] == ':') continue;
        char hi_c = addrStr[i++];
        char lo_c = addrStr[i];
        uint8_t hi = (hi_c >= 'a') ? hi_c-'a'+10 : (hi_c >= 'A') ? hi_c-'A'+10 : hi_c-'0';
        uint8_t lo = (lo_c >= 'a') ? lo_c-'a'+10 : (lo_c >= 'A') ? lo_c-'A'+10 : lo_c-'0';
        raw[2 + byteIdx++] = (hi << 4) | lo;
    }
    for (int i = 0; i < 8; i++) raw[8+i] = (uint8_t)esp_random();

    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aesKey, 128);
    uint8_t encrypted[16];
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, raw, encrypted);
    mbedtls_aes_free(&aes);

    
    volatile bool notif = false;
    if (pChar->canNotify()) {
        pChar->subscribe(true,
            [&notif](NimBLERemoteCharacteristic*, uint8_t*, size_t, bool) {
                notif = true;
            });
    }
    pChar->writeValue(encrypted, 16, true);

    uint32_t t = millis();
    while (millis() - t < 5000 && !notif) delay(50);

    dev.vulnerable = notif;
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);

    if (notif) {
        whisper_append_log("[!] VULNERABLE -- Notif received!");
        snprintf(msg, sizeof(msg), "VULN: %s", dev.name);
        if (whisper_status_lbl) {
            lv_label_set_text(whisper_status_lbl, msg);
            lv_obj_set_style_text_color(whisper_status_lbl, lv_color_hex(0xFF3300), 0);
        }
    } else {
        whisper_append_log("[OK] SAFE -- No response");
        snprintf(msg, sizeof(msg), "SAFE: %s", dev.name);
        if (whisper_status_lbl) {
            lv_label_set_text(whisper_status_lbl, msg);
            lv_obj_set_style_text_color(whisper_status_lbl, G, 0);
        }
    }
    whisper_sd_log(dev);

    
    if (whisper_list) {
        lv_obj_t* item = lv_obj_get_child(whisper_list, idx);
        if (item) {
            lv_obj_t* lbl = lv_obj_get_child(item, 0);
            if (lbl)
                lv_obj_set_style_text_color(lbl,
                    notif ? lv_color_hex(0xFF3300) : G, 0);
        }
    }
}

static void whisper_list_item_cb(lv_event_t* e) {
    haptic_click();
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    whisper_test_device(idx);
}

static void whisper_close_cb(lv_event_t* e) {
    (void)e; haptic_click();
    if (whisper_scanning) {
        NimBLEDevice::getScan()->stop();
        whisper_scanning = false;
    }
    if (whisper_modal) {
        lv_obj_delete(whisper_modal);
        whisper_modal      = nullptr;
        whisper_list       = nullptr;
        whisper_log_label  = nullptr;
        whisper_status_lbl = nullptr;
    }
    whisper_dev_count = 0;
    
    
}

class WhisperScanCB : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (!dev->isAdvertisingService(NimBLEUUID((uint16_t)WHISPER_FP_UUID))) return;
        if (whisper_dev_count >= WHISPER_MAX_DEV) return;

        WhisperDevice& d = whisper_devs[whisper_dev_count];
        lv_strlcpy(d.name, dev->getName().c_str(), sizeof(d.name));
        if (strlen(d.name) == 0) lv_strlcpy(d.name, "FastPair Dev", sizeof(d.name));
        lv_strlcpy(d.address, dev->getAddress().toString().c_str(), sizeof(d.address));
        d.rssi       = dev->getRSSI();
        d.vulnerable = false;

        if (dev->haveServiceData()) {
            std::string sd = dev->getServiceData(NimBLEUUID((uint16_t)WHISPER_FP_UUID));
            if (sd.length() >= 3)
                snprintf(d.model_id, sizeof(d.model_id), "%02X%02X%02X",
                         (uint8_t)sd[0], (uint8_t)sd[1], (uint8_t)sd[2]);
            else
                lv_strlcpy(d.model_id, "------", sizeof(d.model_id));
        } else {
            lv_strlcpy(d.model_id, "------", sizeof(d.model_id));
        }

        if (whisper_list) {
            int cur_idx = whisper_dev_count;
            lv_obj_t* item = lv_obj_create(whisper_list);
            lv_obj_remove_style_all(item);
            lv_obj_set_size(item, lv_pct(100), 36);
            lv_obj_set_style_bg_color(item, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, D, 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_pad_all(item, 4, 0);
            lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);

            char lbl_buf[64];
            snprintf(lbl_buf, sizeof(lbl_buf), "%s [%s] %ddBm",
                     d.name, d.model_id, d.rssi);
            lv_obj_t* lbl = lv_label_create(item);
            lv_label_set_text(lbl, lbl_buf);
            lv_obj_set_style_text_color(lbl, G, 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_add_event_cb(item, whisper_list_item_cb, LV_EVENT_CLICKED,
                                (void*)(intptr_t)cur_idx);
        }
        whisper_dev_count++;
    }
};

static void whisper_scan_cb(lv_event_t* e) {
    (void)e; haptic_click();
    if (whisper_scanning) return;

    
    if (recon_is_ble_scanning()) {
        if (whisper_log_label)
            whisper_append_log("[-] Recon BLE scan active -- tap STOP first");
        if (whisper_status_lbl) {
            lv_label_set_text(whisper_status_lbl, "STOP Recon BLE first");
            lv_obj_set_style_text_color(whisper_status_lbl, lv_color_hex(0xFF9900), 0);
        }
        return;
    }

    whisper_dev_count = 0;
    if (whisper_list)      lv_obj_clean(whisper_list);
    if (whisper_log_label) lv_label_set_text(whisper_log_label, "[*] Scanning 10s...");
    if (whisper_status_lbl) {
        lv_label_set_text(whisper_status_lbl, "SCANNING...");
        lv_obj_set_style_text_color(whisper_status_lbl, G, 0);
    }

    if (!NimBLEDevice::isInitialized()) NimBLEDevice::init("WDGWhisper");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(new WhisperScanCB(), true);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(60);
    pScan->setMaxResults(0);
    whisper_scanning = true;
    pScan->start(10, [](NimBLEScanResults) {
        whisper_scanning = false;
        if (whisper_status_lbl) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Found: %d devices", whisper_dev_count);
            lv_label_set_text(whisper_status_lbl, buf);
            lv_obj_set_style_text_color(whisper_status_lbl, G, 0);
        }
        if (whisper_log_label) {
            char buf2[48];
            snprintf(buf2, sizeof(buf2), "[*] Done. %d FP devices found.", whisper_dev_count);
            whisper_append_log(buf2);
        }
    }, false);
}

static void whisper_btn_cb(lv_event_t* e) {
    (void)e; haptic_click();
    if (whisper_modal) return;

    whisper_sd_init();

    whisper_modal = lv_obj_create(scr);
    lv_obj_set_size(whisper_modal, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(whisper_modal, 0, 0);
    lv_obj_set_style_bg_color(whisper_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(whisper_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(whisper_modal, G, 0);
    lv_obj_set_style_border_width(whisper_modal, 1, 0);
    lv_obj_set_scrollbar_mode(whisper_modal, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(whisper_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(whisper_modal);
    lv_label_set_text(title, "[ WHISPER -- Fast Pair Scanner ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP);

    whisper_status_lbl = lv_label_create(whisper_modal);
    lv_label_set_text(whisper_status_lbl, "READY -- Tap SCAN");
    lv_obj_set_style_text_color(whisper_status_lbl, D, 0);
    lv_obj_set_style_text_font(whisper_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(whisper_status_lbl, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 22);

    const int WB = 110, WH_BTN = 36;
    const int ROW_Y = SAFE_TOP + 44;

    lv_obj_t* btn_scan = lv_button_create(whisper_modal);
    lv_obj_set_size(btn_scan, WB, WH_BTN);
    lv_obj_set_pos(btn_scan, SAFE_LEFT, ROW_Y);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(btn_scan, G, 0);
    lv_obj_set_style_border_width(btn_scan, 1, 0);
    lv_obj_set_style_radius(btn_scan, 4, 0);
    lv_obj_t* sl = lv_label_create(btn_scan);
    lv_label_set_text(sl, LV_SYMBOL_REFRESH " SCAN");
    lv_obj_set_style_text_color(sl, G, 0);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_14, 0);
    lv_obj_center(sl);
    lv_obj_add_event_cb(btn_scan, whisper_scan_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_close = lv_button_create(whisper_modal);
    lv_obj_set_size(btn_close, WB, WH_BTN);
    lv_obj_set_pos(btn_close, SCREEN_WIDTH - SAFE_RIGHT - WB, ROW_Y);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(btn_close, D, 0);
    lv_obj_set_style_border_width(btn_close, 1, 0);
    lv_obj_set_style_radius(btn_close, 4, 0);
    lv_obj_t* cl = lv_label_create(btn_close);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE " CLOSE");
    lv_obj_set_style_text_color(cl, D, 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, 0);
    lv_obj_center(cl);
    lv_obj_add_event_cb(btn_close, whisper_close_cb, LV_EVENT_CLICKED, nullptr);

    const int WHISPER_LIST_Y = ROW_Y + WH_BTN + 6;
    const int WHISPER_LIST_H = 200;
    whisper_list = lv_obj_create(whisper_modal);
    lv_obj_set_size(whisper_list, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, WHISPER_LIST_H);
    lv_obj_set_pos(whisper_list, SAFE_LEFT, WHISPER_LIST_Y);
    lv_obj_set_style_bg_color(whisper_list, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(whisper_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(whisper_list, D, 0);
    lv_obj_set_style_border_width(whisper_list, 1, 0);
    lv_obj_set_style_pad_all(whisper_list, 2, 0);
    lv_obj_set_flex_flow(whisper_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(whisper_list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(whisper_list, 2, 0);
    lv_obj_set_scrollbar_mode(whisper_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(whisper_list, LV_OBJ_FLAG_SCROLLABLE);

    const int LOG_Y = WHISPER_LIST_Y + WHISPER_LIST_H + 4;
    const int LOG_H = SCREEN_HEIGHT - LOG_Y - SAFE_BOTTOM;
    lv_obj_t* log_box = lv_obj_create(whisper_modal);
    lv_obj_set_size(log_box, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, LOG_H);
    lv_obj_set_pos(log_box, SAFE_LEFT, LOG_Y);
    lv_obj_set_style_bg_color(log_box, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(log_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(log_box, D, 0);
    lv_obj_set_style_border_width(log_box, 1, 0);
    lv_obj_set_style_pad_all(log_box, 3, 0);
    lv_obj_set_scrollbar_mode(log_box, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(log_box, LV_OBJ_FLAG_SCROLLABLE);

    whisper_log_label = lv_label_create(log_box);
    lv_label_set_text(whisper_log_label, "Tap SCAN to find Fast Pair devices");
    lv_obj_set_style_text_color(whisper_log_label, D, 0);
    lv_obj_set_style_text_font(whisper_log_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(whisper_log_label, lv_pct(100));
    lv_label_set_long_mode(whisper_log_label, LV_LABEL_LONG_WRAP);
}

#include <ArduinoJson.h>

struct LedSignDevice {
    char ssid[33];
    int rssi;
    char protocol[24];
    char default_pass[32];
};

static LedSignDevice led_devices[20];
static int led_device_count = 0;
static bool led_scanning = false;
static bool led_old_wifi_connected = false;
static String led_old_wifi_ssid;
static String led_old_wifi_pass;
static bool led_was_web_server_active = false;

static char led_sel_ssid[33] = "";
static char led_sel_pass[64] = "";
static char led_sel_proto[24] = "";
static char led_sel_text[128] = "WELCOME";
static int led_sel_effect = 1; 
static int led_sel_speed = 5;

static lv_obj_t* led_modal = nullptr;
static lv_obj_t* led_content_panel = nullptr;
static lv_obj_t* led_status_lbl = nullptr;
static lv_obj_t* led_log_label = nullptr;
static lv_obj_t* led_list = nullptr;
static lv_obj_t* led_kb = nullptr;

static void led_show_step_1();
static void led_show_step_2();
static void led_show_step_3();

static void led_load_password(const char* ssid, char* dest, size_t dest_sz) {
    if (strncmp(ssid, "W60-", 4) == 0 || strncmp(ssid, "W62-", 4) == 0 || strncmp(ssid, "WF1-", 4) == 0 || strncmp(ssid, "WF2-", 4) == 0 || strncmp(ssid, "HD-", 3) == 0) {
        snprintf(dest, dest_sz, "88888888");
    } else if (strncmp(ssid, "ZH-", 3) == 0) {
        snprintf(dest, dest_sz, "12345678");
    } else if (strncmp(ssid, "TF-", 3) == 0) {
        snprintf(dest, dest_sz, "88888888");
    } else {
        snprintf(dest, dest_sz, "88888888");
    }

    if (!SD.exists("/led_ctrl")) {
        SD.mkdir("/led_ctrl");
    }
    if (SD.exists("/led_ctrl/passwords.json")) {
        File f = SD.open("/led_ctrl/passwords.json", FILE_READ);
        if (f) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, f);
            f.close();
            if (!err) {
                if (doc.containsKey(ssid)) {
                    const char* saved = doc[ssid] | "";
                    if (strlen(saved) > 0) {
                        lv_strlcpy(dest, saved, dest_sz);
                    }
                }
            }
        }
    }
}

static void led_save_password(const char* ssid, const char* pass) {
    if (!SD.exists("/led_ctrl")) {
        SD.mkdir("/led_ctrl");
    }
    JsonDocument doc;
    if (SD.exists("/led_ctrl/passwords.json")) {
        File f = SD.open("/led_ctrl/passwords.json", FILE_READ);
        if (f) {
            deserializeJson(doc, f);
            f.close();
        }
    }
    doc[ssid] = pass;
    File f = SD.open("/led_ctrl/passwords.json", FILE_WRITE);
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

static bool led_send_packet(const char* proto, const char* text, int effect, int speed) {
    WiFiClient client;
    const char* ip = "192.168.1.1";
    int port = 10001;
    
    if (strcmp(proto, "Huidu (HD)") == 0) {
        port = 10001;
        ip = "192.168.1.1";
    } else if (strcmp(proto, "Onbon (BX)") == 0) {
        port = 5005;
        ip = "192.168.1.1";
    } else if (strcmp(proto, "PowerLed (TF)") == 0) {
        port = 80;
        ip = "192.168.1.252";
    } else if (strcmp(proto, "Zhonghang (ZH)") == 0) {
        port = 8000;
        ip = "192.168.1.253";
    }
    
    Serial.printf("[LED CTRL] Transmitting payload to %s:%d using %s protocol\n", ip, port, proto);
    if (!client.connect(ip, port)) {
        ip = "192.168.0.1";
        if (!client.connect(ip, port)) {
            ip = "192.168.4.1";
            if (!client.connect(ip, port)) {
                return false;
            }
        }
    }
    
    uint8_t payload[256] = {0};
    int len = 0;
    
    if (strcmp(proto, "Huidu (HD)") == 0) {
        payload[0] = 0x55; payload[1] = 0xAA;
        payload[2] = 0x01;
        payload[3] = (uint8_t)effect;
        payload[4] = (uint8_t)speed;
        payload[5] = strlen(text);
        memcpy(&payload[6], text, payload[5]);
        len = 6 + payload[5];
        uint8_t crc = 0;
        for (int i = 0; i < len; i++) crc += payload[i];
        payload[len] = crc;
        len++;
    } else if (strcmp(proto, "Onbon (BX)") == 0) {
        payload[0] = 0xA5; payload[1] = 0x5A;
        payload[2] = 0x03;
        payload[3] = (uint8_t)effect;
        payload[4] = (uint8_t)speed;
        payload[5] = strlen(text);
        memcpy(&payload[6], text, payload[5]);
        len = 6 + payload[5];
    } else {
        len = snprintf((char*)payload, sizeof(payload), "TEXT=%s&EFFECT=%d&SPEED=%d\r\n", text, effect, speed);
    }
    
    client.write(payload, len);
    delay(200);
    client.stop();
    return true;
}

static void led_show_step_1() {
    if (led_content_panel) {
        lv_obj_clean(led_content_panel);
    }
    if (led_kb) {
        lv_obj_delete(led_kb);
        led_kb = nullptr;
    }
    
    led_list = lv_obj_create(led_content_panel);
    lv_obj_set_size(led_list, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(led_list, 0, 0);
    lv_obj_set_style_bg_color(led_list, BG, 0);
    lv_obj_set_style_bg_opa(led_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(led_list, 0, 0);
    lv_obj_set_style_pad_all(led_list, 0, 0);
    lv_obj_set_flex_flow(led_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(led_list, 4, 0);
    lv_obj_set_scrollbar_mode(led_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(led_list, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < led_device_count; i++) {
        lv_obj_t* item = lv_obj_create(led_list);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, lv_pct(100), 38);
        lv_obj_set_style_bg_color(item, BG, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, D, 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_pad_all(item, 4, 0);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        
        char label_text[128];
        snprintf(label_text, sizeof(label_text), "%s  [%s]  %ddBm",
                 led_devices[i].ssid, led_devices[i].protocol, led_devices[i].rssi);
                 
        lv_obj_t* lbl = lv_label_create(item);
        lv_label_set_text(lbl, label_text);
        lv_obj_set_style_text_color(lbl, G, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
        
        lv_obj_add_event_cb(item, [](lv_event_t* ev) {
            haptic_click();
            int index = (int)(intptr_t)lv_event_get_user_data(ev);
            
            lv_strlcpy(led_sel_ssid, led_devices[index].ssid, sizeof(led_sel_ssid));
            lv_strlcpy(led_sel_proto, led_devices[index].protocol, sizeof(led_sel_proto));
            
            led_load_password(led_sel_ssid, led_sel_pass, sizeof(led_sel_pass));
            
            led_show_step_2();
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    if (led_device_count == 0) {
        lv_obj_t* no_devices_lbl = lv_label_create(led_list);
        lv_label_set_text(no_devices_lbl, "No LED signs found.");
        lv_obj_set_style_text_color(no_devices_lbl, D, 0);
        lv_obj_set_style_text_font(no_devices_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_width(no_devices_lbl, lv_pct(100));
        lv_obj_set_style_text_align(no_devices_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(no_devices_lbl, 40, 0);
    }
}

static void led_show_step_2() {
    if (led_content_panel) {
        lv_obj_clean(led_content_panel);
    }
    if (led_kb) {
        lv_obj_delete(led_kb);
        led_kb = nullptr;
    }
    if (led_status_lbl) {
        lv_label_set_text(led_status_lbl, "WiFi Authentication");
    }
    
    lv_obj_t* info_lbl = lv_label_create(led_content_panel);
    char info_buf[128];
    snprintf(info_buf, sizeof(info_buf), "SSID: %s\nProto: %s", led_sel_ssid, led_sel_proto);
    lv_label_set_text(info_lbl, info_buf);
    lv_obj_set_style_text_color(info_lbl, G, 0);
    lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(info_lbl, LV_ALIGN_TOP_MID, 0, 2);
    
    lv_obj_t* ta_pass = lv_textarea_create(led_content_panel);
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_text(ta_pass, led_sel_pass);
    lv_obj_set_size(ta_pass, 320, 36);
    lv_obj_align(ta_pass, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_set_style_bg_color(ta_pass, BG, 0);
    lv_obj_set_style_text_color(ta_pass, G, 0);
    lv_obj_set_style_border_color(ta_pass, D, 0);
    lv_obj_set_style_border_width(ta_pass, 1, 0);
    
    led_kb = lv_keyboard_create(led_modal);
    lv_obj_set_size(led_kb, SCREEN_WIDTH - 20, 220);
    lv_obj_align(led_kb, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_keyboard_set_textarea(led_kb, ta_pass);

    
    lv_obj_set_style_bg_color(led_kb, BG, 0);
    lv_obj_set_style_bg_opa(led_kb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(led_kb, BG, LV_PART_ITEMS);
    lv_obj_set_style_text_color(led_kb, G, LV_PART_ITEMS);
    lv_obj_set_style_border_color(led_kb, D, LV_PART_ITEMS);
    lv_obj_set_style_border_width(led_kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(led_kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(led_kb, &lv_font_montserrat_16, LV_PART_ITEMS);
    
    lv_obj_t* btn_conn = lv_button_create(led_content_panel);
    lv_obj_set_size(btn_conn, 180, 36);
    lv_obj_align(btn_conn, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_color(btn_conn, BG, 0);
    lv_obj_set_style_border_color(btn_conn, G, 0);
    lv_obj_set_style_border_width(btn_conn, 1, 0);
    lv_obj_t* btn_lbl = lv_label_create(btn_conn);
    lv_label_set_text(btn_lbl, "SAVE & CONNECT");
    lv_obj_set_style_text_color(btn_lbl, G, 0);
    lv_obj_center(btn_lbl);
    
    lv_obj_add_event_cb(btn_conn, [](lv_event_t* ev) {
        haptic_click();
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(ev);
        lv_strlcpy(led_sel_pass, lv_textarea_get_text(ta), sizeof(led_sel_pass));
        
        led_save_password(led_sel_ssid, led_sel_pass);
        
        if (led_status_lbl) {
            lv_label_set_text(led_status_lbl, "CONNECTING...");
            lv_obj_set_style_text_color(led_status_lbl, G, 0);
        }
        
        lv_refr_now(nullptr);
        
        led_was_web_server_active = web_server_is_active();
        if (led_was_web_server_active) {
            web_server_stop();
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            led_old_wifi_connected = true;
            led_old_wifi_ssid = WiFi.SSID();
            led_old_wifi_pass = WiFi.psk();
        } else {
            led_old_wifi_connected = false;
        }
        
        WiFi.disconnect();
        WiFi.begin(led_sel_ssid, led_sel_pass);
        
        int count = 0;
        while (WiFi.status() != WL_CONNECTED && count < 15) {
            delay(500);
            count++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            if (led_status_lbl) {
                lv_label_set_text(led_status_lbl, "CONNECTED!");
                lv_obj_set_style_text_color(led_status_lbl, lv_color_hex(0x00FF66), 0);
            }
            delay(1000);
            led_show_step_3();
        } else {
            if (led_status_lbl) {
                lv_label_set_text(led_status_lbl, "CONN FAILED");
                lv_obj_set_style_text_color(led_status_lbl, lv_color_hex(0xFF3300), 0);
            }
        }
    }, LV_EVENT_CLICKED, ta_pass);
}

struct LedSendData {
    lv_obj_t* ta;
    lv_obj_t* effect;
    lv_obj_t* speed;
};

static void led_show_step_3() {
    if (led_content_panel) {
        lv_obj_clean(led_content_panel);
    }
    if (led_kb) {
        lv_obj_delete(led_kb);
        led_kb = nullptr;
    }
    if (led_status_lbl) {
        lv_label_set_text(led_status_lbl, "Send Text Panel");
        lv_obj_set_style_text_color(led_status_lbl, lv_color_hex(0x00FF66), 0);
    }
    
    lv_obj_t* text_ta = lv_textarea_create(led_content_panel);
    lv_textarea_set_one_line(text_ta, true);
    lv_textarea_set_text(text_ta, led_sel_text);
    lv_obj_set_size(text_ta, 340, 36);
    lv_obj_align(text_ta, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(text_ta, BG, 0);
    lv_obj_set_style_text_color(text_ta, G, 0);
    lv_obj_set_style_border_color(text_ta, D, 0);
    lv_obj_set_style_border_width(text_ta, 1, 0);
    
    lv_obj_t* dd_effect = lv_dropdown_create(led_content_panel);
    lv_dropdown_set_options(dd_effect, "Static Text\nScrolling Text\nFlashing Text");
    lv_dropdown_set_selected(dd_effect, led_sel_effect);
    lv_obj_set_size(dd_effect, 165, 36);
    lv_obj_align(dd_effect, LV_ALIGN_TOP_LEFT, 10, 42);
    lv_obj_set_style_bg_color(dd_effect, BG, 0);
    lv_obj_set_style_text_color(dd_effect, G, 0);
    lv_obj_set_style_border_color(dd_effect, D, 0);
    lv_obj_set_style_border_width(dd_effect, 1, 0);
    
    lv_obj_t* dd_speed = lv_dropdown_create(led_content_panel);
    lv_dropdown_set_options(dd_speed, "Speed: 1\nSpeed: 3\nSpeed: 5\nSpeed: 7\nSpeed: 10");
    int speed_idx = (led_sel_speed <= 1) ? 0 : (led_sel_speed <= 3) ? 1 : (led_sel_speed <= 5) ? 2 : (led_sel_speed <= 7) ? 3 : 4;
    lv_dropdown_set_selected(dd_speed, speed_idx);
    lv_obj_set_size(dd_speed, 165, 36);
    lv_obj_align(dd_speed, LV_ALIGN_TOP_RIGHT, -10, 42);
    lv_obj_set_style_bg_color(dd_speed, BG, 0);
    lv_obj_set_style_text_color(dd_speed, G, 0);
    lv_obj_set_style_border_color(dd_speed, D, 0);
    lv_obj_set_style_border_width(dd_speed, 1, 0);
 
    led_kb = lv_keyboard_create(led_modal);
    lv_obj_set_size(led_kb, SCREEN_WIDTH - 20, 220);
    lv_obj_align(led_kb, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_keyboard_set_textarea(led_kb, text_ta);

    
    lv_obj_set_style_bg_color(led_kb, BG, 0);
    lv_obj_set_style_bg_opa(led_kb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(led_kb, BG, LV_PART_ITEMS);
    lv_obj_set_style_text_color(led_kb, G, LV_PART_ITEMS);
    lv_obj_set_style_border_color(led_kb, D, LV_PART_ITEMS);
    lv_obj_set_style_border_width(led_kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(led_kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(led_kb, &lv_font_montserrat_16, LV_PART_ITEMS);
    
    lv_obj_t* btn_send = lv_button_create(led_content_panel);
    lv_obj_set_size(btn_send, 180, 36);
    lv_obj_align(btn_send, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_color(btn_send, BG, 0);
    lv_obj_set_style_border_color(btn_send, G, 0);
    lv_obj_set_style_border_width(btn_send, 1, 0);
    lv_obj_t* btn_lbl = lv_label_create(btn_send);
    lv_label_set_text(btn_lbl, "SEND TEXT");
    lv_obj_set_style_text_color(btn_lbl, G, 0);
    lv_obj_center(btn_lbl);

    LedSendData* sd = new LedSendData{text_ta, dd_effect, dd_speed};

    lv_obj_add_event_cb(btn_send, [](lv_event_t* ev) {
        haptic_click();
        LedSendData* data = (LedSendData*)lv_event_get_user_data(ev);
        
        lv_strlcpy(led_sel_text, lv_textarea_get_text(data->ta), sizeof(led_sel_text));
        led_sel_effect = lv_dropdown_get_selected(data->effect);
        
        int speed_sel = lv_dropdown_get_selected(data->speed);
        led_sel_speed = (speed_sel == 0) ? 1 : (speed_sel == 1) ? 3 : (speed_sel == 2) ? 5 : (speed_sel == 3) ? 7 : 10;
        
        if (led_status_lbl) {
            lv_label_set_text(led_status_lbl, "SENDING PAYLOAD...");
            lv_obj_set_style_text_color(led_status_lbl, G, 0);
        }
        lv_refr_now(nullptr);
        
        bool ok = led_send_packet(led_sel_proto, led_sel_text, led_sel_effect, led_sel_speed);
        
        if (ok) {
            if (led_status_lbl) {
                lv_label_set_text(led_status_lbl, "SEND SUCCESS!");
                lv_obj_set_style_text_color(led_status_lbl, lv_color_hex(0x00FF66), 0);
            }
        } else {
            if (led_status_lbl) {
                lv_label_set_text(led_status_lbl, "SEND FAILED");
                lv_obj_set_style_text_color(led_status_lbl, lv_color_hex(0xFF3300), 0);
            }
        }
        
        delay(1500);
        delete data;
        
        led_restore_wifi_state();
        
        if (led_modal) {
            lv_obj_delete(led_modal);
            led_modal = nullptr;
            led_content_panel = nullptr;
            led_status_lbl = nullptr;
            led_log_label = nullptr;
            led_list = nullptr;
        }
    }, LV_EVENT_CLICKED, sd);
}

static void led_scan_task(lv_event_t* e) {
    (void)e;
    haptic_click();
    if (led_scanning) return;
    led_scanning = true;
    
    if (led_status_lbl) {
        lv_label_set_text(led_status_lbl, "SCANNING...");
        lv_obj_set_style_text_color(led_status_lbl, G, 0);
    }
    if (led_list) lv_obj_clean(led_list);
    
    lv_obj_t* temp_lbl = lv_label_create(led_content_panel);
    lv_label_set_text(temp_lbl, "Scanning WiFi networks...");
    lv_obj_set_style_text_color(temp_lbl, G, 0);
    lv_obj_center(temp_lbl);
    
    lv_refr_now(nullptr);
    
    int n = WiFi.scanNetworks();
    led_device_count = 0;
    
    lv_obj_delete(temp_lbl);
    
    for (int i = 0; i < n && led_device_count < 20; i++) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        
        const char* proto = nullptr;
        const char* dpass = "88888888";
        
        if (ssid.startsWith("W60-") || ssid.startsWith("W62-") || ssid.startsWith("WF1-") || ssid.startsWith("WF2-") || ssid.startsWith("HD-")) {
            proto = "Huidu (HD)";
            dpass = "88888888";
        } else if (ssid.startsWith("BX-")) {
            proto = "Onbon (BX)";
            dpass = "88888888";
        } else if (ssid.startsWith("TF-")) {
            proto = "PowerLed (TF)";
            dpass = "88888888";
        } else if (ssid.startsWith("ZH-")) {
            proto = "Zhonghang (ZH)";
            dpass = "12345678";
        }
        
        if (proto) {
            LedSignDevice& d = led_devices[led_device_count];
            lv_strlcpy(d.ssid, ssid.c_str(), sizeof(d.ssid));
            d.rssi = rssi;
            lv_strlcpy(d.protocol, proto, sizeof(d.protocol));
            lv_strlcpy(d.default_pass, dpass, sizeof(d.default_pass));
            led_device_count++;
        }
    }
    
    for (int i = 0; i < led_device_count - 1; i++) {
        for (int j = i + 1; j < led_device_count; j++) {
            if (led_devices[i].rssi < led_devices[j].rssi) {
                LedSignDevice tmp = led_devices[i];
                led_devices[i] = led_devices[j];
                led_devices[j] = tmp;
            }
        }
    }
    
    led_show_step_1();
    led_scanning = false;
    
    if (led_status_lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Found: %d signs", led_device_count);
        lv_label_set_text(led_status_lbl, buf);
    }
}

static void led_restore_wifi_state() {
    WiFi.disconnect();
    if (led_was_web_server_active) {
        web_server_init();
    } else if (led_old_wifi_connected) {
        WiFi.begin(led_old_wifi_ssid.c_str(), led_old_wifi_pass.c_str());
    }
}

static void led_close_cb(lv_event_t* e) {
    (void)e; haptic_click();
    
    led_restore_wifi_state();
    
    if (led_modal) {
        lv_obj_delete(led_modal);
        led_modal = nullptr;
        led_content_panel = nullptr;
        led_status_lbl = nullptr;
        led_log_label = nullptr;
        led_list = nullptr;
    }
}

static void led_ctrl_btn_cb(lv_event_t* e) {
    (void)e; haptic_click();
    
    if (recon_is_scanning() || recon_is_deauthing() || recon_is_deauth_detecting()) {
        if (lbl_status) {
            lv_label_set_text(lbl_status, "STOP Recon WiFi first!");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3300), 0);
        }
        return;
    }
    
    if (led_modal) return;

    led_modal = lv_obj_create(scr);
    lv_obj_set_size(led_modal, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(led_modal, 0, 0);
    lv_obj_set_style_bg_color(led_modal, BG, 0);
    lv_obj_set_style_bg_opa(led_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(led_modal, G, 0);
    lv_obj_set_style_border_width(led_modal, 1, 0);
    lv_obj_set_scrollbar_mode(led_modal, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(led_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(led_modal);
    lv_label_set_text(title, "[ LED SIGN CONTROLLER ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP);

    led_status_lbl = lv_label_create(led_modal);
    lv_label_set_text(led_status_lbl, "READY - Tap SCAN");
    lv_obj_set_style_text_color(led_status_lbl, D, 0);
    lv_obj_set_style_text_font(led_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(led_status_lbl, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 22);

    const int BTN_W = 110, BTN_H = 36;
    const int ROW_Y = SAFE_TOP + 44;

    lv_obj_t* btn_scan = lv_button_create(led_modal);
    lv_obj_set_size(btn_scan, BTN_W, BTN_H);
    lv_obj_set_pos(btn_scan, SAFE_LEFT, ROW_Y);
    lv_obj_set_style_bg_color(btn_scan, BG, 0);
    lv_obj_set_style_border_color(btn_scan, G, 0);
    lv_obj_set_style_border_width(btn_scan, 1, 0);
    lv_obj_set_style_radius(btn_scan, 4, 0);
    lv_obj_t* sl = lv_label_create(btn_scan);
    lv_label_set_text(sl, LV_SYMBOL_REFRESH " SCAN");
    lv_obj_set_style_text_color(sl, G, 0);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_14, 0);
    lv_obj_center(sl);
    lv_obj_add_event_cb(btn_scan, led_scan_task, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_close = lv_button_create(led_modal);
    lv_obj_set_size(btn_close, BTN_W, BTN_H);
    lv_obj_set_pos(btn_close, SCREEN_WIDTH - SAFE_RIGHT - BTN_W, ROW_Y);
    lv_obj_set_style_bg_color(btn_close, BG, 0);
    lv_obj_set_style_border_color(btn_close, D, 0);
    lv_obj_set_style_border_width(btn_close, 1, 0);
    lv_obj_set_style_radius(btn_close, 4, 0);
    lv_obj_t* cl = lv_label_create(btn_close);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE " CLOSE");
    lv_obj_set_style_text_color(cl, D, 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, 0);
    lv_obj_center(cl);
    lv_obj_add_event_cb(btn_close, led_close_cb, LV_EVENT_CLICKED, nullptr);

    const int CONTENT_Y = ROW_Y + BTN_H + 6;
    const int CONTENT_H = SCREEN_HEIGHT - CONTENT_Y - SAFE_BOTTOM;
    
    led_content_panel = lv_obj_create(led_modal);
    lv_obj_set_size(led_content_panel, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, CONTENT_H);
    lv_obj_set_pos(led_content_panel, SAFE_LEFT, CONTENT_Y);
    lv_obj_set_style_bg_color(led_content_panel, BG, 0);
    lv_obj_set_style_bg_opa(led_content_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(led_content_panel, D, 0);
    lv_obj_set_style_border_width(led_content_panel, 1, 0);
    lv_obj_set_style_pad_all(led_content_panel, 4, 0);
    lv_obj_clear_flag(led_content_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* init_lbl = lv_label_create(led_content_panel);
    lv_label_set_text(init_lbl, "Tap SCAN to discover local LED signs");
    lv_obj_set_style_text_color(init_lbl, D, 0);
    lv_obj_set_style_text_font(init_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(init_lbl);
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

static void deauth_sniff_selected_cb(lv_event_t *e) {
    haptic_click();
    lv_obj_t* item = (lv_obj_t*)lv_event_get_target(e);
    recon_request_deauth_detect();
    
    
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

    
    lv_obj_t* sniff_btn = lv_list_add_button(list, nullptr, "[ SNIFFING DEAUTH ]");
    lv_obj_set_style_bg_color(sniff_btn, BG, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(sniff_btn, 0), G, 0);
    lv_obj_add_event_cb(sniff_btn, deauth_sniff_selected_cb, LV_EVENT_CLICKED, nullptr);

    for (int i = 0; i < wc; i++) {
        const ReconWiFi* net = recon_get_wifi(i);
        if (!net) continue;
        char buf[128];
        const char* diag = "Open:No PMF";
        if (strcmp(net->auth, "WPA3") == 0 || strcmp(net->auth, "WPA2/3") == 0) {
            diag = "WPA3:PMF Req";
        } else if (strcmp(net->auth, "WPA2") == 0 || strcmp(net->auth, "WPA/2") == 0 || strcmp(net->auth, "WPA") == 0) {
            diag = "WPA2:PMF Opt";
        } else if (strcmp(net->auth, "WEP") == 0) {
            diag = "WEP:No PMF";
        }
        snprintf(buf, sizeof(buf), "%s (CH%d, %ddBm) [%s]", net->ssid, net->channel, net->rssi, diag);
        
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
    
    
    lv_obj_t* btn_start = make_btn(modal, 10, 30, 350, 48, "START BEACON SPAM", beacon_start_cb);
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
    load_custom_coords();
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
    make_btn(scr, x, y, bw, bh, "ARP", arp_btn_cb);
    make_btn(scr, x+bw+7, y, bw, bh, "ADS-B", adsb_btn_cb);
    lv_obj_t* rec_btn = make_btn(scr, x+2*(bw+7), y, bw, bh, "#FF0000 " LV_SYMBOL_BULLET "# REC", rec_btn_cb);
    lv_obj_t* rec_lbl = lv_obj_get_child(rec_btn, 0);
    if (rec_lbl) {
        lv_label_set_recolor(rec_lbl, true);
    }

    y += bh + 7;
    make_btn(scr, x, y, 115, bh, LV_SYMBOL_BLUETOOTH " WHISPER", whisper_btn_cb);
    make_btn(scr, x+125, y, 115, bh, LV_SYMBOL_KEYBOARD " LED CTRL", led_ctrl_btn_cb);

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

    if (audio_rec_is_recording()) {
        const char* fname = audio_rec_get_filename();
        if (strncmp(fname, "/recordings/", 12) == 0) fname += 12;
        char b[64];
        snprintf(b, sizeof(b), "REC: %s", fname);
        lv_label_set_text(lbl_status, b);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF0000), 0);
    } else if (recon_is_scanning() || recon_is_arp_scanning()) {
        const char* arp_msg = recon_is_arp_waiting_wifi() ? "ARP: Connecting..." : "ARP SCANNING...";
        lv_label_set_text(lbl_status, recon_is_arp_scanning() ? arp_msg : "SCANNING...");
        lv_obj_set_style_text_color(lbl_status, G, 0);
    } else if (recon_is_deauthing()) {
        lv_label_set_text(lbl_status, "DEAUTH ACTIVE");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF3300), 0);
        return;
    } else if (recon_is_deauth_detecting()) {
        int count = recon_deauth_detect_count();
        if (count > 0) {
            lv_label_set_text(lbl_status, "Deauth detected");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF9900), 0); 
        } else {
            lv_label_set_text(lbl_status, "DEAUTH DETECTING...");
            lv_obj_set_style_text_color(lbl_status, G, 0);
        }
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
    } else if (recon_is_adsb_tracking()) {
        char b[48];
        snprintf(b, sizeof(b), "ADS-B: %s", recon_get_adsb_status());
        lv_label_set_text(lbl_status, b);
        lv_obj_set_style_text_color(lbl_status, G, 0);
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
    } else if (recon_is_deauth_detecting()) {
        pos += snprintf(buf+pos, sizeof(buf)-pos, "[ DEAUTH DETECTOR ]\n");
        int count = recon_deauth_detect_count();
        pos += snprintf(buf+pos, sizeof(buf)-pos, "Packets sniffed: %d\n", recon_sniffer_packet_count());
        pos += snprintf(buf+pos, sizeof(buf)-pos, "Deauths detected: %d\n\n", count);
        
        int ec = recon_get_deauth_event_count();
        if (ec == 0) {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "No attacks detected yet.\nMonitoring channels...\n");
        } else {
            pos += snprintf(buf+pos, sizeof(buf)-pos, "Last events:\n");
            for (int i = ec - 1; i >= 0; i--) {
                uint8_t src[6], dst[6], bssid[6], subtype;
                uint32_t t;
                if (recon_get_deauth_event(i, src, dst, bssid, &subtype, &t)) {
                    pos += snprintf(buf+pos, sizeof(buf)-pos,
                        " -> Src: %02X:%02X:%02X\n    Dst: %02X:%02X:%02X (sub:%02X)\n",
                        src[0], src[1], src[2], dst[0], dst[1], dst[2], subtype);
                }
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
            pos += snprintf(buf+pos, sizeof(buf)-pos, "#%s Captured: %s#\n", RC_G, recon_et_last_cred());
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
                        "#FF9900 %d. %s (%s) (CAM!)#\n",
                        i+1, n->ssid, strcmp(n->auth, "OPEN") == 0 ? "Unprotected" : "Protected");
                } else {
                    pos += snprintf(buf+pos, sizeof(buf)-pos,
                        " %d. %s (%s)\n",
                        i+1, n->ssid, strcmp(n->auth, "OPEN") == 0 ? "Unprotected" : "Protected");
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
                        pos += snprintf(buf+pos, sizeof(buf)-pos, "#%s  %s %s [%d] (airtag)#\n", RC_G,
                            d->mac, d->name[0] ? d->name : "?", d->rssi);
                    } else if (d->is_flipper) {
                        pos += snprintf(buf+pos, sizeof(buf)-pos, "#%s  %s %s [%d] (flipper)#\n", RC_G,
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
    adsb_destroy_overlay();
    recon_request_stop();
    if (adsb_airport_modal) { lv_obj_delete(adsb_airport_modal); adsb_airport_modal = nullptr; }
    if (adsb_coords_modal) { lv_obj_delete(adsb_coords_modal); adsb_coords_modal = nullptr; }
    ta_custom_lat = ta_custom_lon = kb_coords = nullptr;
    if (kbd_container) {
        lv_obj_delete(kbd_container);
        kbd_container = nullptr;
    }
    ta_input = nullptr;
    original_ta = nullptr;
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_status = lbl_results = nullptr;
}
