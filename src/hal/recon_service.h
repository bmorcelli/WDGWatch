#pragma once
#include <LilyGoLib.h>

#define BEACON_SSID_COUNT 15
#define BEACON_SSID_LEN   33
#define MAX_ARP_RESULTS   32
#define MAX_SNIFF_IPS     32

struct ReconWiFi {
    char ssid[33];
    char bssid[18];
    int  rssi;
    int  channel;
    char auth[16];
    bool is_camera;
};

struct ArpDevice {
    char ip[16];
    char mac[18];
    char vendor[32];
};

struct BleDevice {
    char mac[18];
    char name[33];
    int  rssi;
    bool is_airtag;
    bool is_flipper;
};

void recon_service_init(void);
void recon_service_loop(void);

void recon_request_wifi_scan(void);
void recon_request_ble_scan(int duration_sec = 10);
void recon_request_deauth(const char* bssid, int channel);
void recon_request_deauth_all(void);
void recon_request_sniffer(int channel = 0);
void recon_request_deauth_detect(void);
void recon_request_stop(void);

void recon_request_beacon_spam(const char ssids[][BEACON_SSID_LEN], int count);

void recon_request_evil_twin(const char* ssid, int channel);

void recon_request_evil_twin_full(const char* ssid, int channel,
                                   const char* bssid,
                                   const char* html_path);

bool recon_is_scanning(void);
bool recon_is_deauthing(void);
bool recon_is_sniffing(void);
bool recon_is_evil_twin(void);
bool recon_is_beacon_spamming(void);

int recon_sniffer_packet_count(void);
int recon_deauth_detect_count(void);
int recon_beacon_active_count(void);

const char* recon_et_last_cred(void);
bool        recon_et_has_new_cred(void);

int recon_wifi_count(void);
int recon_ble_count(void);
int recon_arp_count(void);

const ReconWiFi* recon_get_wifi(int idx);
const BleDevice* recon_get_ble(int idx);
const ArpDevice* recon_get_arp_device(int idx);

void recon_request_arp_scan(void);
bool recon_is_arp_scanning(void);
bool recon_is_arp_waiting_wifi(void);
int  recon_arp_scan_progress(void);

void recon_request_ip_sniff(const char* target_ip);
bool recon_is_ip_sniffing(void);
const char* recon_sniff_target_ip(void);
int  recon_sniff_unique_ip_count(void);
const char* recon_sniff_get_ip(int idx);

void recon_request_bitgotchi(bool active);
bool recon_is_bitgotchi_active(void);
int  recon_bitgotchi_friends_count(void);
int  recon_bitgotchi_handshakes_count(void);
const char* recon_bitgotchi_last_event(void);
bool recon_bitgotchi_has_new_event(void);
