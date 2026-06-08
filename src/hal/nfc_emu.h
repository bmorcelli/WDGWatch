#pragma once
#include <LilyGoLib.h>

#if defined(USING_ST25R3916)
#include "nfc_hal.h"

bool nfc_emu_start(const uint8_t *uid, uint8_t uid_len, const char *ndef_text);

bool nfc_emu_loop(void);

void nfc_emu_stop(void);

bool nfc_emu_is_active(void);

#endif
