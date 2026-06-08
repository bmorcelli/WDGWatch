#pragma once
#include <lvgl.h>
#include <stdint.h>

void pet_app_create(lv_obj_t *parent);
void pet_app_update(void);
void pet_app_destroy(void);

void pet_feed_action(void);
void pet_heal_action(void);
void pet_clean_action(void);
void pet_get_stats(uint32_t *level, uint32_t *xp, uint32_t *energy, uint32_t *health, uint32_t *clean, uint32_t *poops);
