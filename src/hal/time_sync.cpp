#include "time_sync.h"
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>
#include <Preferences.h>
#include "../config.h"
#include "../web/web_server.h"

static void wifi_shutdown_safe(void) {

    if (web_server_is_active()) {
        WiFi.disconnect(false);
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
}

static bool synced = false;
static bool wifi_connected = false;
static bool wifi_enabled = false;
static bool ntp_configured = false;
static volatile bool ntp_got_time = false;

static const WiFiNetwork *networks = nullptr;
static int network_count = 0;
static int current_network = 0;
static uint32_t wifi_start_time = 0;
static const uint32_t WIFI_TIMEOUT_PER_NET = 15000;

static void ntp_time_sync_cb(struct timeval *tv) {
    Serial.println("[TIME] NTP callback fired!");
    ntp_got_time = true;
}

static void try_next_network(void) {
    WiFi.disconnect(true);

    if (current_network >= network_count) {

        current_network = 0;
    }

    const WiFiNetwork &net = networks[current_network];
    Serial.printf("[TIME] Trying WiFi #%d: %s%s\n",
        current_network, net.ssid, net.hidden ? " (hidden)" : "");

    WiFi.mode(WIFI_STA);
    if (net.hidden) {

        WiFi.begin(net.ssid, net.password, 0, nullptr, true);
    } else {
        WiFi.begin(net.ssid, net.password);
    }
    wifi_enabled = true;
    wifi_start_time = millis();
}

void time_sync_init(const WiFiNetwork *nets, int count) {
    networks = nets;
    network_count = count;
    current_network = 0;
    synced = false;
    wifi_connected = false;
    wifi_enabled = false;
    ntp_configured = false;
    ntp_got_time = false;

    sntp_set_time_sync_notification_cb(ntp_time_sync_cb);

    Preferences prefs;
    prefs.begin("timesync", true);
    bool previously_synced = prefs.getBool("synced", false);
    prefs.end();

    if (previously_synced) {

        synced = true;
        Serial.println("[TIME] RTC valid from NVS — skipping WiFi, no IMU conflict.");
        return;
    }

    wifi_start_time = millis() + 3000;
    if (count > 0) {

        delay(500);
        try_next_network();
    }
}

void time_sync_loop(void) {
    if (synced) return;

    if (ntp_got_time) {
        ntp_got_time = false;
        instance.rtc.hwClockWrite();

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0)) {
            Serial.printf("[TIME] SYNCED: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }

        Preferences prefs;
        prefs.begin("timesync", false);
        prefs.putBool("synced", true);
        prefs.end();

        synced = true;
        wifi_shutdown_safe();
        wifi_enabled = false;
        wifi_connected = false;
        Serial.println("[TIME] Done, WiFi off — NVS flag saved.");
        return;
    }

    if (!wifi_enabled) return;

    wl_status_t st = WiFi.status();

    if (st == WL_CONNECTED && !wifi_connected) {
        wifi_connected = true;
        Serial.printf("[TIME] WiFi connected: %s (IP: %s)\n",
            networks[current_network].ssid, WiFi.localIP().toString().c_str());

        configTzTime("GST-4",
                     "pool.ntp.org", "time.nist.gov", "time.google.com");
        ntp_configured = true;
        Serial.println("[TIME] NTP request sent...");
    }

    if (!wifi_connected && (millis() - wifi_start_time > WIFI_TIMEOUT_PER_NET)) {
        Serial.printf("[TIME] WiFi #%d timeout (status=%d)\n", current_network, st);
        current_network++;
        if (current_network < network_count) {
            try_next_network();
        } else {

            Serial.println("[TIME] All networks failed, retrying in 10s...");
            wifi_shutdown_safe();
            wifi_enabled = false;
            current_network = 0;
            wifi_start_time = millis();
        }
    }

    if (wifi_connected && ntp_configured && (millis() - wifi_start_time > 45000)) {
        Serial.println("[TIME] NTP timeout");
        wifi_shutdown_safe();
        wifi_enabled = false;
        wifi_connected = false;
        ntp_configured = false;

    }

    if (!wifi_enabled && !synced && (millis() - wifi_start_time > 10000)) {
        try_next_network();
    }
}

bool time_sync_is_synced(void) { return synced; }
bool time_sync_wifi_connected(void) { return wifi_connected; }

void time_sync_force_retry(void) {
    wifi_shutdown_safe();
    wifi_enabled = false;
    wifi_connected = false;
    synced = false;
    ntp_configured = false;
    ntp_got_time = false;
    current_network = 0;

    Preferences prefs;
    prefs.begin("timesync", false);
    prefs.putBool("synced", false);
    prefs.end();
    Serial.println("[TIME] Force retry — NVS flag cleared.");
    if (network_count > 0) try_next_network();
}
