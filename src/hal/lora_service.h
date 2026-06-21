#pragma once
#include <LilyGoLib.h>

struct MeshMsg {
    char channel[32];
    char text[200];
    float rssi;
    int hops;
    uint32_t timestamp;
};

struct MeshNode {
    char name[32];
    char type[16];
    float lat, lon;
    float rssi, snr;
    uint8_t pubkey[32];
};

enum LoraMode {
    MODE_MESHCORE = 0,
    MODE_MESHTASTIC = 1,
    MODE_POCSAG = 2,
    MODE_BRUCE = 3
};

void lora_svc_set_ric(uint32_t ric);
uint32_t lora_svc_get_ric(void);
void lora_svc_set_freq(float freq_mhz);
float lora_svc_get_freq(void);

void lora_service_init(void);
void lora_service_loop(void);

void lora_svc_start(LoraMode mode);
void lora_svc_stop(void);
void lora_svc_send_message(const char *text);
void lora_svc_send_advert(void);
void lora_svc_set_node_name(const char *name);
const char* lora_svc_get_node_name(void);

bool lora_svc_is_running(void);
LoraMode lora_svc_get_mode(void);
bool lora_svc_has_new_message(void);
bool lora_svc_has_new_message_web(void);
bool lora_svc_has_new_message_ble(void);
const MeshMsg* lora_svc_last_message(void);
const MeshMsg* lora_svc_get_message(int idx);
int lora_svc_message_count(void);
int lora_svc_node_count(void);
void lora_svc_save_history(void);
void lora_svc_load_history(void);
