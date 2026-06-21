#include "sensor_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include <cmath>
#include "../config.h"
#include "../hal/power_hal.h"
#include "../hal/haptic.h"
#include <Preferences.h>
#include <bosch/bhy2_parse.h>

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_batt = nullptr;
static lv_obj_t *lbl_gps_sats = nullptr;
static lv_obj_t *lbl_heap = nullptr;
static lv_obj_t *lbl_uptime = nullptr;
static lv_obj_t *lbl_imu = nullptr;
static lv_obj_t *lbl_compass_val = nullptr;

#define COMPASS_R 100
#define NUM_TICKS 36
#define SWEEP_TRAILS 5
static int compass_cx = 0, compass_cy = 0;
static lv_obj_t *dir_labels[4] = {};
static lv_obj_t *deg_labels[12] = {};
static lv_obj_t *tick_lines[NUM_TICKS] = {};
static lv_point_precise_t tick_pts[NUM_TICKS][2];
static lv_obj_t *sweep_lines[SWEEP_TRAILS] = {};
static lv_point_precise_t sweep_trail_pts[SWEEP_TRAILS][2];
static float sweep_angle = 0;

static lv_obj_t *top_marker = nullptr;

static float imu_roll = 0, imu_pitch = 0, imu_heading = 0;
static float raw_yaw = 0;
static float cal_offset = 0;
static bool imu_active = false;

#define G  lv_color_hex(PIPBOY_GREEN)
#define D  lv_color_hex(PIPBOY_GREEN_DIM)
#define BG lv_color_hex(PIPBOY_BG)

static void imu_callback(uint8_t sensor_id, uint8_t *data_ptr, uint32_t len, uint64_t *timestamp, void *user_data) {
    float roll, pitch, yaw;
    bhy2_quaternion_to_euler(data_ptr, &roll, &pitch, &yaw);
    imu_roll = roll;
    imu_pitch = pitch;
    raw_yaw = -yaw;
    imu_heading = raw_yaw + cal_offset;
}

static lv_obj_t *cal_btn_ref = nullptr;

static void calibrate_compass(lv_event_t *e) {
    (void)e; haptic_success();
    cal_offset = -raw_yaw;
    Preferences p;
    if (p.begin("compass", false)) {
        p.putFloat("offset", cal_offset);
        p.end();
    }

    if (cal_btn_ref) lv_obj_add_flag(cal_btn_ref, LV_OBJ_FLAG_HIDDEN);
}

static void load_calibration(void) {
    Preferences p;
    if (p.begin("compass", true)) {
        cal_offset = p.getFloat("offset", 0.0f);
        p.end();
    }
}

static void start_imu(void) {
    if (imu_active) return;

    instance.sensor.configure(SensorBHI260AP::GAME_ROTATION_VECTOR, 100.0f, 0);
    instance.sensor.onResultEvent(SensorBHI260AP::GAME_ROTATION_VECTOR, imu_callback);
    imu_active = true;
}

static void stop_imu(void) {
    if (!imu_active) return;
    instance.sensor.configure(SensorBHI260AP::GAME_ROTATION_VECTOR, 0, 0);
    imu_active = false;
}

static void update_compass_rose(float heading_deg) {
    float offset = -heading_deg;

    for (int i = 0; i < NUM_TICKS; i++) {
        if (!tick_lines[i]) continue;
        float deg = i * 10.0f + offset;
        float a = (deg - 90.0f) * 3.14159f / 180.0f;
        int r1 = COMPASS_R;
        int r2 = (i % 3 == 0) ? COMPASS_R - 14 : COMPASS_R - 7;
        if (i % 9 == 0) r2 = COMPASS_R - 18;
        tick_pts[i][0].x = compass_cx + (int)(cosf(a)*r1);
        tick_pts[i][0].y = compass_cy + (int)(sinf(a)*r1);
        tick_pts[i][1].x = compass_cx + (int)(cosf(a)*r2);
        tick_pts[i][1].y = compass_cy + (int)(sinf(a)*r2);
        lv_line_set_points(tick_lines[i], tick_pts[i], 2);
    }

    const float dir_angles[] = {0, 90, 180, 270};
    for (int i = 0; i < 4; i++) {
        if (!dir_labels[i]) continue;
        float a = (dir_angles[i] + offset - 90.0f) * 3.14159f / 180.0f;
        int lx = compass_cx + (int)(cosf(a) * (COMPASS_R - 30)) - 6;
        int ly = compass_cy + (int)(sinf(a) * (COMPASS_R - 30)) - 8;
        lv_obj_set_pos(dir_labels[i], lx, ly);
    }

    for (int i = 0; i < 12; i++) {
        if (!deg_labels[i]) continue;
        float deg = i * 30.0f + offset;
        float a = (deg - 90.0f) * 3.14159f / 180.0f;
        int lx = compass_cx + (int)(cosf(a) * (COMPASS_R + 14)) - 10;
        int ly = compass_cy + (int)(sinf(a) * (COMPASS_R + 14)) - 6;
        lv_obj_set_pos(deg_labels[i], lx, ly);
    }
}

static void update_radar_sweep(void) {
    sweep_angle += 12.0f;
    if (sweep_angle >= 360.0f) sweep_angle -= 360.0f;

    for (int i = SWEEP_TRAILS - 1; i > 0; i--) {
        if (sweep_lines[i] && sweep_lines[i-1]) {
            sweep_trail_pts[i][0] = sweep_trail_pts[i-1][0];
            sweep_trail_pts[i][1] = sweep_trail_pts[i-1][1];
            lv_line_set_points(sweep_lines[i], sweep_trail_pts[i], 2);
        }
    }

    if (sweep_lines[0]) {
        float srad = (sweep_angle - 90.0f) * 3.14159f / 180.0f;
        sweep_trail_pts[0][0].x = compass_cx;
        sweep_trail_pts[0][0].y = compass_cy;
        sweep_trail_pts[0][1].x = compass_cx + (int)(cosf(srad) * (COMPASS_R - 2));
        sweep_trail_pts[0][1].y = compass_cy + (int)(sinf(srad) * (COMPASS_R - 2));
        lv_line_set_points(sweep_lines[0], sweep_trail_pts[0], 2);
    }
}

void sensor_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    int x = SAFE_LEFT + 5;
    int y = SAFE_TOP;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ SENSOR ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 25;

    lbl_batt = lv_label_create(scr);
    lv_label_set_text(lbl_batt, "BAT: --");
    lv_obj_set_style_text_color(lbl_batt, G, 0);
    lv_obj_set_style_text_font(lbl_batt, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_batt, x, y);

    y += 20;

    lbl_gps_sats = lv_label_create(scr);
    lv_label_set_text(lbl_gps_sats, "GPS: --");
    lv_obj_set_style_text_color(lbl_gps_sats, D, 0);
    lv_obj_set_style_text_font(lbl_gps_sats, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_gps_sats, x, y);

    y += 20;

    lbl_heap = lv_label_create(scr);
    lv_label_set_text(lbl_heap, "HEAP: --");
    lv_obj_set_style_text_color(lbl_heap, D, 0);
    lv_obj_set_style_text_font(lbl_heap, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_heap, x, y);

    y += 16;
    lbl_uptime = lv_label_create(scr);
    lv_label_set_text(lbl_uptime, "UP: --");
    lv_obj_set_style_text_color(lbl_uptime, D, 0);
    lv_obj_set_style_text_font(lbl_uptime, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_uptime, x, y);

    y += 25;
    lv_obj_t *imu_title = lv_label_create(scr);
    lv_label_set_text(imu_title, "IMU (BHI260AP)");
    lv_obj_set_style_text_color(imu_title, D, 0);
    lv_obj_set_style_text_font(imu_title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(imu_title, x, y);

    y += 16;
    lbl_imu = lv_label_create(scr);
    lv_label_set_text(lbl_imu, "Roll: --  Pitch: --");
    lv_obj_set_style_text_color(lbl_imu, G, 0);
    lv_obj_set_style_text_font(lbl_imu, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_imu, x, y);
    lv_obj_set_width(lbl_imu, 360);

    y += 22;
    lbl_compass_val = lv_label_create(scr);
    lv_label_set_text(lbl_compass_val, "HDG: --");
    lv_obj_set_style_text_color(lbl_compass_val, G, 0);
    lv_obj_set_style_text_font(lbl_compass_val, &lv_font_montserrat_22, 0);
    lv_obj_set_pos(lbl_compass_val, x, y);

    y += 15;
    compass_cx = SCREEN_WIDTH / 2;
    compass_cy = y + COMPASS_R + 15;

    uint8_t opacities[] = {180, 120, 70, 35, 15};
    for (int i = SWEEP_TRAILS - 1; i >= 0; i--) {
        sweep_lines[i] = lv_line_create(scr);
        sweep_trail_pts[i][0].x = compass_cx;
        sweep_trail_pts[i][0].y = compass_cy;
        sweep_trail_pts[i][1].x = compass_cx;
        sweep_trail_pts[i][1].y = compass_cy;
        lv_line_set_points(sweep_lines[i], sweep_trail_pts[i], 2);
        lv_obj_set_style_line_width(sweep_lines[i], 2, 0);
        lv_obj_set_style_line_color(sweep_lines[i], G, 0);
        lv_obj_set_style_line_opa(sweep_lines[i], opacities[i], 0);
    }

    for (int i = 0; i < NUM_TICKS; i++) {
        tick_lines[i] = lv_line_create(scr);
        tick_pts[i][0].x = compass_cx; tick_pts[i][0].y = compass_cy;
        tick_pts[i][1].x = compass_cx; tick_pts[i][1].y = compass_cy;
        lv_line_set_points(tick_lines[i], tick_pts[i], 2);
        int w = (i % 9 == 0) ? 2 : (i % 3 == 0) ? 2 : 1;
        lv_obj_set_style_line_width(tick_lines[i], w, 0);
        lv_obj_set_style_line_color(tick_lines[i], (i % 9 == 0) ? G : D, 0);
    }

    for (int i = 0; i < 12; i++) {
        deg_labels[i] = lv_label_create(scr);
        char db[5]; snprintf(db, sizeof(db), "%d", i * 30);
        lv_label_set_text(deg_labels[i], db);
        lv_obj_set_style_text_color(deg_labels[i], D, 0);
        lv_obj_set_style_text_font(deg_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_pos(deg_labels[i], compass_cx, compass_cy);
    }

    const char *dir_texts[] = {"N", "E", "S", "W"};
    lv_color_t dir_colors[] = {lv_color_hex(0xFF3333), G, G, G};
    for (int i = 0; i < 4; i++) {
        dir_labels[i] = lv_label_create(scr);
        lv_label_set_text(dir_labels[i], dir_texts[i]);
        lv_obj_set_style_text_color(dir_labels[i], dir_colors[i], 0);
        lv_obj_set_style_text_font(dir_labels[i], &lv_font_montserrat_22, 0);
        lv_obj_set_pos(dir_labels[i], compass_cx, compass_cy);
    }

    top_marker = lv_label_create(scr);
    lv_label_set_text(top_marker, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(top_marker, lv_color_hex(0xFF3333), 0);
    lv_obj_set_style_text_font(top_marker, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(top_marker, compass_cx - 7, compass_cy - COMPASS_R - 22);

    static lv_point_precise_t ch_pts[2], cv_pts[2];
    ch_pts[0].x = compass_cx - 8; ch_pts[0].y = compass_cy;
    ch_pts[1].x = compass_cx + 8; ch_pts[1].y = compass_cy;
    lv_obj_t *lh = lv_line_create(scr);
    lv_line_set_points(lh, ch_pts, 2);
    lv_obj_set_style_line_width(lh, 1, 0);
    lv_obj_set_style_line_color(lh, D, 0);

    cv_pts[0].x = compass_cx; cv_pts[0].y = compass_cy - 8;
    cv_pts[1].x = compass_cx; cv_pts[1].y = compass_cy + 8;
    lv_obj_t *lv2 = lv_line_create(scr);
    lv_line_set_points(lv2, cv_pts, 2);
    lv_obj_set_style_line_width(lv2, 1, 0);
    lv_obj_set_style_line_color(lv2, D, 0);

    cal_btn_ref = lv_button_create(scr);
    lv_obj_t *cal_btn = cal_btn_ref;
    lv_obj_set_size(cal_btn, 200, 48);
    lv_obj_set_pos(cal_btn, SCREEN_WIDTH/2 - 100, compass_cy + COMPASS_R + 25);
    lv_obj_set_style_bg_color(cal_btn, BG, 0);
    lv_obj_set_style_border_color(cal_btn, D, 0);
    lv_obj_set_style_border_width(cal_btn, 1, 0);
    lv_obj_set_style_radius(cal_btn, 0, 0);
    lv_obj_add_event_cb(cal_btn, calibrate_compass, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cl = lv_label_create(cal_btn);
    lv_label_set_text(cl, "POINT N + CALIBRATE");
    lv_obj_set_style_text_color(cl, D, 0);
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, 0);
    lv_obj_center(cl);

    load_calibration();
    start_imu();
}

void sensor_app_update(void) {
    if (!scr) return;
    char buf[64];

    if (lbl_batt) {
        snprintf(buf, sizeof(buf), "BAT: %.2fV %d%% %s",
            power_hal_battery_voltage(), power_hal_battery_percent(),
            power_hal_is_charging() ? "CHG" : "");
        lv_label_set_text(lbl_batt, buf);
    }

    if (lbl_gps_sats) {
        snprintf(buf, sizeof(buf), "GPS: %d sats %s",
            instance.gps.satellites.value(),
            instance.gps.location.isValid() ? "FIX" : "");
        lv_label_set_text(lbl_gps_sats, buf);
    }

    if (lbl_heap) {
        snprintf(buf, sizeof(buf), "HEAP: %dKB  PSRAM: %dKB",
            ESP.getFreeHeap()/1024, ESP.getFreePsram()/1024);
        lv_label_set_text(lbl_heap, buf);
    }

    if (lbl_uptime) {
        uint32_t s = millis()/1000;
        snprintf(buf, sizeof(buf), "UP: %dh%02dm%02ds", s/3600, (s%3600)/60, s%60);
        lv_label_set_text(lbl_uptime, buf);
    }

    if (lbl_imu && imu_active) {
        snprintf(buf, sizeof(buf), "Roll: %.1f  Pitch: %.1f", imu_roll, imu_pitch);
        lv_label_set_text(lbl_imu, buf);
    }

    if (compass_cx > 0) {

        update_radar_sweep();

        float hdg = fmodf(imu_heading, 360.0f);
        if (hdg < 0) hdg += 360.0f;

        if (imu_active) update_compass_rose(hdg);

        if (lbl_compass_val) {
            const char *dir = "N";
            if (hdg > 337.5 || hdg <= 22.5) dir = "N";
            else if (hdg <= 67.5) dir = "NE";
            else if (hdg <= 112.5) dir = "E";
            else if (hdg <= 157.5) dir = "SE";
            else if (hdg <= 202.5) dir = "S";
            else if (hdg <= 247.5) dir = "SW";
            else if (hdg <= 292.5) dir = "W";
            else dir = "NW";
            snprintf(buf, sizeof(buf), "%03.0f° %s", hdg, dir);
            lv_label_set_text(lbl_compass_val, buf);
        }
    }
}

void sensor_app_destroy(void) {
    stop_imu();
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_batt = lbl_gps_sats = lbl_heap = lbl_uptime = nullptr;
    lbl_imu = lbl_compass_val = nullptr;
    top_marker = nullptr;
    for (int i = 0; i < 4; i++) dir_labels[i] = nullptr;
    for (int i = 0; i < 12; i++) deg_labels[i] = nullptr;
    for (int i = 0; i < NUM_TICKS; i++) tick_lines[i] = nullptr;
    for (int i = 0; i < SWEEP_TRAILS; i++) sweep_lines[i] = nullptr;
    compass_cx = 0;
    cal_btn_ref = nullptr;
}
