#include <LilyGoLib.h>
#include <WiFi.h>
#include <LV_Helper.h>
#include "config.h"
#include "app_manager.h"
#include "ui/watchface.h"
#include "ui/boot_screen.h"
#include "hal/power_hal.h"
#include "hal/time_sync.h"
#include "hal/haptic.h"
#include "web/web_server.h"
#include "hal/nfc_service.h"
#include "hal/lora_service.h"
#include "hal/recon_service.h"
#include "hal/ble_uart_service.h"
#include "api/command_handler.h"
#include "hal/rf_service.h"
#include "hal/hid_service.h"
#include "apps/gps_app.h"
#include "hal/sound_settings.h"

#include <bosch/BoschSensorDataHelper.hpp>

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
static const WiFiNetwork wifi_networks[] = {

    {"", "", false},
};
#endif

static uint32_t last_update_ms = 0;
static const uint32_t UPDATE_INTERVAL = 500;

#define BOOT_BUTTON_PIN 0
static bool boot_btn_last = true;
static SensorStepCounter *step_counter = nullptr;

static void update_watchface_data(void);

void setup() {
    Serial.begin(115200);
    Serial.println("\n=================================");
    Serial.println("  WDGWatch (PipBoy-3000) v0.1.1");
    Serial.println("  T-Watch Ultra | ESP32-S3");
    Serial.println("=================================\n");

    instance.begin();

    instance.powerControl(POWER_GPS, false);
    instance.powerControl(POWER_RADIO, false);
    instance.powerControl(POWER_NFC, false);
    WiFi.mode(WIFI_OFF);

    step_counter = new SensorStepCounter(instance.sensor);
    step_counter->enable(1.0f, 0);

    Serial.println("[INIT] Hardware OK");

    beginLvglHelper(instance);
    lv_indev_t *touch_indev = lv_get_touch_indev();
    if (touch_indev) {
        lv_indev_set_scroll_limit(touch_indev, 25);
    }
    Serial.println("[INIT] LVGL OK");

    
    lv_obj_t *root_scr = lv_scr_act();
    boot_screen_show(root_scr);
    boot_screen_update_progress("Hardware OK", 5);

    app_manager_init();
    boot_screen_update_progress("App manager OK", 15);

    power_hal_init();
    boot_screen_update_progress("Power HAL OK", 25);

    time_sync_init(wifi_networks, sizeof(wifi_networks) / sizeof(wifi_networks[0]));
    boot_screen_update_progress("Time sync OK", 35);

    instance.setBrightness(PIPBOY_DEFAULT_BRIGHTNESS);

    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    instance.onEvent([](DeviceEvent_t event, void *params, void *user_data) {
        if (event == POWER_EVENT) {
            PMUEventType_t pmu = instance.getPMUEventType(params);
            if (pmu == PMU_EVENT_KEY_CLICKED) {
                if (millis() - power_hal_last_wakeup_time() < 500) {
                    return;
                }
                power_hal_screen_toggle();
            }
        }
    });

    nfc_service_init();
    boot_screen_update_progress("NFC OK", 50);

    lora_service_init();
    boot_screen_update_progress("LoRa OK", 62);

    recon_service_init();
    boot_screen_update_progress("Recon OK", 72);

    rf_service_init();
    boot_screen_update_progress("RF OK", 82);

    api_init();
    boot_screen_update_progress("API OK", 90);

    api_set_event_callback([](const char *json) {
        ble_uart_send(json);
        web_push_log(json);
    });

    haptic_init();
    sound_settings_init();
    boot_screen_update_progress("System ready", 100);
    delay(600);

    boot_screen_hide();

    haptic_success();
    Serial.println("[INIT] Ready!\n");
}

void loop() {

    instance.loop();

    if (digitalRead(SENSOR_INT) == HIGH) {
        instance.sensor.update();
    }

    lv_timer_handler();

    uint32_t now = millis();
    if (now - last_update_ms >= UPDATE_INTERVAL) {
        last_update_ms = now;
        update_watchface_data();
    }

    time_sync_loop();

    nfc_service_loop();
    lora_service_loop();
    recon_service_loop();
    rf_service_loop();
    hid_service_loop();
    ble_uart_loop();
    api_loop();

    if (gps_app_is_enabled()) {
        instance.gps.loop();
    }

    if (watchface_alarm_is_ringing()) {
        power_hal_reset_activity();
    }

    web_server_loop();

    app_manager_update();

    int16_t tx, ty;
    if (instance.getPoint(&tx, &ty, 1) > 0) {
        if (power_hal_screen_is_off()) {
            power_hal_screen_toggle();
        }
        power_hal_reset_activity();
    }

    bool boot_btn = digitalRead(BOOT_BUTTON_PIN);
    if (!boot_btn && boot_btn_last) {
        if (power_hal_screen_is_off()) {
            power_hal_screen_toggle();
        } else if (!watchface_alarm_is_ringing()) {
            app_manager_back();
        }
        power_hal_reset_activity();
    }
    boot_btn_last = boot_btn;

    power_hal_check_sleep();

    watchface_set_sync_status(
        time_sync_wifi_connected(),
        time_sync_is_synced(),
        instance.gps.location.isValid()
    );

    delay(5);
}

static void update_watchface_data(void) {

    struct tm timeinfo;
    if (time_sync_is_synced() && getLocalTime(&timeinfo, 0)) {

        watchface_set_time(timeinfo.tm_hour, timeinfo.tm_min);
        watchface_set_seconds(timeinfo.tm_sec);

        watchface_set_date(timeinfo.tm_mday, timeinfo.tm_mon + 1,
                          timeinfo.tm_wday, 0);
    } else {

        RTC_DateTime dt = instance.rtc.getDateTime();
        watchface_set_time(dt.getHour(), dt.getMinute());
        watchface_set_seconds(dt.getSecond());
        uint8_t wd = instance.rtc.getDayOfWeek(dt.getDay(), dt.getMonth(), dt.getYear());
        watchface_set_date(dt.getDay(), dt.getMonth(), wd, 0);
    }

    uint8_t bat = power_hal_battery_percent();
    bool charging = power_hal_is_charging();
    watchface_set_battery(bat, charging);

    if (instance.gps.location.isValid()) {
        watchface_set_gps(
            instance.gps.location.lat(),
            instance.gps.location.lng(),
            instance.gps.altitude.meters()
        );
    } else {
        watchface_set_gps(NAN, NAN, 0.0f);
    }

    uint32_t steps = 0;
    if (step_counter) {
        steps = step_counter->getStepCount();
    }
    watchface_set_steps(steps, 5000);
    watchface_set_distance((steps * 0.7f) / 1000.0f);
}
