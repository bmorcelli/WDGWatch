#pragma once
#include <LilyGoLib.h>

struct ReconWiFi {
    char ssid[33];
    char bssid[18];
    int rssi;
    int channel;
    char auth[16];
};

struct BleDevice {
    char mac[18];
    char name[33];
    int rssi;
    bool is_airtag;
};

void recon_service_init(void);
void recon_service_loop(void);

void recon_request_wifi_scan(void);
void recon_request_ble_scan(int duration_sec = 10);
void recon_request_deauth(const char* bssid, int channel);
void recon_request_deauth_all(void);
void recon_request_sniffer(int channel = 0);
void recon_request_deauth_detect(void);
void recon_request_evil_twin(const char* ssid, int channel);
void recon_request_stop(void);

bool recon_is_scanning(void);
bool recon_is_deauthing(void);
bool recon_is_sniffing(void);
bool recon_is_evil_twin(void);

int recon_sniffer_packet_count(void);
int recon_deauth_detect_count(void);

const char* recon_et_last_cred(void);
bool recon_et_has_new_cred(void);
int  recon_wifi_count(void);
int  recon_ble_count(void);

const ReconWiFi* recon_get_wifi(int idx);
const BleDevice*   recon_get_ble(int idx);
