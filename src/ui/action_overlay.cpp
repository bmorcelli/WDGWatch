#include "action_overlay.h"
#include "../config.h"
#include <LilyGoLib.h>
#include <cstdio>

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static lv_obj_t *overlay_scr = nullptr;
static lv_obj_t *lbl_action = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_time = nullptr;
static bool active = false;
static uint8_t prev_brightness = 0;

void action_overlay_show(const char *action_name) {
    if (active) {

        if (lbl_action) lv_label_set_text(lbl_action, action_name);
        return;
    }

    prev_brightness = PIPBOY_DEFAULT_BRIGHTNESS;

    overlay_scr = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay_scr);
    lv_obj_set_size(overlay_scr, 410, 502);
    lv_obj_set_style_bg_color(overlay_scr, BG, 0);
    lv_obj_set_style_bg_opa(overlay_scr, LV_OPA_COVER, 0);
    lv_obj_set_pos(overlay_scr, 0, 0);

    lv_obj_add_flag(overlay_scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay_scr, LV_OBJ_FLAG_SCROLLABLE);

    bool is_watchdogs = (strcmp(action_name, "WATCH_DOGS") == 0);
    bool is_pairing   = (strcmp(action_name, "PAIRING") == 0);

    if (is_pairing) {

        lv_obj_t *hdr = lv_label_create(overlay_scr);
        lv_label_set_text(hdr, "BLE PAIRING");
        lv_obj_set_style_text_color(hdr, D, 0);
        lv_obj_set_style_text_font(hdr, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_letter_space(hdr, 2, 0);
        lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 30);

        lv_obj_t *sub = lv_label_create(overlay_scr);
        lv_label_set_text(sub, "Enter PIN on uConsole");
        lv_obj_set_style_text_color(sub, D, 0);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
        lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 60);

        lbl_status = lv_label_create(overlay_scr);
        lv_label_set_text(lbl_status, "------");
        lv_obj_set_style_text_color(lbl_status, G, 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_letter_space(lbl_status, 8, 0);
        lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t *foot = lv_label_create(overlay_scr);
        lv_label_set_text(foot, "WAITING FOR CONFIRM");
        lv_obj_set_style_text_color(foot, D, 0);
        lv_obj_set_style_text_font(foot, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_letter_space(foot, 2, 0);
        lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -(SAFE_BOTTOM + 20));

        instance.setBrightness(PIPBOY_MAX_BRIGHTNESS);
        active = true;
        return;
    }

    if (is_watchdogs) {

        lbl_action = lv_label_create(overlay_scr);
        lv_label_set_text(lbl_action, "WATCH DOGS");
        lv_obj_set_style_text_color(lbl_action, G, 0);
        lv_obj_set_style_text_font(lbl_action, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_letter_space(lbl_action, 3, 0);
        lv_obj_align(lbl_action, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 20);

        lbl_status = lv_label_create(overlay_scr);
        lv_label_set_text(lbl_status,
            "      ___-----___\n"
            "    --             --\n"
            "  /                   \\\n"
            " |  _----_   _----_   |\n"
            " | /      \\ /      \\  |\n"
            " | |      | |      |  |\n"
            " |  \\_  _/   \\_  _/   |\n"
            " |    ~~--   --~~     |\n"
            " |        /V\\        |\n"
            " |       | ^ |       |\n"
            "  \\_      ~~~      _/\n"
            "    ~--_________--~");
        lv_obj_set_style_text_color(lbl_status, G, 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl_status, 380);
        lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_WRAP);
        lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, -5);

        lbl_time = lv_label_create(overlay_scr);
        lv_label_set_text(lbl_time, "--:--");
        lv_obj_set_style_text_color(lbl_time, G, 0);
        lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
        lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, 110);

        lv_obj_t *linked = lv_label_create(overlay_scr);
        lv_label_set_text(linked, "L I N K E D");
        lv_obj_set_style_text_color(linked, D, 0);
        lv_obj_set_style_text_font(linked, &lv_font_montserrat_14, 0);
        lv_obj_align(linked, LV_ALIGN_BOTTOM_MID, 0, -(SAFE_BOTTOM + 15));

        instance.setBrightness(20);
    } else {

        lv_obj_t *hdr = lv_label_create(overlay_scr);
        lv_label_set_text(hdr, "[ IN ACTION ]");
        lv_obj_set_style_text_color(hdr, D, 0);
        lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
        lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 20);

        lbl_action = lv_label_create(overlay_scr);
        lv_label_set_text(lbl_action, action_name);
        lv_obj_set_style_text_color(lbl_action, G, 0);
        lv_obj_set_style_text_font(lbl_action, &lv_font_montserrat_20, 0);
        lv_obj_align(lbl_action, LV_ALIGN_CENTER, 0, -30);

        lbl_status = lv_label_create(overlay_scr);
        lv_label_set_text(lbl_status, "Waiting...");
        lv_obj_set_style_text_color(lbl_status, D, 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_width(lbl_status, 360);
        lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_WRAP);

        lbl_time = lv_label_create(overlay_scr);
        lv_label_set_text(lbl_time, "--:--");
        lv_obj_set_style_text_color(lbl_time, D, 0);
        lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
        lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, 80);

        lv_obj_t *hint = lv_label_create(overlay_scr);
        lv_label_set_text(hint, "Controlled from phone");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x003840), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -(SAFE_BOTTOM + 10));

        instance.setBrightness(40);
    }

    active = true;
}

void action_overlay_set_status(const char *status) {
    if (!active || !lbl_status) return;
    lv_label_set_text(lbl_status, status);

    if (lbl_time) {
        struct tm ti;
        if (getLocalTime(&ti, 0)) {
            char b[8]; snprintf(b, sizeof(b), "%02d:%02d", ti.tm_hour, ti.tm_min);
            lv_label_set_text(lbl_time, b);
        }
    }
}

void action_overlay_hide(void) {
    if (!active) return;
    if (overlay_scr) {
        lv_obj_delete(overlay_scr);
        overlay_scr = nullptr;
    }
    lbl_action = nullptr;
    lbl_status = nullptr;
    lbl_time = nullptr;

    instance.setBrightness(prev_brightness);
    active = false;
}

void action_overlay_set_time(const char *time_str) {
    if (!active || !lbl_time) return;
    lv_label_set_text(lbl_time, time_str);
}

bool action_overlay_is_active(void) {
    return active;
}
