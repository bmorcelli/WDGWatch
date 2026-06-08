#pragma once
#include <TinyGPSPlus.h>
#include <LilyGoLib.h>

void haptic_init(void);
void haptic_click(void);
void haptic_double_click(void);
void haptic_buzz(void);
void haptic_alarm(void);
void haptic_success(void);
void haptic_error(void);
void haptic_play(uint8_t effect);
void haptic_set_enabled(bool en);
bool haptic_is_enabled(void);
