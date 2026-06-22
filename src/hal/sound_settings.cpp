#include "sound_settings.h"
#include <Preferences.h>

static uint8_t master_volume = 80;

void sound_settings_init(void) {
    Preferences prefs;
    prefs.begin("sound", true);
    master_volume = prefs.getUChar("volume", 80);
    prefs.end();
}

uint8_t sound_get_volume(void) {
    return master_volume;
}

void sound_set_volume(uint8_t vol) {
    if (vol > 100) vol = 100;
    master_volume = vol;
    Preferences prefs;
    prefs.begin("sound", false);
    prefs.putUChar("volume", vol);
    prefs.end();
}
