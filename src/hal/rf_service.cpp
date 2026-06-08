#include "rf_service.h"
#include <RadioLib.h>
#include <LilyGoLib.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "lora_service.h"

static volatile bool jammer_active   = false;
static volatile uint32_t jam_freq_hz = 433920000;
static TaskHandle_t jammer_task_h    = nullptr;

static volatile bool tesla_pending   = false;
static volatile bool tesla_sending   = false;

static const uint16_t TESLA_RAW[] = {
    400,  400,  400,  400,  400,  400,  400,  400,  400,  400,
    400,  400,  400,  400,  400,  400,  400,  400,  400,  400,
    400,  400,  400,  400,  400, 1200,  400,  400,  400,  400,
    800,  800,  400,  400,  800,  800,  800,  800,  400,  400,
    800,  800,  800,  800,  800,  800,  800,  800,  800,  800,
    400,  400,  800,  400,  400,  800,  800,  400,  400,  800,
    400,  400,  800,  400,  400,  400,  400,  800,  400,  400,
    400,  400,  800,  400,  400,  800,  800,  400,  400,  800,
    800,  800,  400,  400,  400,  400,  400,  400,  800,  400,
    400,  800,  400,  400,  800, 1200
};
static const size_t TESLA_RAW_LEN = sizeof(TESLA_RAW) / sizeof(uint16_t);

static void jammer_task(void *arg) {

    float center_mhz = (float)jam_freq_hz / 1000000.0f;
    float step_mhz   = 0.02f;
    float range_mhz  = 0.5f;
    float start_mhz  = center_mhz - range_mhz;
    float end_mhz    = center_mhz + range_mhz;

    radio.setFrequency(center_mhz);
    radio.setOutputPower(22);
    radio.setDio2AsRfSwitch(true);
    radio.transmitDirect();

    Serial.printf("[RF] Jammer started @ %.3f MHz\n", center_mhz);

    while (jammer_active) {
        for (float f = start_mhz; f <= end_mhz && jammer_active; f += step_mhz) {
            radio.setFrequency(f);
            esp_rom_delay_us(200);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    radio.standby();
    Serial.println("[RF] Jammer stopped");
    jammer_task_h = nullptr;
    vTaskDelete(nullptr);
}

void rf_service_init(void) {
    Serial.println("[RF] Service initialized");
}

bool rf_jammer_start(uint32_t freq_hz) {
    if (jammer_active) return true;

    lora_svc_stop();
    vTaskDelay(pdMS_TO_TICKS(100));

    jam_freq_hz  = freq_hz;
    jammer_active = true;

    xTaskCreatePinnedToCore(
        jammer_task, "rf_jammer", 4096, nullptr, 5, &jammer_task_h, 1
    );
    return true;
}

void rf_jammer_stop(void) {
    if (!jammer_active) return;
    jammer_active = false;

    if (jammer_task_h != nullptr) {
        vTaskDelete(jammer_task_h);
        jammer_task_h = nullptr;
    }

    radio.standby();
    Serial.println("[RF] Jammer stopped, radio in standby");
}

bool rf_jammer_is_active(void) { return jammer_active; }
uint32_t rf_jammer_get_freq(void) { return jam_freq_hz; }

void rf_tesla_send(void) {
    if (tesla_sending || jammer_active) return;
    tesla_pending = true;
}

bool rf_tesla_is_sending(void) { return tesla_sending; }

void rf_service_loop(void) {
    if (!tesla_pending) return;
    tesla_pending = false;
    tesla_sending = true;

    lora_svc_stop();
    delay(80);

    Serial.println("[RF] Tesla Port: sending signal @ 433.92 MHz");

    radio.setFrequency(433.92f);
    radio.setOutputPower(22);
    radio.setDio2AsRfSwitch(true);

    for (size_t i = 0; i < TESLA_RAW_LEN; i++) {
        if (i % 2 == 0) {
            radio.transmitDirect();
        } else {
            radio.standby();
        }
        esp_rom_delay_us(TESLA_RAW[i]);
    }

    radio.standby();
    Serial.println("[RF] Tesla Port: done");
    tesla_sending = false;
}
