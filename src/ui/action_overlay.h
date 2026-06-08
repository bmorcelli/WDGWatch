#pragma once
#include <lvgl.h>

void action_overlay_show(const char *action_name);

void action_overlay_set_status(const char *status);

void action_overlay_hide(void);

void action_overlay_set_time(const char *time_str);

bool action_overlay_is_active(void);
