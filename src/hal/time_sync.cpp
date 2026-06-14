#include "time_sync.h"
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>
#include <Preferences.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <vector>
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

struct DynamicWiFiNetwork {
    String ssid;
    String password;
    bool hidden;
};

static std::vector<DynamicWiFiNetwork> dynamic_networks;

static void ntp_time_sync_cb(struct timeval *tv) {
    Serial.println("[TIME] NTP callback fired!");
    ntp_got_time = true;
}

void time_sync_load_networks(void) {
    dynamic_networks.clear();

    
    bool loaded_from_sd = false;
    if (SD.exists("/wifi_config.json")) {
        File f = SD.open("/wifi_config.json", FILE_READ);
        if (f) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, f);
            f.close();
            if (!err) {
                JsonArray arr = doc["networks"].as<JsonArray>();
                for (JsonObject obj : arr) {
                    DynamicWiFiNetwork net;
                    net.ssid = obj["ssid"] | "";
                    net.password = obj["password"] | "";
                    net.hidden = obj["hidden"] | false;
                    if (net.ssid.length() > 0) {
                        dynamic_networks.push_back(net);
                    }
                }
                Serial.printf("[TIME] Loaded %d networks from SD card /wifi_config.json\n", (int)dynamic_networks.size());
                loaded_from_sd = true;
            } else {
                Serial.printf("[TIME] Failed to parse /wifi_config.json: %s\n", err.c_str());
            }
        }
    }

    
    if (!loaded_from_sd || dynamic_networks.empty()) {
        Preferences prefs;
        if (prefs.begin("wificreds", true)) {
            String jsonStr = prefs.getString("networks_json", "");
            prefs.end();
            if (jsonStr.length() > 0) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, jsonStr);
                if (!err) {
                    JsonArray arr = doc["networks"].as<JsonArray>();
                    for (JsonObject obj : arr) {
                        DynamicWiFiNetwork net;
                        net.ssid = obj["ssid"] | "";
                        net.password = obj["password"] | "";
                        net.hidden = obj["hidden"] | false;
                        if (net.ssid.length() > 0) {
                            dynamic_networks.push_back(net);
                        }
                    }
                    Serial.printf("[TIME] Loaded %d networks from NVS preferences\n", (int)dynamic_networks.size());
                }
            }
        }
    }

    
    if (dynamic_networks.empty()) {
        if (networks != nullptr && network_count > 0) {
            for (int i = 0; i < network_count; i++) {
                if (strlen(networks[i].ssid) > 0) {
                    DynamicWiFiNetwork net;
                    net.ssid = networks[i].ssid;
                    net.password = networks[i].password;
                    net.hidden = networks[i].hidden;
                    dynamic_networks.push_back(net);
                }
            }
            Serial.printf("[TIME] Loaded %d networks from compile-time settings\n", (int)dynamic_networks.size());
        }
    }
}

void time_sync_save_network(const char *ssid, const char *password, bool hidden) {
    if (!ssid || strlen(ssid) == 0) return;

    
    time_sync_load_networks();

    
    bool found = false;
    for (auto &net : dynamic_networks) {
        if (net.ssid == ssid) {
            net.password = password;
            net.hidden = hidden;
            found = true;
            break;
        }
    }
    if (!found) {
        DynamicWiFiNetwork net;
        net.ssid = ssid;
        net.password = password;
        net.hidden = hidden;
        dynamic_networks.push_back(net);
    }

    
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (const auto &net : dynamic_networks) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["password"] = net.password;
        obj["hidden"] = net.hidden;
    }

    
    File f = SD.open("/wifi_config.json", FILE_WRITE);
    if (f) {
        if (serializeJson(doc, f) == 0) {
            Serial.println("[TIME] Failed to serialize JSON to SD card");
        } else {
            Serial.println("[TIME] Saved wifi_config.json to SD card");
        }
        f.close();
    } else {
        Serial.println("[TIME] SD card not available or failed to write /wifi_config.json");
    }

    
    String jsonStr;
    serializeJson(doc, jsonStr);
    Preferences prefs;
    if (prefs.begin("wificreds", false)) {
        prefs.putString("networks_json", jsonStr);
        prefs.end();
        Serial.println("[TIME] Saved wifi config to NVS preferences");
    } else {
        Serial.println("[TIME] Failed to open NVS wificreds for writing");
    }
}

int time_sync_get_saved_network_count(void) {
    return (int)dynamic_networks.size();
}

bool time_sync_get_saved_network(int index, String &ssid, String &password, bool &hidden) {
    if (index < 0 || index >= (int)dynamic_networks.size()) {
        return false;
    }
    ssid = dynamic_networks[index].ssid;
    password = dynamic_networks[index].password;
    hidden = dynamic_networks[index].hidden;
    return true;
}

static void try_next_network(void) {
    WiFi.disconnect(true);

    time_sync_load_networks();

    if (dynamic_networks.empty()) {
        Serial.println("[TIME] No Wi-Fi networks configured.");
        wifi_enabled = false;
        return;
    }

    if (current_network >= (int)dynamic_networks.size()) {
        current_network = 0;
    }

    const DynamicWiFiNetwork &net = dynamic_networks[current_network];
    Serial.printf("[TIME] Trying WiFi #%d: %s%s\n",
        current_network, net.ssid.c_str(), net.hidden ? " (hidden)" : "");

    WiFi.mode(WIFI_STA);
    if (net.hidden) {
        WiFi.begin(net.ssid.c_str(), net.password.c_str(), 0, nullptr, true);
    } else {
        WiFi.begin(net.ssid.c_str(), net.password.c_str());
    }
    wifi_enabled = true;
    wifi_start_time = millis();
}

void time_sync_set_timezone(const char *tz) {
    Preferences prefs;
    if (prefs.begin("timesync", false)) {
        prefs.putString("tz", tz);
        prefs.end();
    } else {
        Serial.println("[TIME] Warning: Failed to open NVS timesync for writing");
    }
    configTzTime(tz, "pool.ntp.org", "time.nist.gov", "time.google.com");
    Serial.printf("[TIME] Timezone updated to: %s\n", tz);
}

String time_sync_get_timezone(void) {
    Preferences prefs;
    String tz = "GST-4";
    if (prefs.begin("timesync", true)) {
        tz = prefs.getString("tz", "GST-4");
        prefs.end();
    }
    return tz;
}

void time_sync_init(const WiFiNetwork *nets, int count) {
    networks = nets;
    network_count = count;
    current_network = 0;
    
    sntp_set_time_sync_notification_cb(ntp_time_sync_cb);

    
    time_sync_load_networks();

    Preferences prefs;
    bool already_synced = true;
    if (prefs.begin("timesync", true)) {
        already_synced = prefs.getBool("synced", false); 
        prefs.end();
    }
    synced = already_synced;
    
    wifi_connected = false;
    wifi_enabled = false;
    ntp_configured = false;
    ntp_got_time = false;

    String tz = time_sync_get_timezone();
    configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");

    instance.rtc.hwClockRead();
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
        if (prefs.begin("timesync", false)) {
            prefs.putBool("synced", true);
            prefs.end();
            Serial.println("[TIME] Synced status saved to NVS.");
        } else {
            Serial.println("[TIME] Warning: Failed to save synced status to NVS.");
        }

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
        
        String connected_ssid = "";
        if (current_network >= 0 && current_network < (int)dynamic_networks.size()) {
            connected_ssid = dynamic_networks[current_network].ssid;
        } else {
            connected_ssid = WiFi.SSID();
        }

        Serial.printf("[TIME] WiFi connected: %s (IP: %s)\n",
            connected_ssid.c_str(), WiFi.localIP().toString().c_str());

        String tz = time_sync_get_timezone();
        configTzTime(tz.c_str(),
                     "pool.ntp.org", "time.nist.gov", "time.google.com");
        ntp_configured = true;
        Serial.println("[TIME] NTP request sent...");
    }

    if (!wifi_connected && (millis() - wifi_start_time > WIFI_TIMEOUT_PER_NET)) {
        Serial.printf("[TIME] WiFi #%d timeout (status=%d)\n", current_network, st);
        current_network++;
        time_sync_load_networks();
        if (!dynamic_networks.empty() && current_network < (int)dynamic_networks.size()) {
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
        time_sync_load_networks();
        if (!dynamic_networks.empty()) {
            try_next_network();
        }
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
    if (prefs.begin("timesync", false)) {
        prefs.putBool("synced", false);
        prefs.end();
        Serial.println("[TIME] Force retry — NVS flag cleared.");
    } else {
        Serial.println("[TIME] Warning: Failed to clear synced status in NVS.");
    }
    
    time_sync_load_networks();
    if (!dynamic_networks.empty()) try_next_network();
}
