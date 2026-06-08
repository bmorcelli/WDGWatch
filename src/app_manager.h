#pragma once
#include <lvgl.h>
#include "config.h"

void app_manager_init(void);
void app_manager_show(AppId app);
AppId app_manager_current(void);
void app_manager_back(void);

void app_manager_handle_gesture(lv_dir_t dir);

void app_manager_update(void);
