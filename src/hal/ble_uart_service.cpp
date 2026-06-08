#include "ble_uart_service.h"
#include <NimBLEDevice.h>
#include <LilyGoLib.h>
#include <cstdio>
#include <cstring>
#include "../config.h"
#include "../api/command_handler.h"
#include "../ui/action_overlay.h"
#include "haptic.h"

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLEServer *pServer = nullptr;
static NimBLECharacteristic *pTxChar = nullptr;
static bool ble_connected = false;
static bool ble_initialized = false;

static char pin_code[7] = "000000";
static uint32_t pin_numeric = 0;

static char rx_buf[512] = "";
static int rx_pos = 0;
static volatile bool cmd_ready = false;
static char cmd_buf[512] = "";
static uint32_t last_rx_time = 0;
static volatile bool show_pin_flag = false;
static volatile bool hide_pin_flag = false;
static volatile bool ble_just_connected = false;
static volatile bool ble_just_disconnected = false;

static void generate_pin(void) {
    pin_numeric = esp_random() % 1000000;
    snprintf(pin_code, sizeof(pin_code), "%06lu", (unsigned long)pin_numeric);
    Serial.printf("[BLE] PIN: %s\n", pin_code);
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s, NimBLEConnInfo &connInfo) override {
        ble_connected = true;
        ble_just_connected = true;
        haptic_success();
        Serial.printf("[BLE] Connected: %s\n", connInfo.getAddress().toString().c_str());
    }

    void onDisconnect(NimBLEServer *s, NimBLEConnInfo &connInfo, int reason) override {
        ble_connected = false;
        ble_just_disconnected = true;
        Serial.printf("[BLE] Disconnected (reason=%d)\n", reason);
        NimBLEDevice::startAdvertising();
    }

    uint32_t onPassKeyDisplay() override {
        Serial.printf("[BLE] PassKey display: %lu\n", (unsigned long)pin_numeric);
        show_pin_flag = true;
        return pin_numeric;
    }

    void onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t pass_key) override {

        NimBLEDevice::injectConfirmPasskey(connInfo, pass_key == pin_numeric);
    }

    void onAuthenticationComplete(NimBLEConnInfo &connInfo) override {
        hide_pin_flag = true;
        if (connInfo.isEncrypted()) {
            Serial.println("[BLE] Auth OK - encrypted");
        } else {
            Serial.println("[BLE] Auth failed - disconnecting");
            pServer->disconnect(connInfo.getConnHandle());
        }
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &connInfo) override {
        NimBLEAttValue val = pChar->getValue();
        const uint8_t *data = val.data();
        size_t len = val.size();

        last_rx_time = millis();
        Serial.printf("[BLE] RX %d bytes: ", len);
        for (size_t i = 0; i < len && i < 40; i++) Serial.printf("%c", data[i] >= 32 ? data[i] : '.');
        Serial.println();

        for (size_t i = 0; i < len && rx_pos < 510; i++) {
            char c = (char)data[i];
            if (c == '\n' || c == '\r') {
                if (rx_pos > 0) {
                    rx_buf[rx_pos] = 0;
                    memcpy(cmd_buf, rx_buf, rx_pos + 1);
                    cmd_ready = true;
                    rx_pos = 0;
                }
            } else {
                rx_buf[rx_pos++] = c;
            }
        }
    }
};

void ble_uart_init(void) {
    if (ble_initialized) return;

    generate_pin();

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    const char charset[] = "0123456789abcdefghjkmnpqrstuvwxyz";
    uint32_t seed = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
    char suffix[6];
    for (int i = 0; i < 5; i++) { suffix[i] = charset[seed % 32]; seed /= 32; }
    suffix[5] = 0;
    char ble_name[32];
    snprintf(ble_name, sizeof(ble_name), "SCR Terminal-%s", suffix);

    if (NimBLEDevice::isInitialized()) {
        NimBLEDevice::deinit(true);
        delay(50);
    }
    NimBLEDevice::init(ble_name);
    NimBLEDevice::setSecurityAuth(true, false, false);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService *pService = pServer->createService(NUS_SERVICE_UUID);

    pTxChar = pService->createCharacteristic(
        NUS_TX_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );

    NimBLECharacteristic *pRxChar = pService->createCharacteristic(
        NUS_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxChar->setCallbacks(new RxCallbacks());

    pServer->start();

    NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName(ble_name);
    pAdv->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setCompleteServices(NimBLEUUID(NUS_SERVICE_UUID));
    pAdv->setScanResponseData(scanData);

    pAdv->start();
    Serial.printf("[BLE] Advertising: %s\n", ble_name);

    ble_initialized = true;
    Serial.printf("[BLE] Started: %s PIN: %s\n", ble_name, pin_code);
}

void ble_uart_stop(void) {
    if (!ble_initialized) return;

    if (ble_connected && pServer) {
        pServer->disconnect(0);
        ble_connected = false;
    }

    NimBLEDevice::stopAdvertising();
    NimBLEDevice::deinit(true);

    pServer = nullptr;
    pTxChar = nullptr;
    ble_initialized = false;
    ble_connected = false;
    Serial.println("[BLE] Stopped");
}

bool ble_uart_is_active(void) {
    return ble_initialized;
}

void ble_uart_loop(void) {
    if (!ble_initialized) return;

    if (ble_just_connected) {
        ble_just_connected = false;
        action_overlay_show("WATCH_DOGS");
        instance.setBrightness(15);
    }
    if (ble_just_disconnected) {
        ble_just_disconnected = false;
        action_overlay_hide();
        instance.setBrightness(PIPBOY_DEFAULT_BRIGHTNESS);
    }

    if (show_pin_flag) {
        show_pin_flag = false;
        action_overlay_show("PAIRING");
        action_overlay_set_status(pin_code);
    }
    if (hide_pin_flag) {
        hide_pin_flag = false;
        action_overlay_hide();
    }

    if (ble_connected && action_overlay_is_active()) {
        static uint32_t last_clock = 0;
        if (millis() - last_clock > 1000) {
            last_clock = millis();
            struct tm ti;
            if (getLocalTime(&ti, 0)) {
                char t[8]; snprintf(t, sizeof(t), "%02d:%02d", ti.tm_hour, ti.tm_min);
                action_overlay_set_time(t);
            }
        }
    }

    if (ble_connected && last_rx_time > 0 && (millis() - last_rx_time > 60000)) {
        Serial.println("[BLE] Heartbeat timeout - forcing disconnect");
        if (pServer) pServer->disconnect(0);
        last_rx_time = 0;
    }

    if (cmd_ready) {
        cmd_ready = false;
        Serial.printf("[BLE] CMD: %s\n", cmd_buf);

        char *response = api_handle_command(cmd_buf);
        if (response) {
            Serial.printf("[BLE] RSP: %.60s\n", response);
            ble_uart_send(response);
            free(response);

            if (strstr(cmd_buf, "\"reboot\"")) {
                delay(200);
                ESP.restart();
            }
        }
    }
}

bool ble_uart_is_connected(void) {
    return ble_connected;
}

void ble_uart_send(const char *data) {
    if (!ble_connected || !pTxChar) return;

    int len = strlen(data);

    for (int i = 0; i < len; i += 20) {
        int chunk = (len - i > 20) ? 20 : len - i;
        pTxChar->notify((const uint8_t*)(data + i), chunk);
        if (i + 20 < len) delay(5);
    }

    pTxChar->notify((const uint8_t*)"\n", 1);
}

const char* ble_uart_get_pin(void) {
    return pin_code;
}
