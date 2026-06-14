#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void boot_screen_show(lv_obj_t *parent);
void boot_screen_update_progress(const char *msg, uint8_t pct);
void boot_screen_hide(void);

#ifdef __cplusplus
}
#endif
