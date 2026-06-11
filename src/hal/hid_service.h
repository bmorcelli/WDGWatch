#pragma once
#include <stdint.h>
#include <stdbool.h>

enum KeyboardLayoutId {
    KB_LAYOUT_US = 0,
    KB_LAYOUT_DK,
    KB_LAYOUT_UK,
    KB_LAYOUT_FR,
    KB_LAYOUT_DE,
    KB_LAYOUT_HU,
    KB_LAYOUT_IT,
    KB_LAYOUT_BR,
    KB_LAYOUT_PT,
    KB_LAYOUT_SI,
    KB_LAYOUT_ES,
    KB_LAYOUT_SV,
    KB_LAYOUT_TR,
    KB_LAYOUT_COUNT
};

bool hid_svc_start(void);
void hid_svc_stop(void);
bool hid_svc_is_active(void);
bool hid_svc_is_connected(void);
bool hid_svc_is_usb_connected(void);
const char* hid_svc_get_name(void);

void hid_svc_set_layout(KeyboardLayoutId layout);
KeyboardLayoutId hid_svc_get_layout(void);
const char* hid_svc_get_layout_name(KeyboardLayoutId layout);

void hid_svc_run_script(const char *sd_path, bool ble);
bool hid_svc_is_running_script(void);
void hid_svc_abort_script(void);

void hid_media_vol_up(void);
void hid_media_vol_down(void);
void hid_media_screenshot(void);

bool hid_airmouse_start(void);
void hid_airmouse_stop(void);
bool hid_airmouse_is_active(void);
void hid_airmouse_calibrate(void);

void hid_mouse_click(uint8_t buttons);
void hid_mouse_scroll(int8_t wheel);

void hid_service_loop(void);
