#include "power_hal.h"
#include <WiFi.h>
#include "ble_uart_service.h"
#include "../web/web_server.h"
#include "../apps/gps_app.h"
#include "../app_manager.h"
#include "lora_service.h"
#include "rf_service.h"
#include "nfc_service.h"
#include "hid_service.h"
#include "../ui/watchface.h"

static uint32_t last_activity_ms = 0;
static uint32_t sleep_timeout_ms = SLEEP_TIMEOUT_MS;
static uint32_t deep_sleep_timeout_ms = DEEP_SLEEP_TIMEOUT;
static bool screen_off = false;
static uint32_t last_wakeup_ms = 0;

void power_hal_init(void) {
    last_activity_ms = millis();

    instance.onEvent([](DeviceEvent_t event, void *params, void *user_data) {
        if (event == POWER_EVENT) {
            PMUEventType_t pmu_event = instance.getPMUEventType(params);
            if (pmu_event == PMU_EVENT_KEY_CLICKED) {
                power_hal_reset_activity();
            }
        }
    });
}

void power_hal_check_sleep(void) {
    if (screen_off) return;

    if (app_manager_current() == APP_HID) {
        if (hid_svc_is_active() || hid_airmouse_is_active() || hid_svc_is_running_script()) {
            power_hal_reset_activity();
            return;
        }
    }

    if (app_manager_current() == APP_RECON) {
        power_hal_reset_activity();
        return;
    }

    uint32_t elapsed = millis() - last_activity_ms;
    if (elapsed > sleep_timeout_ms) {
        power_hal_screen_toggle();
    }
}

void power_hal_reset_activity(void) {
    last_activity_ms = millis();
}

uint32_t power_hal_last_wakeup_time(void) {
    return last_wakeup_ms;
}

void power_hal_light_sleep(void) {
    if (gps_app_is_enabled() || web_server_is_active() || ble_uart_is_active() || lora_svc_is_running() || instance.pmu.isVbusIn()) {
        screen_off = true;
        instance.setBrightness(0);
        return;
    }

    screen_off = true;
    instance.setBrightness(0);

    if (rf_jammer_is_active()) {
        rf_jammer_stop();
    }

    if (nfc_svc_is_scanning() || nfc_svc_is_emulating()) {
        nfc_svc_request_stop();
    }

    bool was_hid_active = hid_svc_is_active();
    if (was_hid_active) {
        hid_svc_stop();
    }
    bool was_ble_uart_active = ble_uart_is_active();
    if (was_ble_uart_active) {
        ble_uart_stop();
    }

    instance.powerControl(POWER_GPS, false);
    instance.powerControl(POWER_NFC, false);
    instance.powerControl(POWER_RADIO, false);
    instance.powerControl(POWER_SPEAK, false);

    WiFi.mode(WIFI_OFF);

    while (true) {
        if (watchface_alarm_is_enabled()) {
            RTC_DateTime dt = instance.rtc.getDateTime();
            uint8_t alarm_h = 12, alarm_m = 0;
            watchface_alarm_get_time(&alarm_h, &alarm_m);

            int32_t curr_secs = dt.getHour() * 3600 + dt.getMinute() * 60 + dt.getSecond();
            int32_t alarm_secs = alarm_h * 3600 + alarm_m * 60;
            int32_t diff_secs = alarm_secs - curr_secs;
            if (diff_secs <= 0) {
                diff_secs += 24 * 3600;
            }
            if (diff_secs < 2) {
                diff_secs = 2;
            }
            esp_sleep_enable_timer_wakeup((uint64_t)diff_secs * 1000000ULL);
        }

        instance.lightSleep((WakeupSource_t)(WAKEUP_SRC_POWER_KEY | WAKEUP_SRC_TOUCH_PANEL | WAKEUP_SRC_BOOT_BUTTON));

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            RTC_DateTime dt = instance.rtc.getDateTime();
            watchface_set_time(dt.getHour(), dt.getMinute());
            
            if (watchface_alarm_is_ringing()) {
                break;
            }
            continue;
        } else {
            break;
        }
    }

    last_wakeup_ms = millis();
    screen_off = false;

    if (gps_app_is_enabled()) {
        instance.powerControl(POWER_GPS, true);
        instance.gps.init(&Serial1);
    }

    AppId current = app_manager_current();
    if (current == APP_NFC) {
        instance.powerControl(POWER_NFC, true);
    }
    if (current == APP_RF || current == APP_LORA || lora_svc_is_running()) {
        instance.powerControl(POWER_RADIO, true);
    }

    if (was_hid_active) {
        hid_svc_start();
    }
    if (was_ble_uart_active) {
        ble_uart_init();
    }

    instance.setBrightness(PIPBOY_DEFAULT_BRIGHTNESS);
    power_hal_reset_activity();
}

void power_hal_deep_sleep(void) {
    instance.setBrightness(0);
    instance.powerControl(POWER_GPS, false);
    instance.powerControl(POWER_NFC, false);
    instance.powerControl(POWER_RADIO, false);
    instance.powerControl(POWER_SPEAK, false);

    instance.sleep((WakeupSource_t)(WAKEUP_SRC_POWER_KEY | WAKEUP_SRC_BOOT_BUTTON));

    power_hal_reset_activity();
}

static uint8_t cached_bat_pct = 0;
static bool cached_charging = false;
static float cached_bat_volt = 0;
static uint32_t last_pmu_read = 0;

static void refresh_pmu_cache(void) {
    uint32_t now = millis();
    if (now - last_pmu_read < 2000) return;
    last_pmu_read = now;
    int pct = instance.pmu.getBatteryPercent();
    cached_bat_pct = (pct < 0) ? 0 : (pct > 100) ? 100 : (uint8_t)pct;
    cached_charging = instance.pmu.isCharging();
    cached_bat_volt = instance.pmu.getBattVoltage() / 1000.0f;
}

uint8_t power_hal_battery_percent(void) {
    refresh_pmu_cache();
    return cached_bat_pct;
}

bool power_hal_is_charging(void) {
    refresh_pmu_cache();
    return cached_charging;
}

float power_hal_battery_voltage(void) {
    refresh_pmu_cache();
    return cached_bat_volt;
}

static PowerProfile current_profile = PROFILE_BALANCED;

void power_hal_set_profile(PowerProfile profile) {
    current_profile = profile;
    switch (profile) {
        case PROFILE_PERFORMANCE:
            sleep_timeout_ms = 30000;
            setCpuFrequencyMhz(240);
            break;
        case PROFILE_BALANCED:
            sleep_timeout_ms = 15000;
            setCpuFrequencyMhz(240);
            break;
        case PROFILE_POWERSAVE:
            sleep_timeout_ms = 8000;
            setCpuFrequencyMhz(160);
            instance.powerControl(POWER_GPS, false);
            break;
    }
}

PowerProfile power_hal_get_profile(void) {
    return current_profile;
}

bool power_hal_screen_is_off(void) {
    return screen_off;
}

void power_hal_screen_toggle(void) {
    if (screen_off) {
        screen_off = false;
        instance.setBrightness(PIPBOY_DEFAULT_BRIGHTNESS);
        power_hal_reset_activity();
    } else {
        power_hal_light_sleep();
    }
}
