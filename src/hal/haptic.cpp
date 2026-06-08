#include "haptic.h"

static bool enabled = true;

void haptic_init(void) {
    enabled = true;
}

static void play_effect(uint8_t e1, uint8_t e2 = 0, uint8_t e3 = 0) {
    if (!enabled) return;
    instance.drv.setWaveform(0, e1);
    instance.drv.setWaveform(1, e2);
    instance.drv.setWaveform(2, e3);
    instance.drv.setWaveform(e2 ? (e3 ? 3 : 2) : 1, 0);
    instance.drv.run();
}

void haptic_click(void)        { play_effect(17); }
void haptic_double_click(void) { play_effect(10); }
void haptic_buzz(void)         { play_effect(15, 15); }
void haptic_alarm(void)        { play_effect(16, 16, 16); }
void haptic_success(void)      { play_effect(14); }
void haptic_error(void)        { play_effect(54); }

void haptic_play(uint8_t effect) { play_effect(effect); }

void haptic_set_enabled(bool en) { enabled = en; }
bool haptic_is_enabled(void)     { return enabled; }
