#pragma once
#include <lvgl.h>

void gps_app_create(lv_obj_t *parent);
void gps_app_update(void);
void gps_app_destroy(void);
bool gps_app_is_enabled(void);
void gps_app_set_enabled(bool enabled);
bool gps_app_is_wardriving_active(void);
