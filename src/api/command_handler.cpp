#include "command_handler.h"
#include <ArduinoJson.h>
#include <LilyGoLib.h>
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include "../config.h"
#include "../hal/power_hal.h"
#include "../hal/haptic.h"
#include "../hal/time_sync.h"
#include "../hal/nfc_service.h"
#include "../hal/lora_service.h"
#include "../hal/recon_service.h"
#include "../ui/watchface.h"
#include "../hal/rf_service.h"
#include "../hal/hid_service.h"
#include "../apps/pet_app.h"

static api_event_cb_t event_cb = nullptr;

static void push_event(const char *json) {
    if (event_cb) event_cb(json);
}

static char* cmd_status(void) {
    JsonDocument doc;
    doc["type"] = "status";

    struct tm ti;
    char tbuf[12], dbuf[20];
    if (getLocalTime(&ti, 0)) {
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
        snprintf(dbuf, sizeof(dbuf), "%04d-%02d-%02d", ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday);
    } else {
        RTC_DateTime dt = instance.rtc.getDateTime();
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", dt.getHour(), dt.getMinute(), dt.getSecond());
        snprintf(dbuf, sizeof(dbuf), "%04d-%02d-%02d", dt.getYear(), dt.getMonth(), dt.getDay());
    }
    doc["time"] = tbuf;
    doc["date"] = dbuf;
    doc["bat"] = power_hal_battery_percent();
    doc["bat_v"] = power_hal_battery_voltage();
    doc["charging"] = power_hal_is_charging();
    doc["ntp"] = time_sync_is_synced();
    doc["heap"] = ESP.getFreeHeap() / 1024;
    doc["lora"] = lora_svc_is_running();
    doc["nfc"] = nfc_svc_is_scanning();

    if (instance.gps.location.isValid()) {
        doc["gps_lat"] = instance.gps.location.lat();
        doc["gps_lon"] = instance.gps.location.lng();
        doc["gps_sats"] = instance.gps.satellites.value();
    }

    char *buf = (char*)malloc(512);
    serializeJson(doc, buf, 512);
    return buf;
}

static char* cmd_version(void) {
    char *buf = (char*)malloc(192);
    snprintf(buf, 192, "{\"version\":\"WDGWatch v0.1.1\",\"codename\":\"SCR Terminal\",\"hw\":\"T-Watch Ultra ESP32-S3\",\"features\":[\"nfc\",\"lora\",\"gps\",\"recon\",\"compass\"]}");
    return buf;
}

static char* cmd_nfc_scan(void)    { nfc_svc_request_scan(); return strdup("{\"ok\":true,\"msg\":\"nfc scanning\"}"); }
static char* cmd_nfc_stop(void)    { nfc_svc_request_stop(); return strdup("{\"ok\":true}"); }
static char* cmd_nfc_save(void)    { nfc_svc_request_save(); return strdup("{\"ok\":true,\"msg\":\"tag saved\"}"); }
static char* cmd_nfc_export(void)  { nfc_svc_request_export(); return strdup("{\"ok\":true,\"msg\":\"exported\"}"); }

static char* cmd_nfc_list(void) {
    JsonDocument doc;
    doc["type"] = "nfc_tags";
    JsonArray arr = doc["tags"].to<JsonArray>();
    for (int i = 0; i < nfc_svc_saved_count(); i++) {
        arr.add(nfc_svc_tag_name(i));
    }
    char *buf = (char*)malloc(512);
    serializeJson(doc, buf, 512);
    return buf;
}

static char* cmd_nfc_delete(int idx) {
    nfc_svc_request_delete(idx);
    return strdup("{\"ok\":true}");
}

static char* cmd_nfc_download(int idx) {
    char fn[32]; snprintf(fn, sizeof(fn), "/nfc/tag_%d.nfc", idx);
    File f = SD.open(fn, FILE_READ);
    if (!f) return strdup("{\"error\":\"file not found\"}");

    int fsize = f.size();
    if (fsize > 4096) { f.close(); return strdup("{\"error\":\"file too large\"}"); }

    uint8_t *raw = (uint8_t*)malloc(fsize);
    f.read(raw, fsize); f.close();

    size_t b64_len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64_len, raw, fsize);
    char *b64 = (char*)malloc(b64_len + 1);
    mbedtls_base64_encode((uint8_t*)b64, b64_len, &b64_len, raw, fsize);
    b64[b64_len] = 0;
    free(raw);

    JsonDocument doc;
    doc["type"] = "nfc_file";
    doc["name"] = fn;
    doc["data"] = b64;
    free(b64);

    char *buf = (char*)malloc(b64_len + 128);
    serializeJson(doc, buf, b64_len + 128);
    return buf;
}

static char* cmd_lora_start(void)  { lora_svc_start(); return strdup("{\"ok\":true,\"msg\":\"meshcore started\"}"); }
static char* cmd_lora_stop(void)   { lora_svc_stop(); return strdup("{\"ok\":true}"); }
static char* cmd_lora_advert(void) { lora_svc_send_advert(); return strdup("{\"ok\":true}"); }

static char* cmd_lora_send(const char *text) {
    lora_svc_send_message(text);
    return strdup("{\"ok\":true}");
}

static char* cmd_lora_history(void) {
    JsonDocument doc;
    JsonArray arr = doc["messages"].to<JsonArray>();
    int count = lora_svc_message_count();
    for (int i = 0; i < count && i < 20; i++) {
        const MeshMsg *m = lora_svc_get_message(i);
        if (!m) continue;
        JsonObject o = arr.add<JsonObject>();
        o["ch"] = m->channel; o["text"] = m->text;
        o["hops"] = m->hops; o["rssi"] = (int)m->rssi;
        o["ts"] = m->timestamp;
    }
    char *buf = (char*)malloc(2048);
    serializeJson(doc, buf, 2048);
    return buf;
}

static char* cmd_recon_wifi(void)    { recon_request_wifi_scan(); return strdup("{\"ok\":true,\"msg\":\"wifi scanning\"}"); }
static char* cmd_recon_ble(int dur)  { recon_request_ble_scan(dur); return strdup("{\"ok\":true,\"msg\":\"ble scanning\"}"); }
static char* cmd_recon_stop(void)    { recon_request_stop(); return strdup("{\"ok\":true}"); }

static char* cmd_recon_deauth(const char *bssid, int ch) {
    recon_request_deauth(bssid, ch);
    return strdup("{\"ok\":true,\"msg\":\"deauth started\"}");
}

static char* cmd_recon_results(void) {
    JsonDocument doc;
    JsonArray wf = doc["wifi"].to<JsonArray>();
    for (int i = 0; i < recon_wifi_count(); i++) {
        const ReconWiFi *n = recon_get_wifi(i);
        if (!n) continue;
        JsonObject o = wf.add<JsonObject>();
        o["ssid"] = n->ssid; o["bssid"] = n->bssid;
        o["rssi"] = n->rssi; o["ch"] = n->channel; o["auth"] = n->auth;
    }
    JsonArray bl = doc["ble"].to<JsonArray>();
    for (int i = 0; i < recon_ble_count(); i++) {
        const BleDevice *d = recon_get_ble(i);
        if (!d) continue;
        JsonObject o = bl.add<JsonObject>();
        o["mac"] = d->mac; o["name"] = d->name;
        o["rssi"] = d->rssi; o["airtag"] = d->is_airtag;
    }
    char *buf = (char*)malloc(4096);
    serializeJson(doc, buf, 4096);
    return buf;
}

static char* cmd_brightness(int v) {
    instance.setBrightness(v);
    return strdup("{\"ok\":true}");
}

static char* cmd_haptic(void) {
    haptic_buzz();
    return strdup("{\"ok\":true}");
}

static char* cmd_watchface(const char *action) {
    if (strcmp(action, "next") == 0) watchface_next();
    else if (strcmp(action, "prev") == 0) watchface_prev();
    return strdup("{\"ok\":true}");
}

static char* cmd_reboot(void) {
    return strdup("{\"ok\":true,\"msg\":\"rebooting\"}");
}

static char* cmd_gps_on(void)  { instance.powerControl(POWER_GPS, true); return strdup("{\"ok\":true}"); }
static char* cmd_gps_off(void) { instance.powerControl(POWER_GPS, false); return strdup("{\"ok\":true}"); }

static char* cmd_compass(void) {
    char *buf = (char*)malloc(128);
    snprintf(buf, 128, "{\"heading\":0,\"roll\":0,\"pitch\":0}");
    return buf;
}

static char* cmd_sensor_data(void) {
    JsonDocument doc;
    doc["bat"] = power_hal_battery_percent();
    doc["bat_v"] = power_hal_battery_voltage();
    doc["charging"] = power_hal_is_charging();
    doc["heap_kb"] = ESP.getFreeHeap() / 1024;
    doc["uptime_s"] = millis() / 1000;
    if (instance.gps.location.isValid()) {
        doc["gps_lat"] = instance.gps.location.lat();
        doc["gps_lon"] = instance.gps.location.lng();
        doc["gps_sats"] = instance.gps.satellites.value();
    }
    char *buf = (char*)malloc(256);
    serializeJson(doc, buf, 256);
    return buf;
}

char* api_handle_command(const char *json_cmd) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_cmd);
    if (err) return strdup("{\"error\":\"invalid json\"}");

    const char *cmd = doc["cmd"] | "";

    if (strcmp(cmd, "status") == 0)     return cmd_status();
    if (strcmp(cmd, "version") == 0)    return cmd_version();
    if (strcmp(cmd, "brightness") == 0) return cmd_brightness(doc["params"]["v"] | 128);
    if (strcmp(cmd, "haptic") == 0)     return cmd_haptic();
    if (strcmp(cmd, "watchface") == 0)  return cmd_watchface(doc["params"]["style"] | "next");
    if (strcmp(cmd, "reboot") == 0)     return cmd_reboot();
    if (strcmp(cmd, "gps_on") == 0)     return cmd_gps_on();
    if (strcmp(cmd, "gps_off") == 0)    return cmd_gps_off();
    if (strcmp(cmd, "compass") == 0)    return cmd_compass();
    if (strcmp(cmd, "sensor_data") == 0) return cmd_sensor_data();

    if (strcmp(cmd, "nfc_scan") == 0)   return cmd_nfc_scan();
    if (strcmp(cmd, "nfc_stop") == 0)   return cmd_nfc_stop();
    if (strcmp(cmd, "nfc_save") == 0)   return cmd_nfc_save();
    if (strcmp(cmd, "nfc_list") == 0)   return cmd_nfc_list();
    if (strcmp(cmd, "nfc_export") == 0) return cmd_nfc_export();
    if (strcmp(cmd, "nfc_delete") == 0) return cmd_nfc_delete(doc["params"]["idx"] | 0);
    if (strcmp(cmd, "nfc_download") == 0) return cmd_nfc_download(doc["params"]["idx"] | 0);

    if (strcmp(cmd, "lora_start") == 0)   return cmd_lora_start();
    if (strcmp(cmd, "lora_stop") == 0)    return cmd_lora_stop();
    if (strcmp(cmd, "lora_send") == 0)    return cmd_lora_send(doc["params"]["text"] | "");
    if (strcmp(cmd, "lora_advert") == 0)  return cmd_lora_advert();
    if (strcmp(cmd, "lora_history") == 0) return cmd_lora_history();

    if (strcmp(cmd, "recon_wifi") == 0)   return cmd_recon_wifi();
    if (strcmp(cmd, "recon_ble") == 0)    return cmd_recon_ble(doc["params"]["duration"] | 10);
    if (strcmp(cmd, "recon_stop") == 0)   return cmd_recon_stop();
    if (strcmp(cmd, "recon_deauth") == 0) return cmd_recon_deauth(doc["params"]["bssid"] | "", doc["params"]["ch"] | 0);
    if (strcmp(cmd, "recon_results") == 0) return cmd_recon_results();
    if (strcmp(cmd, "deauth_all") == 0)   { recon_request_deauth_all(); return strdup("{\"ok\":true,\"msg\":\"blackout started\"}"); }
    if (strcmp(cmd, "sniffer_start") == 0) { recon_request_sniffer(doc["params"]["ch"] | 0); return strdup("{\"ok\":true,\"msg\":\"sniffer started\"}"); }
    if (strcmp(cmd, "sniffer_stop") == 0)  { recon_request_stop(); return strdup("{\"ok\":true}"); }
    if (strcmp(cmd, "deauth_detect") == 0) { recon_request_deauth_detect(); return strdup("{\"ok\":true,\"msg\":\"deauth detector started\"}"); }
    if (strcmp(cmd, "evil_twin") == 0)     { recon_request_evil_twin(doc["params"]["ssid"] | "FreeWiFi", doc["params"]["ch"] | 6); return strdup("{\"ok\":true,\"msg\":\"evil twin started\"}"); }
    if (strcmp(cmd, "evil_twin_stop") == 0) { recon_request_stop(); return strdup("{\"ok\":true}"); }

    if (strcmp(cmd, "rf_jammer_start") == 0) {
        uint32_t freq = doc["params"]["freq"] | 433920000;
        rf_jammer_start(freq);
        return strdup("{\"ok\":true,\"msg\":\"rf jammer active\"}");
    }
    if (strcmp(cmd, "rf_jammer_stop") == 0) {
        rf_jammer_stop();
        return strdup("{\"ok\":true,\"msg\":\"rf jammer stopped\"}");
    }
    if (strcmp(cmd, "rf_tesla_send") == 0) {
        rf_tesla_send();
        return strdup("{\"ok\":true,\"msg\":\"tesla signal sent\"}");
    }
    if (strcmp(cmd, "rf_status") == 0) {
        JsonDocument rdoc;
        rdoc["type"] = "rf_status";
        rdoc["active"] = rf_jammer_is_active();
        rdoc["freq"] = rf_jammer_get_freq();
        rdoc["tesla_sending"] = rf_tesla_is_sending();
        char *buf = (char*)malloc(128);
        serializeJson(rdoc, buf, 128);
        return buf;
    }

    if (strcmp(cmd, "hid_start") == 0) {
        hid_svc_start();
        return strdup("{\"ok\":true,\"msg\":\"hid advertising started\"}");
    }
    if (strcmp(cmd, "hid_stop") == 0) {
        hid_svc_stop();
        return strdup("{\"ok\":true,\"msg\":\"hid stopped\"}");
    }
    if (strcmp(cmd, "hid_run_script") == 0) {
        const char *path = doc["params"]["path"] | "";
        bool ble = doc["params"]["ble"] | false;
        hid_svc_run_script(path, ble);
        return strdup("{\"ok\":true,\"msg\":\"script started\"}");
    }
    if (strcmp(cmd, "hid_abort_script") == 0) {
        hid_svc_abort_script();
        return strdup("{\"ok\":true,\"msg\":\"script aborted\"}");
    }
    if (strcmp(cmd, "hid_status") == 0) {
        JsonDocument hdoc;
        hdoc["type"] = "hid_status";
        hdoc["active"] = hid_svc_is_active();
        hdoc["connected"] = hid_svc_is_connected();
        hdoc["usb_connected"] = hid_svc_is_usb_connected();
        hdoc["running_script"] = hid_svc_is_running_script();
        hdoc["name"] = hid_svc_get_name();
        char *buf = (char*)malloc(192);
        serializeJson(hdoc, buf, 192);
        return buf;
    }

    if (strcmp(cmd, "pet_feed") == 0) {
        pet_feed_action();
        return strdup("{\"ok\":true,\"msg\":\"pet fed\"}");
    }
    if (strcmp(cmd, "pet_heal") == 0) {
        pet_heal_action();
        return strdup("{\"ok\":true,\"msg\":\"pet healed\"}");
    }
    if (strcmp(cmd, "pet_clean") == 0) {
        pet_clean_action();
        return strdup("{\"ok\":true,\"msg\":\"pet cleaned\"}");
    }
    if (strcmp(cmd, "pet_status") == 0) {
        uint32_t lvl = 0, xp = 0, eng = 0, hp = 0, cln = 0, pps = 0;
        pet_get_stats(&lvl, &xp, &eng, &hp, &cln, &pps);
        JsonDocument pdoc;
        pdoc["type"] = "pet_status";
        pdoc["level"] = lvl;
        pdoc["xp"] = xp;
        pdoc["energy"] = eng;
        pdoc["health"] = hp;
        pdoc["cleanliness"] = cln;
        pdoc["poops"] = pps;
        char *buf = (char*)malloc(192);
        serializeJson(pdoc, buf, 192);
        return buf;
    }

    return strdup("{\"error\":\"unknown command\"}");
}

void api_init(void) {
    event_cb = nullptr;
}

void api_set_event_callback(api_event_cb_t cb) {
    event_cb = cb;
}

void api_loop(void) {
    static bool was_scanning = false;
    bool scanning_now = recon_is_scanning();
    if (was_scanning && !scanning_now) {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"event\":\"scan_done\",\"wifi\":%d,\"ble\":%d}",
                 recon_wifi_count(), recon_ble_count());
        push_event(buf);
    }
    was_scanning = scanning_now;

    static int last_deauth_count = 0;
    int dc = recon_deauth_detect_count();
    if (dc > last_deauth_count) {
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"event\":\"deauth_detected\",\"count\":%d}", dc);
        push_event(buf);
        last_deauth_count = dc;
    }

    if (nfc_svc_tag_detected_ble()) {
        JsonDocument doc;
        doc["event"] = "nfc_tag";
        doc["uid"] = nfc_svc_last_uid();
        doc["ndef"] = nfc_svc_last_ndef();
        char buf[256];
        serializeJson(doc, buf, sizeof(buf));
        push_event(buf);
    }

    if (lora_svc_has_new_message_ble()) {
        const MeshMsg *m = lora_svc_last_message();
        if (m) {
            JsonDocument doc;
            doc["event"] = "lora_msg";
            doc["channel"] = m->channel;
            doc["text"] = m->text;
            doc["hops"] = m->hops;
            doc["rssi"] = (int)m->rssi;
            doc["ts"] = m->timestamp;
            char buf[400];
            serializeJson(doc, buf, sizeof(buf));
            push_event(buf);
        }
    }
}
