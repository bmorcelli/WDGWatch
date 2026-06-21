#include "recon_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include <vector>
#include <string>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/recon_service.h"
#include "../hal/time_sync.h"
#include <SD.h>
#include "../hal/audio_record.h"
#include <cmath>
#include <cstring>

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
        char sb[96]; snprintf(sb, sizeof(sb), "#007280 %s#", recon_get_adsb_status());
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
    lv_obj_set_style_text_color(adsb_lbl_title, emerg ? lv_color_hex(0xFF3300) : lv_color_hex(0x00E5FF), 0);
    char buf[256]; lv_label_set_recolor(adsb_lbl_data, true);
    switch (adsb_current_page) {
        case 0: snprintf(buf,sizeof(buf),"#00E5FF FLT:# %s\n#00E5FF REG:# %s\n#00E5FF TYPE:# %s\n#00E5FF ROUTE:# %s\n#00E5FF DIST:# %.1f km",ac->flight,ac->reg,ac->type,ac->route,ac->distance); break;
        case 1: snprintf(buf,sizeof(buf),"#00E5FF ALT:# %d ft\n#00E5FF SPD:# %.0f kt\n#00E5FF V/S:# %+d ft/m\n#00E5FF HDG:# %.0f deg",ac->alt_baro,(double)ac->gs,ac->baro_rate,(double)ac->true_heading); break;
        case 2: snprintf(buf,sizeof(buf),"#00E5FF SQUAWK:# %s\n#00E5FF EMERG:# %s",ac->squawk,emerg?"#FF3300 EMERGENCY#":ac->emergency); break;
        default: snprintf(buf,sizeof(buf),"#007280 --- RAW ---#\n#00E5FF FLT:# %-10s\n#00E5FF SQK:# %s\n#00E5FF ALT:# %d\n#00E5FF GS:#  %.0f\n#00E5FF HDG:# %.0f",ac->flight,ac->squawk,ac->alt_baro,(double)ac->gs,(double)ac->true_heading); break;
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
    lv_obj_set_style_text_color(adsb_lbl_title, lv_color_hex(0x00E5FF), 0);
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
        lv_obj_set_style_line_color(adsb_sweep_lines[i], lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_line_width(adsb_sweep_lines[i], (i == 0) ? 2 : 1, 0);
        lv_obj_set_style_line_opa(adsb_sweep_lines[i], opacities[i], 0);
    }

    lv_obj_t* dot = lv_obj_create(adsb_overlay);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_pos(dot, CX-3, CY-3);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);

    adsb_ac_dot = lv_obj_create(adsb_overlay);
    lv_obj_set_size(adsb_ac_dot, 8, 8);
    lv_obj_set_style_bg_color(adsb_ac_dot, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_radius(adsb_ac_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(adsb_ac_dot, 0, 0);
    lv_obj_clear_flag(adsb_ac_dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(adsb_ac_dot, LV_OBJ_FLAG_HIDDEN);

    static lv_point_precise_t sp[2] = {{SAFE_LEFT,300},{(lv_value_precise_t)(410-SAFE_LEFT),300}};
    lv_obj_t* sep = lv_line_create(adsb_overlay);
    lv_line_set_points(sep, sp, 2);
    lv_obj_set_style_line_color(sep, lv_color_hex(0x007280), 0);
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
    lv_obj_set_style_text_color(adsb_lbl_data, lv_color_hex(0x00E5FF), 0);
    lv_label_set_long_mode(adsb_lbl_data, LV_LABEL_LONG_WRAP);
    lv_label_set_text(adsb_lbl_data, "Connecting...");

    int nav_y = 455;
    lv_obj_t* bp = lv_button_create(adsb_overlay);
    lv_obj_set_size(bp, 110, 42); lv_obj_set_pos(bp, SAFE_LEFT+5, nav_y);
    lv_obj_set_style_bg_color(bp, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(bp, lv_color_hex(0x007280), 0);
    lv_obj_set_style_border_width(bp, 1, 0); lv_obj_set_style_radius(bp, 0, 0);
    lv_obj_add_event_cb(bp, adsb_prev_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bpl = lv_label_create(bp); lv_label_set_text(bpl, "< PREV");
    lv_obj_set_style_text_color(bpl, lv_color_hex(0x007280), 0);
    lv_obj_set_style_text_font(bpl, &lv_font_montserrat_16, 0); lv_obj_center(bpl);

    adsb_lbl_page = lv_label_create(adsb_overlay);
    lv_label_set_text(adsb_lbl_page, "PAGE 1 / 4");
    lv_obj_set_style_text_color(adsb_lbl_page, lv_color_hex(0x007280), 0);
    lv_obj_set_style_text_font(adsb_lbl_page, &lv_font_montserrat_16, 0);
    lv_obj_align(adsb_lbl_page, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t* bn = lv_button_create(adsb_overlay);
    lv_obj_set_size(bn, 110, 42); lv_obj_set_pos(bn, 410-SAFE_LEFT-5-110, nav_y);
    lv_obj_set_style_bg_color(bn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(bn, lv_color_hex(0x007280), 0);
    lv_obj_set_style_border_width(bn, 1, 0); lv_obj_set_style_radius(bn, 0, 0);
    lv_obj_add_event_cb(bn, adsb_next_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bnl = lv_label_create(bn); lv_label_set_text(bnl, "NEXT >");
    lv_obj_set_style_text_color(bnl, lv_color_hex(0x007280), 0);
    lv_obj_set_style_text_font(bnl, &lv_font_montserrat_16, 0); lv_obj_center(bnl);

    adsb_radar_timer  = lv_timer_create(adsb_radar_cb,  50,  nullptr);
    adsb_update_timer = lv_timer_create(adsb_update_cb, 500, nullptr);
    recon_request_adsb_track(lat, lon, airport_name);
    adsb_render_page();
}

static lv_obj_t* adsb_airport_modal = nullptr;

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

static void adsb_btn_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (adsb_airport_modal) { lv_obj_delete(adsb_airport_modal); adsb_airport_modal = nullptr; }

    String tz = time_sync_get_timezone();

    adsb_airport_modal = lv_obj_create(scr);
    lv_obj_set_size(adsb_airport_modal, 390, 440);
    lv_obj_align(adsb_airport_modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(adsb_airport_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(adsb_airport_modal, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(adsb_airport_modal, 2, 0);
    lv_obj_set_style_radius(adsb_airport_modal, 0, 0);

    lv_obj_t* hdr = lv_label_create(adsb_airport_modal);
    lv_label_set_text(hdr, "SELECT AIRPORT");
    lv_obj_set_style_text_color(hdr, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_18, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, -12);

    lv_obj_t* list = lv_list_create(adsb_airport_modal);
    lv_obj_set_size(list, 370, 400);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
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
            lv_obj_set_style_text_color(lv_obj_get_child(b, 1), lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_text_font(lv_obj_get_child(b, 1), &lv_font_montserrat_16, 0);
            lv_obj_add_event_cb(b, adsb_airport_select_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            matched++;
        }
    }
    if (matched == 0) {
        for (int i = 0; i < AIRPORT_DB_COUNT; i++) {
            lv_obj_t* b = lv_list_add_button(list, LV_SYMBOL_RIGHT, airport_db[i].name);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x000000), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 1), lv_color_hex(0x007280), 0);
            lv_obj_set_style_text_font(lv_obj_get_child(b, 1), &lv_font_montserrat_16, 0);
            lv_obj_add_event_cb(b, adsb_airport_select_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    }
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
    lv_obj_set_style_text_color(lv_obj_get_child(sniff_btn, 0), lv_color_hex(0x00E5FF), 0);
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
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x00E5FF), 0);
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
    adsb_destroy_overlay();
    recon_request_stop();
    if (adsb_airport_modal) { lv_obj_delete(adsb_airport_modal); adsb_airport_modal = nullptr; }
    if (kbd_container) {
        lv_obj_delete(kbd_container);
        kbd_container = nullptr;
    }
    ta_input = nullptr;
    original_ta = nullptr;
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_status = lbl_results = nullptr;
}
