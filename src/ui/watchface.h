#pragma once
#include <lvgl.h>
#include "../config.h"

enum WatchfaceStyle {
    WF_PIPBOY = 0,
    WF_MINIMAL,
    WF_ANALOG,
    WF_COUNT
};

void watchface_create(lv_obj_t *parent);
void watchface_update(void);
void watchface_destroy(void);

void watchface_next(void);
void watchface_prev(void);
WatchfaceStyle watchface_get_style(void);

void watchface_set_time(uint8_t hour, uint8_t min);
void watchface_set_seconds(uint8_t sec);
void watchface_set_date(uint8_t day, uint8_t month, uint8_t weekday, uint8_t week_num);
void watchface_set_battery(uint8_t percent, bool charging);
void watchface_set_steps(uint32_t steps, uint32_t goal);
void watchface_set_distance(float km);
void watchface_set_gps(float lat, float lon, float alt);
void watchface_set_temperature(int16_t temp_c);
void watchface_set_sync_status(bool wifi, bool ntp_ok, bool gps_fix);
