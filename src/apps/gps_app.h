#pragma once
#include <lvgl.h>

void gps_app_create(lv_obj_t *parent);
void gps_app_update(void);
void gps_app_destroy(void);
bool gps_app_is_enabled(void);
