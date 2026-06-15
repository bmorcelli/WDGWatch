#include "rf_service.h"
#include <RadioLib.h>
#include <LilyGoLib.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/portmacro.h"
#include "../config.h"
#include "lora_service.h"

static volatile bool jammer_active   = false;
static volatile uint32_t jam_freq_hz = 433920000;
static TaskHandle_t jammer_task_h    = nullptr;

static volatile bool tesla_pending   = false;
static volatile bool tesla_sending   = false;
static TaskHandle_t tesla_task_h     = nullptr;

static volatile bool radio_lifecycle_on = false;

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

static bool radio_begin_with_tcxo(float freq_mhz, float tcxo) {
    return radio.begin(freq_mhz, 125.0f, 9, 7,
                       RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                       14, 8, tcxo) == RADIOLIB_ERR_NONE;
}

static bool radio_configure(float freq_mhz) {
    if (radio_begin_with_tcxo(freq_mhz, 1.6f)) goto ok;
    if (radio_begin_with_tcxo(freq_mhz, 0.0f)) goto ok;
    if (radio_begin_with_tcxo(freq_mhz, 3.0f)) goto ok;
    Serial.printf("[RF] Radio configure failed @ %.3f MHz\n", freq_mhz);
    return false;
ok:
    radio.setOutputPower(22);
    radio.setDio2AsRfSwitch(true);
    return true;
}

bool rf_radio_wake(float freq_mhz) {
    if (radio_lifecycle_on) return true;

    lora_svc_stop();
    vTaskDelay(pdMS_TO_TICKS(100));

    instance.powerControl(POWER_RADIO, true);
    vTaskDelay(pdMS_TO_TICKS(50));     

    if (!radio_configure(freq_mhz)) {
        instance.powerControl(POWER_RADIO, false);
        return false;
    }

    radio.standby();                   
    radio_lifecycle_on = true;
    Serial.printf("[RF] Radio warm @ %.3f MHz\n", freq_mhz);
    return true;
}

void rf_radio_sleep(void) {
    if (!radio_lifecycle_on) return;

    if (jammer_active) rf_jammer_stop();

    radio.standby();
    instance.powerControl(POWER_RADIO, false);
    radio_lifecycle_on = false;
    Serial.println("[RF] Radio powered down (lifecycle)");
}

static void jammer_task(void *arg) {
    float center_mhz = (float)jam_freq_hz / 1000000.0f;
    float step_mhz   = 0.02f;
    float range_mhz  = 0.5f;
    float start_mhz  = center_mhz - range_mhz;
    float end_mhz    = center_mhz + range_mhz;

    
    
    if (!radio_lifecycle_on) {
        if (!rf_radio_wake(center_mhz)) {
            jammer_active = false;
            jammer_task_h = nullptr;
            vTaskDelete(nullptr);
            return;
        }
    } else {
        radio_configure(center_mhz);
    }

    radio.transmitDirect();
    Serial.printf("[RF] Jammer active @ %.3f MHz\n", center_mhz);

    while (jammer_active) {
        for (float f = start_mhz; f <= end_mhz && jammer_active; f += step_mhz) {
            radio.setFrequency(f);
            esp_rom_delay_us(200);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    radio.standby();
    
    if (!radio_lifecycle_on) {
        instance.powerControl(POWER_RADIO, false);
    }
    Serial.println("[RF] Jammer stopped");
    jammer_task_h = nullptr;
    vTaskDelete(nullptr);
}

static void tesla_task(void *arg) {
    Serial.println("[RF] Tesla: task started");

    
    if (!radio_configure(433.92f)) {
        Serial.println("[RF] Tesla: radio configure failed");
        tesla_sending = false;
        tesla_task_h  = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    
    radio.transmitDirect();
    esp_rom_delay_us(500);
    radio.standby();
    esp_rom_delay_us(200);

    Serial.println("[RF] Tesla: PLL warm-up done, sending burst...");

    
    for (size_t i = 0; i < TESLA_RAW_LEN; i++) {
        if (i % 2 == 0) {
            radio.transmitDirect();
        } else {
            radio.standby();
        }
        esp_rom_delay_us(TESLA_RAW[i]);
    }

    radio.standby();

    
    if (radio_lifecycle_on) {
        radio_configure((float)jam_freq_hz / 1000000.0f);
        radio.standby();
    } else {
        instance.powerControl(POWER_RADIO, false);
    }

    Serial.println("[RF] Tesla: burst done");
    tesla_sending = false;
    tesla_task_h  = nullptr;
    vTaskDelete(nullptr);
}

void rf_service_init(void) {
    Serial.println("[RF] Service initialized");
}

bool rf_jammer_start(uint32_t freq_hz) {
    if (jammer_active) return true;

    jam_freq_hz   = freq_hz;
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
    if (!radio_lifecycle_on) {
        instance.powerControl(POWER_RADIO, false);
    }
    Serial.println("[RF] Jammer stopped");
}

bool rf_jammer_is_active(void) { return jammer_active; }
uint32_t rf_jammer_get_freq(void) { return jam_freq_hz; }

void rf_tesla_send(void) {
    if (tesla_sending || jammer_active) return;
    if (!radio_lifecycle_on) {
        
        if (!rf_radio_wake(433.92f)) return;
    }
    tesla_sending = true;
    
    xTaskCreatePinnedToCore(
        tesla_task, "rf_tesla", 4096, nullptr, 6, &tesla_task_h, 1
    );
}

bool rf_tesla_is_sending(void) { return tesla_sending; }

void rf_service_loop(void) {
    
    (void)tesla_pending;
}
