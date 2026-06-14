#include "app_common.h"
#include "pet_app.h"
#include "../config.h"
#include "../hal/haptic.h"
#include <Preferences.h>
#include <time.h>

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static lv_obj_t *scr = nullptr;
static Preferences prefs;

static uint32_t pet_level = 1;
static uint32_t pet_xp = 0;
static uint32_t pet_energy = 80;
static uint32_t pet_health = 80;
static uint32_t pet_clean = 80;
static uint32_t pet_poops = 0;

static lv_obj_t *pet_viewport = nullptr;
static lv_obj_t *pet_container = nullptr;
static lv_obj_t *pet_body = nullptr;
static lv_obj_t *pet_face = nullptr;
static lv_obj_t *pet_expression = nullptr;
static lv_obj_t *pet_pins[6] = {};
static lv_obj_t *pet_antenna = nullptr;
static lv_obj_t *pet_tracks = nullptr;
static lv_obj_t *pet_arms[2] = {};
static lv_obj_t *poop_objs[5] = {};

static lv_obj_t *bar_health = nullptr;
static lv_obj_t *bar_energy = nullptr;
static lv_obj_t *bar_clean = nullptr;
static lv_obj_t *bar_xp = nullptr;

static lv_obj_t *lbl_health_val = nullptr;
static lv_obj_t *lbl_energy_val = nullptr;
static lv_obj_t *lbl_clean_val = nullptr;
static lv_obj_t *lbl_xp_val = nullptr;

static lv_obj_t *lbl_console = nullptr;

static uint32_t expression_override_until = 0;
static const char* override_face = nullptr;

static void update_bars(void);
static void update_pet_visuals(void);
static void log_message(const char* msg);

static void load_pet_state(void) {
    bool loaded = false;
    uint32_t last_time = 0;
    if (prefs.begin("scr_pet", true)) { 
        pet_level = prefs.getUInt("level", 1);
        pet_xp = prefs.getUInt("xp", 0);
        pet_energy = prefs.getUInt("energy", 80);
        pet_health = prefs.getUInt("health", 80);
        pet_clean = prefs.getUInt("clean", 80);
        pet_poops = prefs.getUInt("poops", 0);
        last_time = prefs.getUInt("last_time", 0);
        prefs.end();
        loaded = true;
    }

    uint32_t now = time(nullptr);
    if (loaded && last_time > 0 && now > last_time) {
        uint32_t diff_sec = now - last_time;
        uint32_t energy_decay = (diff_sec * 4) / 3600;
        uint32_t clean_decay = (diff_sec * 3) / 3600;

        if (energy_decay >= pet_energy) pet_energy = 0;
        else pet_energy -= energy_decay;

        if (clean_decay >= pet_clean) pet_clean = 0;
        else pet_clean -= clean_decay;

        if (pet_energy == 0 || pet_clean == 0) {
            uint32_t health_decay = (diff_sec * 5) / 3600;
            if (health_decay >= pet_health) pet_health = 10;
            else pet_health -= health_decay;
        }

        if (pet_clean < 50) {
            uint32_t new_poops = (50 - pet_clean) / 10;
            if (new_poops > 5) new_poops = 5;
            if (new_poops > pet_poops) pet_poops = new_poops;
        }
    }
}

static void save_pet_state(void) {
    if (prefs.begin("scr_pet", false)) {
        prefs.putUInt("level", pet_level);
        prefs.putUInt("xp", pet_xp);
        prefs.putUInt("energy", pet_energy);
        prefs.putUInt("health", pet_health);
        prefs.putUInt("clean", pet_clean);
        prefs.putUInt("poops", pet_poops);
        prefs.putUInt("last_time", time(nullptr));
        prefs.end();
    }
}

static void log_message(const char* msg) {
    if (lbl_console) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[SCR-Bit] %s", msg);
        lv_label_set_text(lbl_console, buf);
    }
}

static void set_override_expression(const char* face, uint32_t duration_ms) {
    override_face = face;
    expression_override_until = millis() + duration_ms;
    if (pet_expression) {
        lv_label_set_text(pet_expression, face);
        lv_obj_set_style_text_color(pet_expression, G, 0);
    }
}

static void pet_app_decay(void) {
    if (pet_energy > 0) pet_energy--;
    if (pet_clean > 0) pet_clean--;

    if (pet_energy == 0 || pet_clean == 0) {
        if (pet_health > 10) pet_health--;
    }

    if (pet_clean < 50) {
        uint32_t expected_poops = (50 - pet_clean) / 10;
        if (expected_poops > 5) expected_poops = 5;
        if (expected_poops > pet_poops) {
            pet_poops = expected_poops;
            log_message("Dropped junk cable scrap!");
        }
    }

    update_bars();
    update_pet_visuals();
    save_pet_state();
}

static void feed_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (pet_energy >= 100) {
        log_message("Buffer overflow: Full!");
        return;
    }
    pet_energy = (pet_energy + 20 > 100) ? 100 : pet_energy + 20;
    pet_xp += 10;
    bool evolved = false;
    if (pet_xp >= 100) {
        pet_xp = 0;
        if (pet_level == 1) {
            pet_level = 2;
            evolved = true;
        }
    }

    set_override_expression("*_*", 1500);
    log_message(evolved ? "EVOLVED TO DROID UNIT!" : "FEED: Code packet injected.");
    update_bars();
    update_pet_visuals();
    save_pet_state();
}

static void heal_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (pet_health >= 100) {
        log_message("Systems fully optimized.");
        return;
    }
    pet_health = (pet_health + 15 > 100) ? 100 : pet_health + 15;
    pet_xp += 15;
    bool evolved = false;
    if (pet_xp >= 100) {
        pet_xp = 0;
        if (pet_level == 1) {
            pet_level = 2;
            evolved = true;
        }
    }

    set_override_expression("^‿^", 1500);
    log_message(evolved ? "EVOLVED TO DROID UNIT!" : "HEAL: Kernel calibrated.");
    update_bars();
    update_pet_visuals();
    save_pet_state();
}

static void clean_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    if (pet_clean >= 100 && pet_poops == 0) {
        log_message("FileSystem is clean.");
        return;
    }
    pet_clean = 100;
    pet_poops = 0;

    set_override_expression("^‿^", 1500);
    log_message("CLEAN: Wire scraps purged.");
    update_bars();
    update_pet_visuals();
    save_pet_state();
}

static void status_cb(lv_event_t *e) {
    (void)e;
    haptic_click();
    char buf[64];
    snprintf(buf, sizeof(buf), "LVL:%d | XP:%d%% | HP:%d%% | ENG:%d%%",
             pet_level, pet_xp, pet_health, pet_energy);
    log_message(buf);
}

static lv_obj_t* make_btn(lv_obj_t *par, int x, int y, int w, int h,
                           const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_button_create(par);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, G, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, G, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, 40, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, G, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_center(l);
    return btn;
}

static void style_bar(lv_obj_t *bar) {
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x001C20), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, G, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
}

static void update_bars(void) {
    if (bar_health) lv_bar_set_value(bar_health, pet_health, LV_ANIM_ON);
    if (bar_energy) lv_bar_set_value(bar_energy, pet_energy, LV_ANIM_ON);
    if (bar_clean)  lv_bar_set_value(bar_clean, pet_clean, LV_ANIM_ON);
    if (bar_xp)     lv_bar_set_value(bar_xp, pet_xp, LV_ANIM_ON);

    char buf[16];
    if (lbl_health_val) { snprintf(buf, sizeof(buf), "%d%%", pet_health); lv_label_set_text(lbl_health_val, buf); }
    if (lbl_energy_val) { snprintf(buf, sizeof(buf), "%d%%", pet_energy); lv_label_set_text(lbl_energy_val, buf); }
    if (lbl_clean_val)  { snprintf(buf, sizeof(buf), "%d%%", pet_clean);  lv_label_set_text(lbl_clean_val, buf); }
    if (lbl_xp_val)     { snprintf(buf, sizeof(buf), "%d%%", pet_xp);     lv_label_set_text(lbl_xp_val, buf); }
}

static void update_pet_visuals(void) {
    if (!pet_container) return;

    lv_obj_add_flag(pet_antenna, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_tracks, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 2; i++) lv_obj_add_flag(pet_arms[i], LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 6; i++) lv_obj_add_flag(pet_pins[i], LV_OBJ_FLAG_HIDDEN);

    if (pet_level == 1) {
        lv_obj_set_style_bg_color(pet_body, lv_color_hex(0x002C33), 0);
        lv_obj_set_style_border_color(pet_body, D, 0);
        lv_obj_set_style_radius(pet_body, 2, 0);
        for (int i = 0; i < 6; i++) {
            lv_obj_remove_flag(pet_pins[i], LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_set_style_bg_color(pet_body, lv_color_hex(0x001C20), 0);
        lv_obj_set_style_border_color(pet_body, G, 0);
        lv_obj_set_style_radius(pet_body, 8, 0);

        lv_obj_remove_flag(pet_antenna, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(pet_tracks, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 2; i++) {
            lv_obj_remove_flag(pet_arms[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (!override_face) {
        if (pet_health < 30 || pet_clean < 30) {
            lv_label_set_text(pet_expression, "x_x");
            lv_obj_set_style_text_color(pet_expression, lv_color_hex(0xFF3B3B), 0);
        } else if (pet_energy < 30) {
            lv_label_set_text(pet_expression, "-_-");
            lv_obj_set_style_text_color(pet_expression, D, 0);
        } else {
            lv_label_set_text(pet_expression, "o_o");
            lv_obj_set_style_text_color(pet_expression, G, 0);
        }
    }

    for (int i = 0; i < 5; i++) {
        if (poop_objs[i]) {
            lv_obj_delete(poop_objs[i]);
            poop_objs[i] = nullptr;
        }
    }
    for (uint32_t i = 0; i < pet_poops; i++) {
        poop_objs[i] = lv_label_create(pet_viewport);
        lv_label_set_text(poop_objs[i], "~=");
        lv_obj_set_style_text_color(poop_objs[i], D, 0);
        lv_obj_set_style_text_font(poop_objs[i], &lv_font_montserrat_16, 0);
        lv_obj_set_pos(poop_objs[i], 25 + (i * 35), 145);
    }
}

void pet_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);
    app_add_back_button(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ SCR-PET ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP);

    load_pet_state();

    pet_viewport = lv_obj_create(scr);
    lv_obj_set_size(pet_viewport, 200, 190);
    lv_obj_set_pos(pet_viewport, SAFE_LEFT, SAFE_TOP + 30);
    lv_obj_set_style_bg_color(pet_viewport, BG, 0);
    lv_obj_set_style_border_color(pet_viewport, D, 0);
    lv_obj_set_style_border_width(pet_viewport, 1, 0);
    lv_obj_set_style_radius(pet_viewport, 0, 0);
    lv_obj_clear_flag(pet_viewport, LV_OBJ_FLAG_SCROLLABLE);

    pet_container = lv_obj_create(pet_viewport);
    lv_obj_remove_style_all(pet_container);
    lv_obj_set_size(pet_container, 100, 110);
    lv_obj_align(pet_container, LV_ALIGN_CENTER, 0, -10);

    pet_antenna = lv_obj_create(pet_container);
    lv_obj_set_size(pet_antenna, 2, 16);
    lv_obj_set_pos(pet_antenna, 49, 0);
    lv_obj_set_style_bg_color(pet_antenna, D, 0);
    lv_obj_set_style_border_width(pet_antenna, 0, 0);
    lv_obj_t *led = lv_obj_create(pet_antenna);
    lv_obj_set_size(led, 6, 6);
    lv_obj_set_pos(led, -2, 0);
    lv_obj_set_style_bg_color(led, G, 0);
    lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(led, 0, 0);

    pet_tracks = lv_obj_create(pet_container);
    lv_obj_set_size(pet_tracks, 66, 8);
    lv_obj_set_pos(pet_tracks, 17, 92);
    lv_obj_set_style_bg_color(pet_tracks, D, 0);
    lv_obj_set_style_border_width(pet_tracks, 0, 0);
    lv_obj_set_style_radius(pet_tracks, 2, 0);

    pet_arms[0] = lv_obj_create(pet_container);
    lv_obj_set_size(pet_arms[0], 6, 25);
    lv_obj_set_pos(pet_arms[0], 11, 40);
    lv_obj_set_style_bg_color(pet_arms[0], D, 0);
    lv_obj_set_style_border_width(pet_arms[0], 0, 0);
    lv_obj_set_style_radius(pet_arms[0], 3, 0);

    pet_arms[1] = lv_obj_create(pet_container);
    lv_obj_set_size(pet_arms[1], 6, 25);
    lv_obj_set_pos(pet_arms[1], 83, 40);
    lv_obj_set_style_bg_color(pet_arms[1], D, 0);
    lv_obj_set_style_border_width(pet_arms[1], 0, 0);
    lv_obj_set_style_radius(pet_arms[1], 3, 0);

    pet_body = lv_obj_create(pet_container);
    lv_obj_set_size(pet_body, 66, 66);
    lv_obj_set_pos(pet_body, 17, 20);
    lv_obj_set_style_border_width(pet_body, 1, 0);

    pet_face = lv_obj_create(pet_body);
    lv_obj_set_size(pet_face, 50, 42);
    lv_obj_center(pet_face);
    lv_obj_set_style_bg_color(pet_face, lv_color_hex(0x000F12), 0);
    lv_obj_set_style_border_color(pet_face, D, 0);
    lv_obj_set_style_border_width(pet_face, 1, 0);
    lv_obj_set_style_radius(pet_face, 2, 0);

    pet_expression = lv_label_create(pet_face);
    lv_label_set_text(pet_expression, "o_o");
    lv_obj_set_style_text_color(pet_expression, G, 0);
    lv_obj_set_style_text_font(pet_expression, &lv_font_montserrat_20, 0);
    lv_obj_center(pet_expression);

    int pin_y_offsets[] = {25, 45, 65};
    for (int i = 0; i < 3; i++) {
        pet_pins[i] = lv_obj_create(pet_container);
        lv_obj_set_size(pet_pins[i], 8, 4);
        lv_obj_set_pos(pet_pins[i], 9, pin_y_offsets[i]);
        lv_obj_set_style_bg_color(pet_pins[i], D, 0);
        lv_obj_set_style_border_width(pet_pins[i], 0, 0);
        pet_pins[i+3] = lv_obj_create(pet_container);
        lv_obj_set_size(pet_pins[i+3], 8, 4);
        lv_obj_set_pos(pet_pins[i+3], 83, pin_y_offsets[i]);
        lv_obj_set_style_bg_color(pet_pins[i+3], D, 0);
        lv_obj_set_style_border_width(pet_pins[i+3], 0, 0);
    }

    const int BW = 68;
    const int BH = 60;
    make_btn(scr, 240, SAFE_TOP + 30, BW, BH, "FEED", feed_cb);
    make_btn(scr, 314, SAFE_TOP + 30, BW, BH, "HEAL", heal_cb);
    make_btn(scr, 240, SAFE_TOP + 100, BW, BH, "CLEAN", clean_cb);
    make_btn(scr, 314, SAFE_TOP + 100, BW, BH, "STATUS", status_cb);

    const int BAR_W = 110;
    const int BAR_H = 10;

    lv_obj_t *lbl_hp = lv_label_create(scr);
    lv_label_set_text(lbl_hp, "HP:");
    lv_obj_set_style_text_color(lbl_hp, D, 0);
    lv_obj_set_style_text_font(lbl_hp, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_hp, 25, 275);

    bar_health = lv_bar_create(scr);
    lv_obj_set_size(bar_health, BAR_W, BAR_H);
    lv_obj_set_pos(bar_health, 55, 278);
    style_bar(bar_health);

    lbl_health_val = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_health_val, G, 0);
    lv_obj_set_style_text_font(lbl_health_val, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_health_val, 55 + BAR_W + 5, 276);

    lv_obj_t *lbl_eng = lv_label_create(scr);
    lv_label_set_text(lbl_eng, "ENG:");
    lv_obj_set_style_text_color(lbl_eng, D, 0);
    lv_obj_set_style_text_font(lbl_eng, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_eng, 25, 305);

    bar_energy = lv_bar_create(scr);
    lv_obj_set_size(bar_energy, BAR_W, BAR_H);
    lv_obj_set_pos(bar_energy, 55, 308);
    style_bar(bar_energy);

    lbl_energy_val = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_energy_val, G, 0);
    lv_obj_set_style_text_font(lbl_energy_val, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_energy_val, 55 + BAR_W + 5, 306);

    lv_obj_t *lbl_cln = lv_label_create(scr);
    lv_label_set_text(lbl_cln, "CLN:");
    lv_obj_set_style_text_color(lbl_cln, D, 0);
    lv_obj_set_style_text_font(lbl_cln, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_cln, 215, 275);

    bar_clean = lv_bar_create(scr);
    lv_obj_set_size(bar_clean, BAR_W, BAR_H);
    lv_obj_set_pos(bar_clean, 245, 278);
    style_bar(bar_clean);

    lbl_clean_val = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_clean_val, G, 0);
    lv_obj_set_style_text_font(lbl_clean_val, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_clean_val, 245 + BAR_W + 5, 276);

    lv_obj_t *lbl_xp = lv_label_create(scr);
    lv_label_set_text(lbl_xp, "XP:");
    lv_obj_set_style_text_color(lbl_xp, D, 0);
    lv_obj_set_style_text_font(lbl_xp, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_xp, 215, 305);

    bar_xp = lv_bar_create(scr);
    lv_obj_set_size(bar_xp, BAR_W, BAR_H);
    lv_obj_set_pos(bar_xp, 245, 308);
    style_bar(bar_xp);

    lbl_xp_val = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_xp_val, G, 0);
    lv_obj_set_style_text_font(lbl_xp_val, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_xp_val, 245 + BAR_W + 5, 306);

    lv_obj_t *console_box = lv_obj_create(scr);
    lv_obj_set_size(console_box, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, 50);
    lv_obj_set_pos(console_box, SAFE_LEFT, 345);
    lv_obj_set_style_bg_color(console_box, BG, 0);
    lv_obj_set_style_border_color(console_box, D, 0);
    lv_obj_set_style_border_width(console_box, 1, 0);
    lv_obj_set_style_radius(console_box, 0, 0);
    lv_obj_clear_flag(console_box, LV_OBJ_FLAG_SCROLLABLE);

    lbl_console = lv_label_create(console_box);
    lv_obj_set_style_text_color(lbl_console, G, 0);
    lv_obj_set_style_text_font(lbl_console, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_console, LV_ALIGN_LEFT_MID, 10, 0);

    update_bars();
    update_pet_visuals();
    log_message("System Online.");
}

void pet_app_update(void) {
    if (!scr) return;

    static uint32_t last_decay_ms = 0;
    static uint32_t last_bounce_ms = 0;
    static bool bounce_up = false;
    uint32_t now = millis();

    if (now - last_decay_ms > 15000) {
        last_decay_ms = now;
        pet_app_decay();
    }

    if (now - last_bounce_ms > 400) {
        last_bounce_ms = now;
        bounce_up = !bounce_up;
        if (pet_container) {
            lv_obj_align(pet_container, LV_ALIGN_CENTER, 0, bounce_up ? -13 : -10);
            if (pet_level == 2) {
                if (pet_arms[0]) lv_obj_set_y(pet_arms[0], bounce_up ? 38 : 40);
                if (pet_arms[1]) lv_obj_set_y(pet_arms[1], bounce_up ? 42 : 40);
            }
        }
    }

    if (override_face && now > expression_override_until) {
        override_face = nullptr;
        update_pet_visuals();
    }
}

void pet_app_destroy(void) {
    save_pet_state();

    pet_viewport = nullptr;
    pet_container = nullptr;
    pet_body = nullptr;
    pet_face = nullptr;
    pet_expression = nullptr;
    pet_antenna = nullptr;
    pet_tracks = nullptr;
    pet_arms[0] = nullptr;
    pet_arms[1] = nullptr;
    for (int i = 0; i < 6; i++) pet_pins[i] = nullptr;
    for (int i = 0; i < 5; i++) poop_objs[i] = nullptr;

    bar_health = nullptr;
    bar_energy = nullptr;
    bar_clean = nullptr;
    bar_xp = nullptr;

    lbl_health_val = nullptr;
    lbl_energy_val = nullptr;
    lbl_clean_val = nullptr;
    lbl_xp_val = nullptr;

    lbl_console = nullptr;
    override_face = nullptr;
    expression_override_until = 0;

    if (scr) { lv_obj_delete(scr); scr = nullptr; }
}

void pet_feed_action(void) {
    bool is_running = (scr != nullptr);
    if (!is_running) {
        load_pet_state();
    }
    if (pet_energy < 100) {
        pet_energy = (pet_energy + 20 > 100) ? 100 : pet_energy + 20;
        pet_xp += 10;
        if (pet_xp >= 100) {
            pet_xp = 0;
            if (pet_level == 1) pet_level = 2;
        }
        if (is_running) {
            set_override_expression("*_*", 1500);
            log_message("FEED: Code packet injected.");
            update_bars();
            update_pet_visuals();
        }
    }
    save_pet_state();
}

void pet_heal_action(void) {
    bool is_running = (scr != nullptr);
    if (!is_running) {
        load_pet_state();
    }
    if (pet_health < 100) {
        pet_health = (pet_health + 15 > 100) ? 100 : pet_health + 15;
        pet_xp += 15;
        if (pet_xp >= 100) {
            pet_xp = 0;
            if (pet_level == 1) pet_level = 2;
        }
        if (is_running) {
            set_override_expression("^‿^", 1500);
            log_message("HEAL: Kernel calibrated.");
            update_bars();
            update_pet_visuals();
        }
    }
    save_pet_state();
}

void pet_clean_action(void) {
    bool is_running = (scr != nullptr);
    if (!is_running) {
        load_pet_state();
    }
    pet_clean = 100;
    pet_poops = 0;
    if (is_running) {
        set_override_expression("^‿^", 1500);
        log_message("CLEAN: Wire scraps purged.");
        update_bars();
        update_pet_visuals();
    }
    save_pet_state();
}

void pet_get_stats(uint32_t *level, uint32_t *xp, uint32_t *energy, uint32_t *health, uint32_t *clean, uint32_t *poops) {
    if (scr == nullptr) {
        load_pet_state();
    }
    if (level) *level = pet_level;
    if (xp) *xp = pet_xp;
    if (energy) *energy = pet_energy;
    if (health) *health = pet_health;
    if (clean) *clean = pet_clean;
    if (poops) *poops = pet_poops;
}
