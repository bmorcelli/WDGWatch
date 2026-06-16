#include "audio_record.h"
#include <SD.h>
#include <LilyGoLib.h>
#include <driver/i2s.h>

volatile bool audio_rec_active = false;
static TaskHandle_t audio_rec_task_handle = nullptr;
static uint32_t audio_rec_bytes_written = 0;
static char audio_rec_filename[64] = "";

struct __attribute__((packed)) wav_header_t {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
    char subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];
    uint32_t subchunk2_size;
};

void audio_rec_task(void *pvParameters) {
    File f = SD.open(audio_rec_filename, FILE_WRITE);
    if (!f) {
        Serial.printf("[REC] Failed to open file for recording: %s\n", audio_rec_filename);
        audio_rec_active = false;
        audio_rec_task_handle = nullptr;
        vTaskDelete(NULL);
        return;
    wav_header_t header;
    memcpy(header.chunk_id, "RIFF", 4);
    header.chunk_size = 36; 
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1_id, "fmt ", 4);
    header.subchunk1_size = 16;
    header.audio_format = 1; 
    header.num_channels = 1; 
    header.sample_rate = 16000;
    header.bits_per_sample = 16;
    header.byte_rate = 16000 * 2 * 1;
    header.block_align = 2;
    memcpy(header.subchunk2_id, "data", 4);
    header.subchunk2_size = 0;

    f.write((uint8_t*)&header, sizeof(header));
    audio_rec_bytes_written = 0;

    const size_t chunk_size = 1024;
    uint8_t *buf = (uint8_t *)malloc(chunk_size);
    if (!buf) {
        Serial.println("[REC] Failed to allocate recording buffer");
        f.close();
        audio_rec_active = false;
        audio_rec_task_handle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    Serial.printf("[REC] Recording started: %s\n", audio_rec_filename);

    uint32_t log_ms = millis();
    while (audio_rec_active) {
        size_t bytes_read = 0;
        if (instance.mic.read(buf, chunk_size, &bytes_read, pdMS_TO_TICKS(100))) {
            if (bytes_read > 0) {
                f.write(buf, bytes_read);
                audio_rec_bytes_written += bytes_read;
            }
        }
        
        if (millis() - log_ms > 2000) {
            log_ms = millis();
            Serial.printf("[REC] Recording... Bytes written so far: %u\n", audio_rec_bytes_written);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    if (f.seek(0)) {
        header.chunk_size = audio_rec_bytes_written + 36;
        header.subchunk2_size = audio_rec_bytes_written;
        f.write((uint8_t*)&header, sizeof(header));
    }

    f.close();
    free(buf);

    Serial.printf("[REC] Recording finished. Wrote %u bytes.\n", audio_rec_bytes_written);
    audio_rec_task_handle = nullptr;
    vTaskDelete(NULL);
}

bool audio_rec_start(const char* filename) {
    if (audio_rec_active) return false;
    
    if (!SD.exists("/rec")) {
        SD.mkdir("/rec");
    }

    strncpy(audio_rec_filename, filename, sizeof(audio_rec_filename) - 1);
    audio_rec_active = true;

    xTaskCreatePinnedToCore(
        audio_rec_task,
        "audio_rec_task",
        4096,
        NULL,
        5,
        &audio_rec_task_handle,
        1
    );

    return true;
}

void audio_rec_stop() {
    if (!audio_rec_active) return;
    audio_rec_active = false;
    while (audio_rec_task_handle != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool audio_rec_is_recording() {
    return audio_rec_active;
}

const char* audio_rec_get_filename() {
    return audio_rec_filename;
}
