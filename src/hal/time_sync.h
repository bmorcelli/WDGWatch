#pragma once
#include <TinyGPSPlus.h>
#include <LilyGoLib.h>

struct WiFiNetwork {
    const char *ssid;
    const char *password;
    bool hidden;
};

void time_sync_init(const WiFiNetwork *networks, int count);
void time_sync_loop(void);
bool time_sync_is_synced(void);
bool time_sync_wifi_connected(void);
void time_sync_force_retry(void);
void time_sync_set_timezone(const char *tz);
String time_sync_get_timezone(void);

void time_sync_load_networks(void);
void time_sync_save_network(const char *ssid, const char *password, bool hidden = false);
int time_sync_get_saved_network_count(void);
bool time_sync_get_saved_network(int index, String &ssid, String &password, bool &hidden);

