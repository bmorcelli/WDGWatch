#include "hid_service.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <LilyGoLib.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <bosch/bhy2_parse.h>
#include "../config.h"
#include "haptic.h"
#include "ble_uart_service.h"

#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <USBHIDConsumerControl.h>
#include "tusb.h"
#include <hal/usb_serial_jtag_ll.h>

#include "keys.h"
#include "KeyboardLayout.h"

#define TAG "[HID]"

#define HID_RPT_KEYBOARD  1
#define HID_RPT_MEDIA     2
#define HID_RPT_MOUSE     3

static NimBLEHIDDevice   *hid_device   = nullptr;
static NimBLECharacteristic *kb_input  = nullptr;
static NimBLECharacteristic *kb_output = nullptr;
static NimBLECharacteristic *media_input = nullptr;
static NimBLECharacteristic *mouse_input = nullptr;
static NimBLEServer        *hid_server = nullptr;
static bool hid_active      = false;
static bool hid_connected   = false;
static bool script_running  = false;
static bool script_abort    = false;
static bool airmouse_active = false;

static USBHIDKeyboard        usb_kb;
static USBHIDMouse           usb_mouse;
static USBHIDConsumerControl usb_media;
static bool usb_started     = false;

static void start_usb_hid(void) {
    if (!usb_started) {
        usb_kb.begin();
        usb_mouse.begin();
        usb_media.begin();
        USB.begin();
        usb_started = true;
        Serial.println(TAG " Native USB HID initialized");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

static void stop_usb_hid(void) {
    if (usb_started) {
        usb_kb.end();
        usb_mouse.end();
        usb_media.end();
        usb_started = false;

        usb_serial_jtag_ll_enable_pad(true);
        vTaskDelay(pdMS_TO_TICKS(200));
        Serial.println(TAG " USB Serial/JTAG restored");
    }
}

static KeyboardLayoutId current_layout = KB_LAYOUT_TR;

static const uint8_t* LAYOUT_TABLES[KB_LAYOUT_COUNT] = {
    KeyboardLayout_en_US,
    KeyboardLayout_da_DK,
    KeyboardLayout_en_UK,
    KeyboardLayout_fr_FR,
    KeyboardLayout_de_DE,
    KeyboardLayout_hu_HU,
    KeyboardLayout_it_IT,
    KeyboardLayout_pt_BR,
    KeyboardLayout_pt_PT,
    KeyboardLayout_si_SI,
    KeyboardLayout_es_ES,
    KeyboardLayout_sv_SE,
    KeyboardLayout_tr_TR
};

static const char* LAYOUT_NAMES[KB_LAYOUT_COUNT] = {
    "US", "DK", "UK", "FR", "DE", "HU", "IT", "BR", "PT", "SI", "ES", "SV", "TR"
};

static float imu_pitch = 0.0f, imu_roll = 0.0f;
static const float SENSITIVITY = 0.87f;
static const float DEADZONE    = 2.0f;
static const float FILTER_K    = 0.3f;
static float last_x = 0.0f, last_y = 0.0f;
static bool imu_running = false;

static const uint8_t HID_DESC[] = {

    0x05, 0x01,
    0x09, 0x06,
    0xA1, 0x01,
    0x85, HID_RPT_KEYBOARD,
    0x05, 0x07,
    0x19, 0xE0,
    0x29, 0xE7,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x03,
    0x95, 0x06,
    0x75, 0x08,
    0x15, 0x00,
    0x25, 0x73,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0x73,
    0x81, 0x00,
    0xC0,

    0x05, 0x0C,
    0x09, 0x01,
    0xA1, 0x01,
    0x85, HID_RPT_MEDIA,
    0x09, 0xE9,
    0x09, 0xEA,
    0x09, 0x65,
    0x09, 0xE2,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x04,
    0x81, 0x02,
    0x95, 0x04,
    0x81, 0x03,
    0xC0,

    0x05, 0x01,
    0x09, 0x02,
    0xA1, 0x01,
    0x85, HID_RPT_MOUSE,
    0x09, 0x01,
    0xA1, 0x00,

    0x05, 0x09,
    0x19, 0x01,
    0x29, 0x03,
    0x15, 0x00,
    0x25, 0x01,
    0x95, 0x03,
    0x75, 0x01,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x05,
    0x81, 0x03,

    0x05, 0x01,
    0x09, 0x30,
    0x09, 0x31,
    0x15, 0x81,
    0x25, 0x7F,
    0x75, 0x08,
    0x95, 0x02,
    0x81, 0x06,
    0xC0, 0xC0
};

class HIDServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s, NimBLEConnInfo &info) override {
        hid_connected = true;
        haptic_success();
        Serial.printf(TAG " Connected: %s\n", info.getAddress().toString().c_str());
    }
    void onDisconnect(NimBLEServer *s, NimBLEConnInfo &info, int reason) override {
        hid_connected = false;
        Serial.printf(TAG " Disconnected (reason=%d)\n", reason);
        if (hid_active) NimBLEDevice::startAdvertising();
    }
};

static void airmouse_imu_cb(uint8_t sensor_id, uint8_t *data, uint32_t len,
                             uint64_t *ts, void *user) {
    float roll, pitch, yaw;
    bhy2_quaternion_to_euler(data, &roll, &pitch, &yaw);
    imu_pitch = pitch;
    imu_roll  = roll;
}

static void send_keyboard(uint8_t modifier, uint8_t keycode) {

    if (hid_svc_is_usb_connected()) {
        KeyReport report;
        report.modifiers = modifier;
        report.reserved = 0;
        report.keys[0] = keycode;
        for (int i = 1; i < 6; i++) report.keys[i] = 0;
        usb_kb.sendReport(&report);
        delay(10);

        report.modifiers = 0;
        report.keys[0] = 0;
        usb_kb.sendReport(&report);
    } else if (hid_connected && kb_input) {
        uint8_t report[8] = { modifier, 0, keycode, 0, 0, 0, 0, 0 };
        kb_input->notify(report, sizeof(report));
        delay(10);

        memset(report, 0, sizeof(report));
        kb_input->notify(report, sizeof(report));
    }
}

static void send_media(uint8_t mask) {
    if (hid_svc_is_usb_connected()) {

        uint16_t code = 0;
        if (mask == 0x01) code = CONSUMER_CONTROL_VOLUME_INCREMENT;
        else if (mask == 0x02) code = CONSUMER_CONTROL_VOLUME_DECREMENT;
        else if (mask == 0x04) code = CONSUMER_CONTROL_PLAY_PAUSE;

        if (code) {
            usb_media.press(code);
            delay(30);
            usb_media.release();
        }
    } else if (hid_connected && media_input) {
        uint8_t report[1] = { mask };
        media_input->notify(report, sizeof(report));
        delay(30);
        report[0] = 0;
        media_input->notify(report, sizeof(report));
    }
}

static void send_mouse(int8_t dx, int8_t dy, uint8_t buttons = 0) {
    if (hid_svc_is_usb_connected()) {
        usb_mouse.move(dx, dy);
        if (buttons) {
            if (buttons & 0x01) usb_mouse.press(MOUSE_LEFT);
            if (buttons & 0x02) usb_mouse.press(MOUSE_RIGHT);
            delay(10);
            usb_mouse.release(MOUSE_LEFT | MOUSE_RIGHT);
        }
    } else if (hid_connected && mouse_input) {
        uint8_t report[4] = { buttons, (uint8_t)dx, (uint8_t)dy, 0 };
        mouse_input->notify(report, sizeof(report));
    }
}

static void airmouse_task(void *arg) {
    instance.sensor.configure(SensorBHI260AP::GAME_ROTATION_VECTOR, 100.0f, 0);
    instance.sensor.onResultEvent(SensorBHI260AP::GAME_ROTATION_VECTOR, airmouse_imu_cb);
    imu_running = true;

    Serial.println(TAG " Air Mouse task started");

    while (airmouse_active) {
        last_x = last_x + FILTER_K * (imu_roll  - last_x);
        last_y = last_y + FILTER_K * (imu_pitch - last_y);

        int8_t moveX = 0, moveY = 0;
        if (fabsf(last_x) > DEADZONE) {
            float v = last_x > 0 ? last_x - DEADZONE : last_x + DEADZONE;
            moveX = (int8_t)constrain((int)(v * SENSITIVITY), -20, 20);
        }
        if (fabsf(last_y) > DEADZONE) {
            float v = last_y > 0 ? last_y - DEADZONE : last_y + DEADZONE;
            moveY = (int8_t)constrain((int)(v * SENSITIVITY), -20, 20);
        }

        if ((hid_connected || hid_svc_is_usb_connected()) && (moveX != 0 || moveY != 0)) {
            send_mouse(moveX, moveY);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    instance.sensor.configure(SensorBHI260AP::GAME_ROTATION_VECTOR, 0, 0);
    imu_running = false;
    Serial.println(TAG " Air Mouse task stopped");
    vTaskDelete(nullptr);
}

struct ScriptTaskArgs {
    char path[128];
    bool is_ble;
};
static ScriptTaskArgs script_args;

static void script_task(void *arg) {
    ScriptTaskArgs *args = (ScriptTaskArgs *)arg;
    Serial.printf(TAG " Running script: %s (BLE: %s)\n", args->path, args->is_ble ? "true" : "false");

    if (!SD.exists(args->path)) {
        Serial.println(TAG " Script file not found on SD");
        script_running = false;
        vTaskDelete(nullptr);
        return;
    }

    File f = SD.open(args->path, FILE_READ);
    if (!f) {
        script_running = false;
        vTaskDelete(nullptr);
        return;
    }

    if (!args->is_ble) {
        start_usb_hid();
    }

    uint32_t t = millis();
    while (!hid_connected && !hid_svc_is_usb_connected() && (millis() - t < 30000) && !script_abort) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if ((!hid_connected && !hid_svc_is_usb_connected()) || script_abort) {
        f.close();
        script_running = false;
        script_abort = false;
        if (!args->is_ble) {
            stop_usb_hid();
        }
        vTaskDelete(nullptr);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    while (f.available() && !script_abort) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("REM") || line.startsWith("//")) continue;

        if (line.startsWith("DELAY ")) {
            delay(line.substring(6).toInt());
        } else if (line.startsWith("STRING ")) {
            String txt = line.substring(7);
            for (int i = 0; i < (int)txt.length(); i++) {
                char c = txt[i];
                if ((uint8_t)c < 128) {
                    uint8_t key_val = pgm_read_byte(&(LAYOUT_TABLES[current_layout][(uint8_t)c]));
                    if (key_val != 0) {
                        uint8_t keycode = key_val & 0x3F;
                        uint8_t mod = 0;
                        if (key_val & 0x80) mod |= 0x02;
                        if (key_val & 0x40) mod |= 0x40;
                        send_keyboard(mod, keycode);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else if (line == "ENTER") {
            send_keyboard(0, 0x28);
        } else if (line == "TAB") {
            send_keyboard(0, 0x2B);
        } else if (line == "ESCAPE" || line == "ESC") {
            send_keyboard(0, 0x29);
        } else if (line == "BACKSPACE") {
            send_keyboard(0, 0x2A);
        } else if (line.startsWith("GUI ") || line.startsWith("WINDOWS ")) {
            int sp = line.indexOf(' ');
            char k = line.charAt(sp + 1);
            uint8_t kc = 0;
            if (k >= 'a' && k <= 'z') kc = 4 + (k - 'a');
            else if (k >= 'A' && k <= 'Z') kc = 4 + (k - 'A');
            send_keyboard(0x08, kc);
        } else if (line.startsWith("CTRL-ALT ") || line.startsWith("CTRL-SHIFT ")) {
            bool isAlt = line.startsWith("CTRL-ALT ");
            int sp = line.lastIndexOf(' ');
            char k = line.charAt(sp + 1);
            uint8_t kc = 0;
            if (k >= 'a' && k <= 'z') kc = 4 + (k - 'a');
            else if (k >= 'A' && k <= 'Z') kc = 4 + (k - 'A');
            send_keyboard(isAlt ? 0x05 : 0x03, kc);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    f.close();
    Serial.println(TAG " Script finished");
    script_running = false;
    script_abort = false;
    if (!args->is_ble) {
        stop_usb_hid();
    }
    vTaskDelete(nullptr);
}

bool hid_svc_start(void) {
    if (hid_active) return true;

    ble_uart_stop();
    delay(100);

    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("SCR-Keyboard");
    }

    hid_server = NimBLEDevice::createServer();
    hid_server->setCallbacks(new HIDServerCallbacks());

    hid_device = new NimBLEHIDDevice(hid_server);
    hid_device->setManufacturer("WDGWatch");
    hid_device->setPnp(0x02, 0x045E, 0x02FD, 0x0110);
    hid_device->setHidInfo(0x00, 0x02);

    hid_device->setReportMap((uint8_t*)HID_DESC, sizeof(HID_DESC));

    kb_input    = hid_device->getInputReport(HID_RPT_KEYBOARD);
    kb_output   = hid_device->getOutputReport(HID_RPT_KEYBOARD);
    media_input = hid_device->getInputReport(HID_RPT_MEDIA);
    mouse_input = hid_device->getInputReport(HID_RPT_MOUSE);

    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName("SCR-Keyboard");
    advData.setAppearance(HID_KEYBOARD);
    advData.setCompleteServices(NimBLEUUID(hid_device->getHidService()->getUUID()));
    adv->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setName("SCR-Keyboard");
    adv->setScanResponseData(scanData);

    adv->start();

    hid_active = true;
    Serial.println(TAG " BLE HID advertising started");
    return true;
}

void hid_svc_stop(void) {
    if (!hid_active) return;

    airmouse_active = false;
    script_abort = true;

    NimBLEDevice::stopAdvertising();
    if (hid_server) {
        hid_server->disconnect(0);
    }
    delay(100);
    NimBLEDevice::deinit(true);

    hid_device  = nullptr;
    kb_input    = nullptr;
    kb_output   = nullptr;
    media_input = nullptr;
    mouse_input = nullptr;
    hid_server  = nullptr;
    hid_active    = false;
    hid_connected = false;
    script_running = false;
    Serial.println(TAG " BLE HID stopped");
}

bool hid_svc_is_active(void)    { return hid_active; }
bool hid_svc_is_connected(void) { return hid_connected; }
bool hid_svc_is_usb_connected(void) {
    return tud_mounted();
}
const char* hid_svc_get_name(void) { return "SCR-Keyboard"; }

void hid_svc_set_layout(KeyboardLayoutId layout) {
    if (layout < KB_LAYOUT_COUNT) {
        current_layout = layout;
    }
}

KeyboardLayoutId hid_svc_get_layout(void) {
    return current_layout;
}

const char* hid_svc_get_layout_name(KeyboardLayoutId layout) {
    if (layout < KB_LAYOUT_COUNT) {
        return LAYOUT_NAMES[layout];
    }
    return "US";
}

void hid_svc_run_script(const char *sd_path, bool ble) {
    if (script_running) return;

    if (ble) {
        if (!hid_active && !hid_svc_start()) return;
    }

    strncpy(script_args.path, sd_path, sizeof(script_args.path) - 1);
    script_args.path[sizeof(script_args.path) - 1] = 0;
    script_args.is_ble = ble;
    script_running = true;
    script_abort   = false;

    xTaskCreatePinnedToCore(script_task, "hid_script", 8192,
                            &script_args, 4, nullptr, 1);
}

bool hid_svc_is_running_script(void) { return script_running; }

void hid_svc_abort_script(void) {
    script_abort  = true;
    script_running = false;
}

void hid_media_vol_up(void)    { send_media(0x01); }
void hid_media_vol_down(void)  { send_media(0x02); }
void hid_media_screenshot(void) {

    send_keyboard(0, 0x46);
}

bool hid_airmouse_start(void) {
    if (airmouse_active) return true;

    if (!hid_active) {
        start_usb_hid();

        uint32_t t = millis();
        while (!hid_svc_is_usb_connected() && (millis() - t < 2000)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (!hid_svc_is_usb_connected()) {
            stop_usb_hid();
            return false;
        }
    }

    airmouse_active = true;
    last_x = 0; last_y = 0;
    xTaskCreatePinnedToCore(airmouse_task, "airmouse", 4096,
                            nullptr, 5, nullptr, 1);
    return true;
}

void hid_airmouse_stop(void) {
    if (!airmouse_active) return;
    airmouse_active = false;

    if (usb_started) {
        stop_usb_hid();
    }
}

bool hid_airmouse_is_active(void) { return airmouse_active; }

void hid_service_loop(void) {

}
