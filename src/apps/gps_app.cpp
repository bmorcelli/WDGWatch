#include "app_common.h"
#include "gps_app.h"
#include <TinyGPSPlus.h>
#include <LilyGoLib.h>
#include <cstdio>
#include <SD.h>
#include <FS.h>
#include "../config.h"
#include "../hal/haptic.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_lat = nullptr;
static lv_obj_t *lbl_lon = nullptr;
static lv_obj_t *lbl_alt = nullptr;
static lv_obj_t *lbl_speed = nullptr;
static lv_obj_t *lbl_sats = nullptr;
static lv_obj_t *lbl_hdop = nullptr;
static lv_obj_t *lbl_heading = nullptr;
static lv_obj_t *lbl_fix = nullptr;

static lv_obj_t *btn_gps_toggle = nullptr;
static lv_obj_t *btn_wardriving = nullptr;

static bool gps_enabled = false;
static bool wardriving_active = false;
static String wardriving_filepath = "";

#define G lv_color_hex(0x00E5FF)
#define D lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static void update_btn_style(lv_obj_t *btn, bool active) {
    if (!btn) return;
    if (active) {
        lv_obj_set_style_bg_color(btn, G, 0);
        lv_obj_t *l = lv_obj_get_child(btn, 0);
        if (l) {
            lv_obj_set_style_text_color(l, BG, 0);
        }
    } else {
        lv_obj_set_style_bg_color(btn, BG, 0);
        lv_obj_t *l = lv_obj_get_child(btn, 0);
        if (l) {
            lv_obj_set_style_text_color(l, G, 0);
        }
    }
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

static lv_obj_t* row(lv_obj_t *p, int y, const char *title) {
    lv_obj_t *lt = lv_label_create(p);
    lv_label_set_text(lt, title);
    lv_obj_set_style_text_color(lt, D, 0);
    lv_obj_set_style_text_font(lt, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lt, SAFE_LEFT + 10, y);

    lv_obj_t *lv = lv_label_create(p);
    lv_label_set_text(lv, "--");
    lv_obj_set_style_text_color(lv, G, 0);
    lv_obj_set_style_text_font(lv, &lv_font_montserrat_22, 0);
    lv_obj_set_pos(lv, SAFE_LEFT + 100, y - 2);
    return lv;
}

static String get_next_trip_filename() {
    if (!SD.exists("/wardriving")) {
        SD.mkdir("/wardriving");
    }
    int num = 1;
    char path[64];
    while (true) {
        snprintf(path, sizeof(path), "/wardriving/trip_%d.csv", num);
        if (!SD.exists(path)) {
            return String(path);
        }
        num++;
    }
}

static void toggle_gps_cb(lv_event_t *e) {
    (void)e; haptic_click();
    gps_app_set_enabled(!gps_enabled);
}

static void toggle_wardriving_cb(lv_event_t *e) {
    (void)e; haptic_click();

    if (!wardriving_active && !gps_enabled) {
        gps_app_set_enabled(true);
    }

    wardriving_active = !wardriving_active;
    if (wardriving_active) {
        if (!SD.exists("/")) {
            wardriving_active = false;
            lv_obj_t *lbl = lv_obj_get_child(btn_wardriving, 0);
            if (lbl) {
                lv_label_set_text(lbl, "NO SD CARD");
            }
            update_btn_style(btn_wardriving, false);
            return;
        }

        wardriving_filepath = get_next_trip_filename();
        File f = SD.open(wardriving_filepath, FILE_WRITE);
        if (f) {
            f.println("Date,Time,Latitude,Longitude,Altitude,Speed,Heading,Satellites,HDOP");
            f.close();
            Serial.printf("[GPS] Wardriving started: %s\n", wardriving_filepath.c_str());
        } else {
            wardriving_active = false;
            lv_obj_t *lbl = lv_obj_get_child(btn_wardriving, 0);
            if (lbl) {
                lv_label_set_text(lbl, "WRITE ERROR");
            }
            update_btn_style(btn_wardriving, false);
            return;
        }
    } else {
        Serial.println("[GPS] Wardriving stopped");
    }

    lv_obj_t *lbl = lv_obj_get_child(btn_wardriving, 0);
    if (lbl) {
        lv_label_set_text(lbl, "WARDRIVING");
    }
    update_btn_style(btn_wardriving, wardriving_active);
}

void gps_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);
    app_add_back_button(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ GPS TRACKER ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP);

    btn_gps_toggle = make_btn(scr, SAFE_LEFT + 20, SAFE_TOP + 30, 340, 45, "GPS", toggle_gps_cb);
    update_btn_style(btn_gps_toggle, gps_enabled);

    int y = SAFE_TOP + 85;
    lbl_fix    = row(scr, y, "FIX");       y += 35;
    lbl_sats   = row(scr, y, "SATS");      y += 35;
    lbl_lat    = row(scr, y, "LAT");       y += 35;
    lbl_lon    = row(scr, y, "LON");       y += 35;
    lbl_alt    = row(scr, y, "ALT");       y += 35;
    lbl_speed  = row(scr, y, "SPEED");     y += 35;
    lbl_heading= row(scr, y, "HDG");       y += 35;
    lbl_hdop   = row(scr, y, "HDOP");

    btn_wardriving = make_btn(scr, SAFE_LEFT + 20, 420, 340, 45, "WARDRIVING", toggle_wardriving_cb);
    update_btn_style(btn_wardriving, wardriving_active);
}

void gps_app_update(void) {
    if (!scr || !lbl_fix) return;
    char b[32];

    if (!gps_enabled) {
        lv_label_set_text(lbl_fix, "OFF");
        lv_label_set_text(lbl_lat, "--");
        lv_label_set_text(lbl_lon, "--");
        lv_label_set_text(lbl_alt, "--");
        lv_label_set_text(lbl_speed, "--");
        lv_label_set_text(lbl_heading, "--");
        lv_label_set_text(lbl_sats, "--");
        lv_label_set_text(lbl_hdop, "--");
        return;
    }

    bool has_location = instance.gps.location.isValid();
    if (has_location) {
        lv_label_set_text(lbl_fix, "3D FIX");
        snprintf(b, sizeof(b), "%.6f", instance.gps.location.lat());
        lv_label_set_text(lbl_lat, b);
        snprintf(b, sizeof(b), "%.6f", instance.gps.location.lng());
        lv_label_set_text(lbl_lon, b);
    } else {
        lv_label_set_text(lbl_fix, "NO FIX");
        lv_label_set_text(lbl_lat, "--");
        lv_label_set_text(lbl_lon, "--");
    }

    if (instance.gps.altitude.isValid()) {
        snprintf(b, sizeof(b), "%.1f m", instance.gps.altitude.meters());
        lv_label_set_text(lbl_alt, b);
    } else {
        lv_label_set_text(lbl_alt, "--");
    }
    if (instance.gps.speed.isValid()) {
        snprintf(b, sizeof(b), "%.1f km/h", instance.gps.speed.kmph());
        lv_label_set_text(lbl_speed, b);
    } else {
        lv_label_set_text(lbl_speed, "--");
    }
    if (instance.gps.course.isValid()) {
        snprintf(b, sizeof(b), "%.1f°", instance.gps.course.deg());
        lv_label_set_text(lbl_heading, b);
    } else {
        lv_label_set_text(lbl_heading, "--");
    }
    snprintf(b, sizeof(b), "%d", instance.gps.satellites.value());
    lv_label_set_text(lbl_sats, b);
    snprintf(b, sizeof(b), "%.1f", instance.gps.hdop.hdop());
    lv_label_set_text(lbl_hdop, b);

    if (wardriving_active && has_location && instance.gps.location.isUpdated()) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "%04d-%02d-%02d,%02d:%02d:%02d,%.6f,%.6f,%.1f,%.1f,%.1f,%d,%.1f\n",
                 instance.gps.date.year(), instance.gps.date.month(), instance.gps.date.day(),
                 instance.gps.time.hour(), instance.gps.time.minute(), instance.gps.time.second(),
                 instance.gps.location.lat(), instance.gps.location.lng(),
                 instance.gps.altitude.meters(), instance.gps.speed.kmph(),
                 instance.gps.course.deg(), instance.gps.satellites.value(),
                 instance.gps.hdop.hdop());

        File f = SD.open(wardriving_filepath, FILE_APPEND);
        if (f) {
            f.print(buf);
            f.close();
        }
    }

    static uint32_t last_print = 0;
    if (millis() - last_print > 5000) {
        last_print = millis();
        Serial.printf("[GPS] Sats: %d, Lat: %.6f, Lon: %.6f, Alt: %.1fm, Fix: %s, Chars: %u\n",
                      instance.gps.satellites.value(),
                      instance.gps.location.lat(),
                      instance.gps.location.lng(),
                      instance.gps.altitude.meters(),
                      instance.gps.location.isValid() ? "3D FIX" : "NO FIX",
                      instance.gps.charsProcessed());
    }
}

void gps_app_destroy(void) {
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_lat = lbl_lon = lbl_alt = lbl_speed = nullptr;
    lbl_sats = lbl_hdop = lbl_heading = lbl_fix = nullptr;
    btn_gps_toggle = nullptr;
    btn_wardriving = nullptr;
}

bool gps_app_is_enabled(void) {
    return gps_enabled;
}

void gps_app_set_enabled(bool enabled) {
    if (gps_enabled == enabled) return;
    gps_enabled = enabled;
    instance.powerControl(POWER_GPS, gps_enabled);
    if (gps_enabled) {
        instance.gps.init(&Serial1);
        Serial.println("[GPS] Powered ON & Initialized");
    } else {
        Serial.println("[GPS] Powered OFF");
    }
    if (!gps_enabled && wardriving_active) {
        wardriving_active = false;
        if (btn_wardriving) {
            lv_obj_t *w_lbl = lv_obj_get_child(btn_wardriving, 0);
            if (w_lbl) {
                lv_label_set_text(w_lbl, "WARDRIVING");
            }
            update_btn_style(btn_wardriving, false);
        }
    }
    if (btn_gps_toggle) {
        update_btn_style(btn_gps_toggle, gps_enabled);
    }
}

bool gps_app_is_wardriving_active(void) {
    return wardriving_active;
}

void gps_app_set_wardriving(bool active) {
    if (wardriving_active == active) return;
    if (active) {
        if (!gps_enabled) {
            gps_app_set_enabled(true);
        }
        if (!SD.exists("/")) {
            Serial.println("[GPS] Wardriving start failed: No SD card");
            return;
        }
        wardriving_filepath = get_next_trip_filename();
        File f = SD.open(wardriving_filepath, FILE_WRITE);
        if (f) {
            f.println("Date,Time,Latitude,Longitude,Altitude,Speed,Heading,Satellites,HDOP");
            f.close();
            wardriving_active = true;
            Serial.printf("[GPS] Wardriving started: %s\n", wardriving_filepath.c_str());
        }
    } else {
        wardriving_active = false;
        Serial.println("[GPS] Wardriving stopped");
    }
    if (btn_wardriving) {
        update_btn_style(btn_wardriving, wardriving_active);
        lv_obj_t *lbl = lv_obj_get_child(btn_wardriving, 0);
        if (lbl) {
            lv_label_set_text(lbl, "WARDRIVING");
        }
    }
}

