#include "watchface.h"
#include "theme.h"
#include <cstdio>
#include <math.h>
#include "../apps/gps_app.h"
#include "../web/web_server.h"
#include "../hal/ble_uart_service.h"
#include "../hal/lora_service.h"

#define G  PIPBOY_GREEN_16
#define D  PIPBOY_GREEN_DIM_16
#define DK PIPBOY_DARK_16
#define BG PIPBOY_BG_16
#define X0 SAFE_LEFT
#define Y0 SAFE_TOP
#define XM (SCREEN_WIDTH - SAFE_RIGHT)
#define YM (SCREEN_HEIGHT - SAFE_BOTTOM)
#define SW (XM - X0)
#define CX (SCREEN_WIDTH / 2)
#define CY (SCREEN_HEIGHT / 2)

static lv_obj_t *scr = nullptr;
static lv_obj_t *parent_scr = nullptr;
static WatchfaceStyle current_style = WF_PIPBOY;

static uint8_t c_hour = 0, c_min = 0, c_sec = 0;
static uint8_t c_day = 1, c_month = 1, c_wday = 0;
static uint8_t c_bat = 0;
static bool c_charging = false;
static bool c_ntp = false, c_wifi = false, c_gps = false;
static uint32_t c_steps = 0;

static lv_obj_t *lbl_time = nullptr;
static lv_obj_t *lbl_seconds = nullptr;
static lv_obj_t *lbl_month = nullptr;
static lv_obj_t *lbl_date = nullptr;
static lv_obj_t *lbl_sync = nullptr;
static lv_obj_t *bar_battery = nullptr;
static lv_obj_t *lbl_battery = nullptr;
static lv_obj_t *lbl_steps_v = nullptr;
static lv_obj_t *lbl_gps = nullptr;
static lv_obj_t *day_labels[7] = {};
static lv_obj_t *day_boxes[7] = {};

static lv_obj_t *m_time = nullptr;
static lv_obj_t *m_date = nullptr;
static lv_obj_t *m_bat = nullptr;
static lv_obj_t *m_sync = nullptr;

static lv_obj_t *a_canvas = nullptr;
static lv_obj_t *a_time_lbl = nullptr;
static lv_obj_t *a_date_lbl = nullptr;

static const char *day_names[] = {"MO","TU","WE","TH","FR","SA","SU"};
static const char *month_names[] = {"JAN","FEB","MAR","APR","MAY","JUN",
    "JUL","AUG","SEP","OCT","NOV","DEC"};
static const char *month_full[] = {"JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE",
    "JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER"};

static void create_pipboy(void) {

    lbl_month = lv_label_create(scr);
    lv_label_set_text(lbl_month, "MARCH");
    lv_obj_set_style_text_color(lbl_month, D, 0);
    lv_obj_set_style_text_font(lbl_month, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_month, X0 + 50, Y0);

    lbl_date = lv_label_create(scr);
    lv_label_set_text(lbl_date, "03-26");
    lv_obj_set_style_text_color(lbl_date, D, 0);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_date, XM - 65, Y0);

    int dw = 42, dh = 18, dg = 4;
    int total_w = 7 * dw + 6 * dg;
    int dx0 = X0 + (SW - total_w) / 2;
    for (int i = 0; i < 7; i++) {
        lv_obj_t *b = lv_obj_create(scr); lv_obj_remove_style_all(b);
        lv_obj_set_size(b, dw, dh); lv_obj_set_pos(b, dx0 + i*(dw+dg), Y0+20);
        lv_obj_set_style_border_color(b, D, 0); lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0); lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        day_boxes[i] = b;
        lv_obj_t *lb = lv_label_create(b);
        lv_label_set_text(lb, day_names[i]);
        lv_obj_set_style_text_color(lb, D, 0);
        lv_obj_set_style_text_font(lb, &lv_font_montserrat_14, 0);
        lv_obj_center(lb); day_labels[i] = lb;
    }

    bar_battery = lv_bar_create(scr);
    lv_obj_set_size(bar_battery, 100, 10); lv_obj_set_pos(bar_battery, X0+50, Y0+46);
    lv_bar_set_range(bar_battery, 0, 100); lv_bar_set_value(bar_battery, 80, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_battery, DK, 0); lv_obj_set_style_bg_opa(bar_battery, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar_battery, G, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_battery, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_battery, 0, 0);
    lv_obj_set_style_radius(bar_battery, 0, LV_PART_INDICATOR);

    lbl_battery = lv_label_create(scr);
    lv_label_set_text(lbl_battery, "80%");
    lv_obj_set_style_text_color(lbl_battery, D, 0);
    lv_obj_set_style_text_font(lbl_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_battery, X0 + 155, Y0+44);

    lv_obj_t *tc = lv_obj_create(scr); lv_obj_remove_style_all(tc);
    lv_obj_set_size(tc, 360, 160); lv_obj_align(tc, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(tc, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(tc, LV_OBJ_FLAG_SCROLLABLE);

    lbl_time = lv_label_create(tc);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_set_style_text_color(lbl_time, G, 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_letter_space(lbl_time, 2, 0);
    lv_obj_center(lbl_time);
    lv_obj_set_style_transform_pivot_x(lbl_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_scale(lbl_time, 640, 0);

    lbl_seconds = lv_label_create(scr);
    lv_label_set_text(lbl_seconds, ":00");
    lv_obj_set_style_text_color(lbl_seconds, D, 0);
    lv_obj_set_style_text_font(lbl_seconds, &lv_font_montserrat_22, 0);
    lv_obj_align(lbl_seconds, LV_ALIGN_CENTER, 0, 55);

    lbl_steps_v = lv_label_create(scr);
    lv_label_set_text(lbl_steps_v, "STEPS: 0");
    lv_obj_set_style_text_color(lbl_steps_v, D, 0);
    lv_obj_set_style_text_font(lbl_steps_v, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_steps_v, X0+30, YM-80);

    lbl_gps = lv_label_create(scr);
    lv_label_set_text(lbl_gps, LV_SYMBOL_GPS " NO FIX");
    lv_obj_set_style_text_color(lbl_gps, D, 0);
    lv_obj_set_style_text_font(lbl_gps, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_gps, LV_ALIGN_BOTTOM_RIGHT, -SAFE_RIGHT, -80);

    lbl_sync = lv_label_create(scr);
    lv_label_set_recolor(lbl_sync, true);
    lv_label_set_text(lbl_sync, "");
    lv_obj_set_style_text_font(lbl_sync, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_sync, LV_ALIGN_BOTTOM_MID, 0, -(SAFE_BOTTOM+5));
}

static void create_minimal(void) {

    m_time = lv_label_create(scr);
    lv_label_set_text(m_time, "00:00");
    lv_obj_set_style_text_color(m_time, G, 0);
    lv_obj_set_style_text_font(m_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_letter_space(m_time, 4, 0);
    lv_obj_align(m_time, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_transform_pivot_x(m_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(m_time, LV_PCT(50), 0);
    lv_obj_set_style_transform_scale(m_time, 640, 0);

    m_date = lv_label_create(scr);
    lv_label_set_text(m_date, "THURSDAY, MARCH 26");
    lv_obj_set_style_text_color(m_date, D, 0);
    lv_obj_set_style_text_font(m_date, &lv_font_montserrat_20, 0);
    lv_obj_align(m_date, LV_ALIGN_CENTER, 0, 40);

    m_bat = lv_label_create(scr);
    lv_label_set_text(m_bat, "80%");
    lv_obj_set_style_text_color(m_bat, D, 0);
    lv_obj_set_style_text_font(m_bat, &lv_font_montserrat_18, 0);
    lv_obj_align(m_bat, LV_ALIGN_TOP_RIGHT, -SAFE_RIGHT-5, Y0);

    m_sync = lv_label_create(scr);
    lv_label_set_text(m_sync, "");
    lv_obj_set_style_text_color(m_sync, D, 0);
    lv_obj_set_style_text_font(m_sync, &lv_font_montserrat_16, 0);
    lv_obj_align(m_sync, LV_ALIGN_BOTTOM_MID, 0, -(SAFE_BOTTOM+5));
}

static lv_point_precise_t hour_pts[2], min_pts[2], sec_pts[2];
static lv_obj_t *line_hour = nullptr, *line_min = nullptr, *line_sec = nullptr;

static void draw_hand(lv_obj_t **line_obj, lv_point_precise_t *pts,
                      float angle_deg, int len, int width, lv_color_t color) {
    float rad = (angle_deg - 90.0f) * 3.14159f / 180.0f;
    pts[0].x = CX; pts[0].y = CY;
    pts[1].x = CX + (int)(cosf(rad) * len);
    pts[1].y = CY + (int)(sinf(rad) * len);

    if (!*line_obj) {
        *line_obj = lv_line_create(scr);
        lv_obj_set_style_line_rounded(*line_obj, true, 0);
    }
    lv_line_set_points(*line_obj, pts, 2);
    lv_obj_set_style_line_width(*line_obj, width, 0);
    lv_obj_set_style_line_color(*line_obj, color, 0);
}

static void create_analog(void) {

    for (int i = 0; i < 12; i++) {
        float rad = (i * 30.0f - 90.0f) * 3.14159f / 180.0f;
        int r1 = 170, r2 = (i % 3 == 0) ? 145 : 155;
        lv_point_precise_t pts[2];
        pts[0].x = CX + (int)(cosf(rad)*r1); pts[0].y = CY + (int)(sinf(rad)*r1);
        pts[1].x = CX + (int)(cosf(rad)*r2); pts[1].y = CY + (int)(sinf(rad)*r2);

        static lv_point_precise_t marker_pts[12][2];
        marker_pts[i][0] = pts[0]; marker_pts[i][1] = pts[1];
        lv_obj_t *l = lv_line_create(scr);
        lv_line_set_points(l, marker_pts[i], 2);
        lv_obj_set_style_line_width(l, (i%3==0)?3:1, 0);
        lv_obj_set_style_line_color(l, (i%3==0)?G:D, 0);
    }

    lv_obj_t *dot = lv_obj_create(scr); lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 10, 10); lv_obj_set_pos(dot, CX-5, CY-5);
    lv_obj_set_style_bg_color(dot, G, 0); lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);

    a_time_lbl = lv_label_create(scr);
    lv_label_set_text(a_time_lbl, "00:00:00");
    lv_obj_set_style_text_color(a_time_lbl, D, 0);
    lv_obj_set_style_text_font(a_time_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(a_time_lbl, LV_ALIGN_CENTER, 0, 100);

    a_date_lbl = lv_label_create(scr);
    lv_label_set_text(a_date_lbl, "MAR 26");
    lv_obj_set_style_text_color(a_date_lbl, D, 0);
    lv_obj_set_style_text_font(a_date_lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(a_date_lbl, LV_ALIGN_CENTER, 0, 125);

    draw_hand(&line_hour, hour_pts, 0, 100, 5, G);
    draw_hand(&line_min, min_pts, 0, 140, 3, G);
    draw_hand(&line_sec, sec_pts, 0, 155, 1, D);
}

static void rebuild_face(void) {
    if (scr) { lv_obj_delete(scr); scr = nullptr; }

    lbl_time = lbl_seconds = lbl_month = lbl_date = lbl_sync = nullptr;
    bar_battery = lbl_battery = lbl_steps_v = lbl_gps = nullptr;
    m_time = m_date = m_bat = m_sync = nullptr;
    a_time_lbl = a_date_lbl = nullptr;
    line_hour = line_min = line_sec = nullptr;
    for (int i = 0; i < 7; i++) { day_labels[i] = nullptr; day_boxes[i] = nullptr; }

    scr = lv_obj_create(parent_scr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    switch (current_style) {
        case WF_PIPBOY:  create_pipboy(); break;
        case WF_MINIMAL: create_minimal(); break;
        case WF_ANALOG:  create_analog(); break;
        default: create_pipboy(); break;
    }
}

void watchface_create(lv_obj_t *parent) {
    parent_scr = parent;
    rebuild_face();
}

void watchface_next(void) {
    current_style = (WatchfaceStyle)((current_style + 1) % WF_COUNT);
    rebuild_face();
}

void watchface_prev(void) {
    current_style = (WatchfaceStyle)((current_style + WF_COUNT - 1) % WF_COUNT);
    rebuild_face();
}

WatchfaceStyle watchface_get_style(void) { return current_style; }

void watchface_set_time(uint8_t hour, uint8_t min) {
    c_hour = hour; c_min = min;
    char b[8]; snprintf(b, sizeof(b), "%02d:%02d", hour, min);

    if (lbl_time) lv_label_set_text(lbl_time, b);
    if (m_time) lv_label_set_text(m_time, b);

    if (line_hour) {
        float h_angle = (hour % 12) * 30.0f + min * 0.5f;
        float m_angle = min * 6.0f;
        draw_hand(&line_hour, hour_pts, h_angle, 100, 5, G);
        draw_hand(&line_min, min_pts, m_angle, 140, 3, G);
    }
    if (a_time_lbl) {
        char tb[12]; snprintf(tb, sizeof(tb), "%02d:%02d:%02d", hour, min, c_sec);
        lv_label_set_text(a_time_lbl, tb);
    }
}

void watchface_set_seconds(uint8_t sec) {
    c_sec = sec;
    if (lbl_seconds) {
        char b[8]; snprintf(b, sizeof(b), ":%02d", sec);
        lv_label_set_text(lbl_seconds, b);
    }
    if (line_sec) {
        float s_angle = sec * 6.0f;
        draw_hand(&line_sec, sec_pts, s_angle, 155, 1, D);
    }
    if (a_time_lbl) {
        char b[12]; snprintf(b, sizeof(b), "%02d:%02d:%02d", c_hour, c_min, sec);
        lv_label_set_text(a_time_lbl, b);
    }
}

static const char *weekday_names[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

void watchface_set_date(uint8_t day, uint8_t month, uint8_t weekday, uint8_t week_num) {
    (void)week_num;
    c_day = day; c_month = month; c_wday = weekday;

    if (lbl_month && month >= 1 && month <= 12)
        lv_label_set_text(lbl_month, month_full[month-1]);
    if (lbl_date) {
        char b[12]; snprintf(b, sizeof(b), "%02d-%02d", month, day);
        lv_label_set_text(lbl_date, b);
    }
    int wd = (weekday == 0) ? 6 : weekday - 1;
    for (int i = 0; i < 7; i++) {
        if (!day_labels[i] || !day_boxes[i]) continue;
        if (i == wd) {
            lv_obj_set_style_text_color(day_labels[i], BG, 0);
            lv_obj_set_style_bg_color(day_boxes[i], G, 0);
            lv_obj_set_style_bg_opa(day_boxes[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_text_color(day_labels[i], D, 0);
            lv_obj_set_style_bg_opa(day_boxes[i], LV_OPA_TRANSP, 0);
        }
    }

    if (m_date && weekday < 7 && month >= 1 && month <= 12) {
        char b[32]; snprintf(b, sizeof(b), "%s, %s %d",
            weekday_names[weekday], month_full[month-1], day);
        lv_label_set_text(m_date, b);
    }

    if (a_date_lbl && month >= 1 && month <= 12) {
        char b[12]; snprintf(b, sizeof(b), "%s %d", month_names[month-1], day);
        lv_label_set_text(a_date_lbl, b);
    }
}

void watchface_set_battery(uint8_t pct, bool charging) {
    c_bat = pct; c_charging = charging;
    if (bar_battery) lv_bar_set_value(bar_battery, pct, LV_ANIM_ON);
    if (lbl_battery) {
        char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
        lv_label_set_text(lbl_battery, b);
    }
    if (m_bat) {
        char b[12]; snprintf(b, sizeof(b), "%s%d%%", charging?LV_SYMBOL_CHARGE:"", pct);
        lv_label_set_text(m_bat, b);
    }
}

void watchface_set_steps(uint32_t steps, uint32_t goal) {
    c_steps = steps;
    if (lbl_steps_v) {
        char b[20]; snprintf(b, sizeof(b), "STEPS: %lu", (unsigned long)steps);
        lv_label_set_text(lbl_steps_v, b);
    }
}

void watchface_set_distance(float km) { (void)km; }

void watchface_set_gps(float lat, float lon, float alt) {
    if (lbl_gps) {
        bool wd = gps_app_is_wardriving_active();
        bool gps_on = gps_app_is_enabled();

        if (!gps_on) {
            lv_label_set_text(lbl_gps, LV_SYMBOL_GPS " OFF");
            lv_obj_set_style_text_color(lbl_gps, D, 0);
            return;
        }

        if (wd) {
            lv_label_set_text(lbl_gps, "wardriving");
            if (isnan(lat) || isnan(lon)) {
                lv_obj_set_style_text_color(lbl_gps, D, 0);
            } else {
                lv_obj_set_style_text_color(lbl_gps, G, 0);
            }
            return;
        }

        char b[64];
        if (isnan(lat) || isnan(lon)) {
            snprintf(b, sizeof(b), LV_SYMBOL_GPS " NO FIX");
            lv_obj_set_style_text_color(lbl_gps, D, 0);
        } else {
            snprintf(b, sizeof(b), LV_SYMBOL_GPS " %.4f,%.4f", lat, lon);
            lv_obj_set_style_text_color(lbl_gps, G, 0);
        }
        lv_label_set_text(lbl_gps, b);
    }
}

void watchface_set_temperature(int16_t temp_c) { (void)temp_c; }

void watchface_set_sync_status(bool wifi, bool ntp_ok, bool gps_fix) {
    c_ntp = ntp_ok; c_wifi = wifi; c_gps = gps_fix;
    char b[128];
    bool ws = web_server_is_active();
    bool wdg = ble_uart_is_active();
    bool mc = lora_svc_is_running();

    snprintf(b, sizeof(b), "%s " LV_SYMBOL_WIFI " %s#  %s " LV_SYMBOL_GPS " %s#  %s " LV_SYMBOL_KEYBOARD " %s#  %s " LV_SYMBOL_BLUETOOTH " %s#  %s " LV_SYMBOL_BULLET " %s#",
        wifi ? "#00e5ff" : "#007280", wifi ? "ON" : "--",
        gps_fix ? "#00e5ff" : "#007280", gps_fix ? "FIX" : "--",
        ws ? "#00e5ff" : "#007280", ws ? "ON" : "--",
        wdg ? "#00e5ff" : "#007280", wdg ? "ON" : "--",
        mc ? "#00e5ff" : "#007280", mc ? "ON" : "--");

    if (lbl_sync) {
        lv_label_set_text(lbl_sync, b);
    }
    if (m_sync) {
        lv_label_set_text(m_sync, ntp_ok ? "NTP OK" : "");
        lv_obj_set_style_text_color(m_sync, ntp_ok ? G : D, 0);
    }
}

void watchface_update(void) {}
void watchface_destroy(void) { if (scr) { lv_obj_delete(scr); scr = nullptr; } }
