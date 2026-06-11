#pragma once
#include <TinyGPSPlus.h>
#include <LilyGoLib.h>
#include "../config.h"

void power_hal_init(void);
void power_hal_check_sleep(void);
void power_hal_reset_activity(void);
bool power_hal_screen_is_off(void);
void power_hal_screen_toggle(void);
void power_hal_light_sleep(void);
void power_hal_deep_sleep(void);
uint32_t power_hal_last_wakeup_time(void);

uint8_t power_hal_battery_percent(void);
bool power_hal_is_charging(void);
float power_hal_battery_voltage(void);

enum PowerProfile { PROFILE_PERFORMANCE, PROFILE_BALANCED, PROFILE_POWERSAVE };
void power_hal_set_profile(PowerProfile profile);
PowerProfile power_hal_get_profile(void);
