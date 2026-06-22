#pragma once

#define SCREEN_WIDTH        410
#define SCREEN_HEIGHT       502
#define PIPBOY_MAX_BRIGHTNESS      255
#define PIPBOY_DEFAULT_BRIGHTNESS  128

extern uint32_t current_primary_hex;
extern uint32_t current_dim_hex;
extern uint32_t current_dark_hex;

#define PIPBOY_GREEN        current_primary_hex
#define PIPBOY_GREEN_DIM    current_dim_hex
#define PIPBOY_GREEN_DARK   current_dark_hex
#define PIPBOY_BG           0x000000

#define PIPBOY_GREEN_16     lv_color_hex(current_primary_hex)
#define PIPBOY_GREEN_DIM_16 lv_color_hex(current_dim_hex)
#define PIPBOY_DARK_16      lv_color_hex(current_dark_hex)
#define PIPBOY_BG_16        lv_color_hex(0x000000)

#define SAFE_TOP    35
#define SAFE_BOTTOM 30
#define SAFE_LEFT   25
#define SAFE_RIGHT  25

#define SLEEP_TIMEOUT_MS    10000
#define DEEP_SLEEP_TIMEOUT  30000

#define GPS_BAUD            38400

#define LORA_FREQ           868.0
#define LORA_BW             125.0
#define LORA_SF             9
#define LORA_CR             7
#define LORA_POWER          14

#define VAULTBOY_FRAMES     8
#define VAULTBOY_FPS        6
#define VAULTBOY_W          110
#define VAULTBOY_H          140

enum AppId {
    APP_WATCHFACE = 0,
    APP_MENU,
    APP_GPS,
    APP_LORA,
    APP_NFC,
    APP_SENSOR,
    APP_AUDIO,
    APP_RECON,
    APP_WIFI,
    APP_SETTINGS,
    APP_TOOLS,
    APP_HID,
    APP_RF,
    APP_PET,
    APP_BOOT,
    APP_COUNT
};
