#include <Arduino.h>
#include "app_manager.h"
#include "ui/watchface.h"
#include "ui/theme.h"
#include "apps/gps_app.h"
#include "apps/lora_app.h"
#include "apps/nfc_app.h"
#include "apps/sensor_app.h"
#include "apps/recon_app.h"
#include "apps/wifi_app.h"
#include "apps/hid_app.h"
#include "apps/rf_app.h"
#include "apps/pet_app.h"
#include "apps/settings_app.h"
#include "apps/tools_app.h"
#include "apps/audio_app.h"
#include "hal/rf_service.h"
#include "hal/haptic.h"
#include <SD.h>
#if LV_USE_GIF
#include "widgets/gif/lv_gif.h"
#endif

static AppId current_app = APP_WATCHFACE;
static AppId previous_app = APP_WATCHFACE;
static lv_obj_t *scr_watchface = nullptr;
static lv_obj_t *scr_menu = nullptr;
static lv_obj_t *scr_app = nullptr;
static bool app_transitioning = false;

static const char *menu_items[] = {
    LV_SYMBOL_GPS "\nGPS",
    LV_SYMBOL_ENVELOPE "\nLoRa",
    LV_SYMBOL_SD_CARD "\nNFC",
    LV_SYMBOL_REFRESH "\nSensor",
    LV_SYMBOL_EYE_OPEN "\nRecon",
    LV_SYMBOL_WIFI "\nWiFi",
    LV_SYMBOL_KEYBOARD "\nHID",
    LV_SYMBOL_VOLUME_MAX "\nRF",
    LV_SYMBOL_IMAGE "\nPET",
};
static const AppId menu_app_ids[] = {
    APP_GPS,  APP_LORA, APP_NFC,
    APP_SENSOR, APP_RECON, APP_WIFI,
    APP_HID,  APP_RF,   APP_PET,
};
static const int MENU_ITEM_COUNT = sizeof(menu_app_ids) / sizeof(menu_app_ids[0]);

static void cleanup_app(void) {
    if (scr_app == nullptr) return;

    switch (current_app) {
        case APP_GPS:      gps_app_destroy();  break;
        case APP_LORA:     lora_app_destroy(); break;
        case APP_NFC:      nfc_app_destroy();  break;
        case APP_SENSOR:   sensor_app_destroy(); break;
        case APP_RECON:    recon_app_destroy(); break;
        case APP_WIFI:     wifi_app_destroy(); break;
        case APP_HID:      hid_app_destroy();  break;
        case APP_RF:       rf_app_destroy();   break;
        case APP_PET:      pet_app_destroy();  break;
        case APP_SETTINGS: settings_app_destroy(); break;
        case APP_TOOLS:    tools_app_destroy(); break;
        case APP_AUDIO:    audio_app_destroy(); break;
        default: break;
    }

    lv_obj_delete(scr_app);
    scr_app = nullptr;
}

static void launch_app(AppId app) {

    cleanup_app();

    scr_app = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_app, PIPBOY_BG_16, 0);
    lv_obj_set_style_bg_opa(scr_app, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr_app, LV_SCROLLBAR_MODE_OFF);

    switch (app) {
        case APP_GPS:      gps_app_create(scr_app);    break;
        case APP_LORA:     lora_app_create(scr_app);   break;
        case APP_NFC:      nfc_app_create(scr_app);    break;
        case APP_SENSOR:   sensor_app_create(scr_app); break;
        case APP_RECON:    recon_app_create(scr_app);  break;
        case APP_WIFI:     wifi_app_create(scr_app);   break;
        case APP_HID:      hid_app_create(scr_app);    break;
        case APP_RF:       rf_app_create(scr_app);     break;
        case APP_PET:      pet_app_create(scr_app);    break;
        case APP_SETTINGS: settings_app_create(scr_app); break;
        case APP_TOOLS:    tools_app_create(scr_app); break;
        case APP_AUDIO:    audio_app_create(scr_app); break;
        default: break;
    }

    lv_obj_add_event_cb(scr_app, [](lv_event_t *e) {
        if (current_app == APP_BOOT) return;
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_RIGHT || dir == LV_DIR_BOTTOM) {
            app_manager_back();
        }
    }, LV_EVENT_GESTURE, nullptr);

    lv_screen_load(scr_app);
    current_app = app;
    haptic_click();
}

static void watchface_gesture_cb(lv_event_t *e) {
    if (current_app == APP_BOOT) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT) {
        watchface_next();
        haptic_click();
    } else if (dir == LV_DIR_RIGHT) {
        watchface_prev();
        haptic_click();
    } else {
        app_manager_handle_gesture(dir);
    }
}

static void menu_btn_cb(lv_event_t *e) {
    if (app_transitioning) return;
    AppId *id = (AppId *)lv_event_get_user_data(e);
    if (id) app_manager_show(*id);
}

static void create_menu_screen(void) {
    scr_menu = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_menu, PIPBOY_BG_16, 0);
    lv_obj_set_style_bg_opa(scr_menu, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr_menu, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(scr_menu);
    lv_label_set_text(title, "[ SCR TERMINAL ]");
    lv_obj_set_style_text_color(title, PIPBOY_GREEN_16, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP);

    const int BTN_W = 110;
    const int BTN_H = 110;

    lv_obj_t *grid = lv_obj_create(scr_menu);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, 350);
    lv_obj_set_pos(grid, SAFE_LEFT, SAFE_TOP + 25);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

    static AppId btn_ids[APP_COUNT];

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        btn_ids[i] = menu_app_ids[i];

        lv_obj_t *btn = lv_obj_create(grid);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, BTN_W, BTN_H);
        lv_obj_set_style_bg_color(btn, PIPBOY_BG_16, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, PIPBOY_GREEN_DIM_16, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, PIPBOY_GREEN_16, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, 50, LV_STATE_PRESSED);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, menu_items[i]);
        lv_obj_set_style_text_color(lbl, PIPBOY_GREEN_16, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, menu_btn_cb, LV_EVENT_CLICKED, &btn_ids[i]);
    }

    static AppId bot_ids[] = { APP_TOOLS, APP_SETTINGS };
    static const char* bot_labels[] = { "TOOLS", "SETTINGS" };

    for (int i = 0; i < 2; i++) {
        lv_obj_t *btn = lv_button_create(scr_menu);
        lv_obj_set_size(btn, 160, 40);
        lv_obj_set_pos(btn, SAFE_LEFT + i * 200, 418);
        lv_obj_set_style_bg_color(btn, PIPBOY_BG_16, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, PIPBOY_GREEN_DIM_16, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, PIPBOY_GREEN_16, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, 50, LV_STATE_PRESSED);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, bot_labels[i]);
        lv_obj_set_style_text_color(lbl, PIPBOY_GREEN_16, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, menu_btn_cb, LV_EVENT_CLICKED, &bot_ids[i]);
    }

    lv_obj_add_event_cb(scr_menu, [](lv_event_t *e) {
        if (current_app == APP_BOOT) return;
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_BOTTOM) app_manager_show(APP_WATCHFACE);
    }, LV_EVENT_GESTURE, nullptr);
}

static lv_obj_t *scr_boot = nullptr;
static lv_obj_t *boot_term_label = nullptr;
static lv_obj_t *boot_progress_bar = nullptr;
static lv_obj_t *boot_progress_label = nullptr;
static lv_timer_t *boot_timer = nullptr;
static int boot_step = 0;

static const char* boot_messages[] = {
    "SCR OS v1.1.0\nInitializing core system...",
    "CPU: ESP32-S3 @ 240MHz\nCores: 2 (Xtensa LX7)",
    "MEM: Testing system memory...\nSRAM: 320KB [OK]",
    "MEM: Testing PSRAM...\nPSRAM: 8MB QSPI [OK]",
    "HW: Detecting on-board peripherals...\nLoRa: SX1262 [OK]\nNFC: ST25R3916 [OK]\nIMU: BHI260 [OK]",
    "SD: Mounting microSD card...\nInterface: SPI\nStatus: Mounted",
    "SD: Verifying storage filesystem...\nFATFS: OK",
    "SYSTEM: Starting Watchface engine...\nWelcome back, Agent.",
    ""
};

static void boot_timer_cb(lv_timer_t *timer) {
    boot_step++;
    int progress = (boot_step * 100) / 8;
    if (progress > 100) progress = 100;

    if (boot_progress_bar) {
        lv_bar_set_value(boot_progress_bar, progress, LV_ANIM_ON);
    }
    if (boot_progress_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", progress);
        lv_label_set_text(boot_progress_label, buf);
    }

    if (boot_step < 8) {
        if (boot_term_label) {
            lv_label_set_text(boot_term_label, boot_messages[boot_step]);
        }
    } else {

        lv_timer_delete(boot_timer);
        boot_timer = nullptr;

        app_manager_show(APP_WATCHFACE);
    }
}

void app_manager_init(void) {
    pipboy_theme_init();

    bool custom_boot_file = false;
    if (SD.exists("/custom_boot.txt")) {
        custom_boot_file = true;
    }

    if (!custom_boot_file) {

        scr_boot = lv_obj_create(nullptr);
        lv_obj_set_style_bg_color(scr_boot, PIPBOY_BG_16, 0);
        lv_obj_set_style_bg_opa(scr_boot, LV_OPA_COVER, 0);
        lv_obj_set_scrollbar_mode(scr_boot, LV_SCROLLBAR_MODE_OFF);

        boot_term_label = lv_label_create(scr_boot);
        lv_obj_set_width(boot_term_label, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT);
        lv_obj_align(boot_term_label, LV_ALIGN_TOP_LEFT, SAFE_LEFT, SAFE_TOP + 10);
        lv_obj_set_style_text_color(boot_term_label, PIPBOY_GREEN_16, 0);
        lv_obj_set_style_text_font(boot_term_label, &lv_font_montserrat_18, 0);
        lv_label_set_text(boot_term_label, boot_messages[0]);

        boot_progress_bar = lv_bar_create(scr_boot);
        lv_obj_set_size(boot_progress_bar, SCREEN_WIDTH - SAFE_LEFT - SAFE_RIGHT, 15);
        lv_obj_align(boot_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -SAFE_BOTTOM - 30);
        lv_bar_set_range(boot_progress_bar, 0, 100);
        lv_bar_set_value(boot_progress_bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(boot_progress_bar, PIPBOY_DARK_16, 0);
        lv_obj_set_style_bg_opa(boot_progress_bar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(boot_progress_bar, PIPBOY_GREEN_16, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(boot_progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(boot_progress_bar, 0, 0);
        lv_obj_set_style_radius(boot_progress_bar, 0, LV_PART_INDICATOR);

        boot_progress_label = lv_label_create(scr_boot);
        lv_obj_align_to(boot_progress_label, boot_progress_bar, LV_ALIGN_OUT_TOP_MID, 0, -5);
        lv_obj_set_style_text_color(boot_progress_label, PIPBOY_GREEN_16, 0);
        lv_obj_set_style_text_font(boot_progress_label, &lv_font_montserrat_18, 0);
        lv_label_set_text(boot_progress_label, "0%");

        lv_screen_load(scr_boot);
        current_app = APP_BOOT;
        boot_step = 0;
        boot_timer = lv_timer_create(boot_timer_cb, 500, nullptr);
    } else {
        app_manager_show(APP_WATCHFACE);
    }
}

void app_manager_show(AppId app) {
    if (app == current_app) return;
    if (app_transitioning) return;
    app_transitioning = true;

    if (current_app == APP_WATCHFACE || current_app == APP_MENU) {
        previous_app = current_app;
    }

    if (current_app == APP_WATCHFACE && scr_watchface != nullptr) {
        watchface_destroy();
        lv_obj_delete(scr_watchface);
        scr_watchface = nullptr;
    } else if (current_app == APP_MENU && scr_menu != nullptr) {
        lv_obj_delete(scr_menu);
        scr_menu = nullptr;
    } else if (current_app == APP_BOOT && scr_boot != nullptr) {
        lv_obj_delete(scr_boot);
        scr_boot = nullptr;
    } else if (current_app != APP_WATCHFACE && current_app != APP_MENU && current_app != APP_BOOT) {
        cleanup_app();
    }

    switch (app) {
        case APP_WATCHFACE:
            scr_watchface = lv_obj_create(nullptr);
            lv_obj_set_style_bg_color(scr_watchface, PIPBOY_BG_16, 0);
            lv_obj_set_style_bg_opa(scr_watchface, LV_OPA_COVER, 0);
            lv_obj_set_scrollbar_mode(scr_watchface, LV_SCROLLBAR_MODE_OFF);
            watchface_create(scr_watchface);
            lv_obj_add_event_cb(scr_watchface, watchface_gesture_cb, LV_EVENT_GESTURE, nullptr);
            lv_screen_load(scr_watchface);
            current_app = APP_WATCHFACE;
            break;
        case APP_MENU:
            create_menu_screen();
            lv_screen_load(scr_menu);
            current_app = APP_MENU;
            break;
        default:
            launch_app(app);
            break;
    }

    app_transitioning = false;
}

AppId app_manager_current(void) { return current_app; }

void app_manager_back(void) {
    if (app_transitioning) return;
    AppId target = previous_app;
    previous_app = APP_WATCHFACE;
    app_manager_show(target);
}

void app_manager_handle_gesture(lv_dir_t dir) {
    if (current_app == APP_BOOT) return;
    if (current_app == APP_WATCHFACE && dir == LV_DIR_TOP) {
        app_manager_show(APP_MENU);
    } else if (current_app == APP_MENU && dir == LV_DIR_BOTTOM) {
        app_manager_show(APP_WATCHFACE);
    }
}

void app_manager_update(void) {
    static uint32_t last_update = 0;
    uint32_t now = millis();
    if (now - last_update < 500) return;
    last_update = now;

    if (scr_app == nullptr) return;

    switch (current_app) {
        case APP_GPS:    gps_app_update();    break;
        case APP_LORA:   lora_app_update();   break;
        case APP_NFC:    nfc_app_update();    break;
        case APP_SENSOR: sensor_app_update(); break;
        case APP_RECON:  recon_app_update();  break;
        case APP_WIFI:   wifi_app_update();   break;
        case APP_HID:    hid_app_update();    break;
        case APP_RF:     rf_app_update();     break;
        case APP_PET:    pet_app_update();    break;
        case APP_AUDIO:  audio_app_update();  break;
    }
}

void app_manager_theme_changed(void) {
    if (scr_menu) {
        lv_obj_delete(scr_menu);
        scr_menu = nullptr;
    }
    if (scr_watchface) {
        watchface_destroy();
        lv_obj_delete(scr_watchface);
        scr_watchface = nullptr;
    }

    if (current_app == APP_MENU) {
        create_menu_screen();
        lv_screen_load(scr_menu);
    } else if (current_app == APP_WATCHFACE) {
        scr_watchface = lv_obj_create(nullptr);
        lv_obj_set_style_bg_color(scr_watchface, PIPBOY_BG_16, 0);
        lv_obj_set_style_bg_opa(scr_watchface, LV_OPA_COVER, 0);
        lv_obj_set_scrollbar_mode(scr_watchface, LV_SCROLLBAR_MODE_OFF);
        watchface_create(scr_watchface);
        lv_obj_add_event_cb(scr_watchface, watchface_gesture_cb, LV_EVENT_GESTURE, nullptr);
        lv_screen_load(scr_watchface);
    }
}
