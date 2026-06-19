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
#include "../apps/gps_app.h"

static api_event_cb_t event_cb = nullptr;
static uint32_t jammer_end_time = 0;
static bool jammer_has_timeout = false;

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

static char* cmd_lora_start(int mode) {
    lora_svc_start((LoraMode)mode);
    const char *mode_name = "meshcore";
    if (mode == 1) mode_name = "meshtastic";
    else if (mode == 2) mode_name = "pocsag";
    else if (mode == 3) mode_name = "bruce";
    char *buf = (char*)malloc(64);
    snprintf(buf, 64, "{\"ok\":true,\"msg\":\"%s started\"}", mode_name);
    return buf;
}
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

static char* cmd_lora_set_ric(uint32_t ric) {
    lora_svc_set_ric(ric);
    return strdup("{\"ok\":true,\"msg\":\"ric updated\"}");
}

static char* cmd_lora_set_freq(float freq) {
    lora_svc_set_freq(freq);
    return strdup("{\"ok\":true,\"msg\":\"freq updated\"}");
}

static char* cmd_lora_set_name(const char *name) {
    lora_svc_set_node_name(name);
    return strdup("{\"ok\":true,\"msg\":\"node name updated\"}");
}

static char* cmd_nfc_emulate(void) {
    nfc_svc_request_emulate();
    return strdup("{\"ok\":true,\"msg\":\"emulation status toggled\"}");
}

static char* cmd_nfc_select_next(void) {
    nfc_svc_request_select_next();
    JsonDocument doc;
    doc["ok"] = true;
    doc["msg"] = "selected next card";
    doc["idx"] = nfc_svc_selected_idx();
    int idx = nfc_svc_selected_idx();
    doc["name"] = (idx >= 0 && idx < nfc_svc_saved_count()) ? nfc_svc_tag_name(idx) : "none";
    char *buf = (char*)malloc(128);
    serializeJson(doc, buf, 128);
    return buf;
}

static char* cmd_nfc_status(void) {
    JsonDocument doc;
    doc["type"] = "nfc_status";
    doc["scanning"] = nfc_svc_is_scanning();
    doc["emulating"] = nfc_svc_is_emulating();
    doc["selected_idx"] = nfc_svc_selected_idx();
    int idx = nfc_svc_selected_idx();
    doc["selected_name"] = (idx >= 0 && idx < nfc_svc_saved_count()) ? nfc_svc_tag_name(idx) : "none";
    char *buf = (char*)malloc(256);
    serializeJson(doc, buf, 256);
    return buf;
}

static char* cmd_gps_status(void) {
    JsonDocument doc;
    doc["type"] = "gps_status";
    doc["enabled"] = gps_app_is_enabled();
    doc["wardriving"] = gps_app_is_wardriving_active();
    char *buf = (char*)malloc(128);
    serializeJson(doc, buf, 128);
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
        o["rssi"] = d->rssi; o["airtag"] = d->is_airtag; o["flipper"] = d->is_flipper;
    }
    char *buf = (char*)malloc(4096);
    serializeJson(doc, buf, 4096);
    return buf;
}

static char* cmd_recon_arp_start(void) {
    recon_request_arp_scan();
    return strdup("{\"ok\":true,\"msg\":\"arp scan started\"}");
}

static char* cmd_recon_arp_results(void) {
    JsonDocument doc;
    doc["type"] = "arp_results";
    doc["scanning"] = recon_is_arp_scanning();
    doc["progress"] = recon_arp_scan_progress();
    JsonArray arr = doc["devices"].to<JsonArray>();
    for (int i = 0; i < recon_arp_count(); i++) {
        const ArpDevice *d = recon_get_arp_device(i);
        if (!d) continue;
        JsonObject o = arr.add<JsonObject>();
        o["ip"] = d->ip; o["mac"] = d->mac; o["vendor"] = d->vendor;
    }
    char *buf = (char*)malloc(2048);
    serializeJson(doc, buf, 2048);
    return buf;
}

static char* cmd_recon_ip_sniff(const char *ip) {
    recon_request_ip_sniff(ip);
    return strdup("{\"ok\":true,\"msg\":\"ip sniff started\"}");
}

static char* cmd_recon_ip_sniff_results(void) {
    JsonDocument doc;
    doc["type"] = "ip_sniff_results";
    doc["active"] = recon_is_ip_sniffing();
    doc["target"] = recon_sniff_target_ip();
    JsonArray arr = doc["ips"].to<JsonArray>();
    for (int i = 0; i < recon_sniff_unique_ip_count(); i++) {
        arr.add(recon_sniff_get_ip(i));
    }
    char *buf = (char*)malloc(1024);
    serializeJson(doc, buf, 1024);
    return buf;
}

static char* cmd_recon_beacon_spam(JsonArray ssids) {
    char ssid_array[BEACON_SSID_COUNT][BEACON_SSID_LEN];
    int count = 0;
    for (size_t i = 0; i < ssids.size() && i < BEACON_SSID_COUNT; i++) {
        const char *val = ssids[i];
        if (val) {
            strncpy(ssid_array[count], val, BEACON_SSID_LEN - 1);
            ssid_array[count][BEACON_SSID_LEN - 1] = 0;
            count++;
        }
    }
    recon_request_beacon_spam(ssid_array, count);
    return strdup("{\"ok\":true,\"msg\":\"beacon spam started\"}");
}

static char* cmd_recon_adsb_start(double lat, double lon, const char *name) {
    recon_request_adsb_track(lat, lon, name);
    return strdup("{\"ok\":true,\"msg\":\"adsb tracking started\"}");
}

static char* cmd_recon_adsb_status(void) {
    JsonDocument doc;
    doc["type"] = "adsb_status";
    doc["active"] = recon_is_adsb_tracking();
    doc["status"] = recon_get_adsb_status();
    doc["has_aircraft"] = recon_adsb_has_aircraft();
    if (recon_adsb_has_aircraft()) {
        const AdsbAircraft *a = recon_get_adsb_aircraft();
        if (a) {
            JsonObject ac = doc["aircraft"].to<JsonObject>();
            ac["flight"] = a->flight;
            ac["reg"] = a->reg;
            ac["type"] = a->type;
            ac["alt"] = a->alt_baro;
            ac["gs"] = a->gs;
            ac["hdg"] = a->true_heading;
            ac["squawk"] = a->squawk;
            ac["emergency"] = a->emergency;
            ac["route"] = a->route;
            ac["dist"] = a->distance;
            ac["bearing"] = a->bearing;
        }
    }
    char *buf = (char*)malloc(512);
    serializeJson(doc, buf, 512);
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

static char* cmd_gps_on(void)  { gps_app_set_enabled(true); return strdup("{\"ok\":true}"); }
static char* cmd_gps_off(void) { gps_app_set_enabled(false); return strdup("{\"ok\":true}"); }

static char* cmd_compass(void) {
    char *buf = (char*)malloc(128);
    snprintf(buf, 128, "{\"heading\":0,\"roll\":0,\"pitch\":0}");
    return buf;
}

static char* cmd_hid_airmouse_start(void) {
    bool ok = hid_airmouse_start();
    if (ok) return strdup("{\"ok\":true,\"msg\":\"airmouse started\"}");
    else return strdup("{\"error\":\"failed to start airmouse (check USB/BLE connection)\"}");
}

static char* cmd_hid_airmouse_stop(void) {
    hid_airmouse_stop();
    return strdup("{\"ok\":true,\"msg\":\"airmouse stopped\"}");
}

static char* cmd_hid_airmouse_calibrate(void) {
    hid_airmouse_calibrate();
    return strdup("{\"ok\":true,\"msg\":\"airmouse calibrated\"}");
}

static char* cmd_hid_media(const char *action) {
    if (strcmp(action, "vol_up") == 0) hid_media_vol_up();
    else if (strcmp(action, "vol_down") == 0) hid_media_vol_down();
    else if (strcmp(action, "screenshot") == 0) hid_media_screenshot();
    else return strdup("{\"error\":\"invalid media action\"}");
    return strdup("{\"ok\":true}");
}

static char* cmd_hid_mouse_click(int buttons) {
    hid_mouse_click(buttons);
    return strdup("{\"ok\":true}");
}

static char* cmd_hid_mouse_scroll(int wheel) {
    hid_mouse_scroll(wheel);
    return strdup("{\"ok\":true}");
}

static char* cmd_sd_list(const char *path) {
    JsonDocument doc;
    doc["type"] = "sd_list";
    doc["path"] = path;
    JsonArray arr = doc["items"].to<JsonArray>();

    File dir = SD.open(path);
    if (dir && dir.isDirectory()) {
        File f = dir.openNextFile();
        int count = 0;
        while (f && count < 64) {
            JsonObject o = arr.add<JsonObject>();
            const char *fname = f.name();
            const char *slash = strrchr(fname, '/');
            if (slash) fname = slash + 1;
            o["name"] = fname;
            o["is_dir"] = f.isDirectory();
            o["size"] = f.size();
            count++;
            f = dir.openNextFile();
        }
        dir.close();
    } else {
        return strdup("{\"error\":\"invalid directory\"}");
    }
    char *buf = (char*)malloc(3072);
    serializeJson(doc, buf, 3072);
    return buf;
}

static char* cmd_sd_mkdir(const char *path) {
    if (SD.mkdir(path)) {
        return strdup("{\"ok\":true,\"msg\":\"directory created\"}");
    } else {
        return strdup("{\"error\":\"failed to create directory\"}");
    }
}

static char* cmd_sd_delete(const char *path) {
    bool ok = false;
    if (SD.exists(path)) {
        File f = SD.open(path);
        if (f) {
            bool isDir = f.isDirectory();
            f.close();
            if (isDir) {
                ok = SD.rmdir(path);
            } else {
                ok = SD.remove(path);
            }
        }
    }
    if (ok) return strdup("{\"ok\":true,\"msg\":\"deleted\"}");
    else return strdup("{\"error\":\"failed to delete\"}");
}

static char* cmd_sd_rename(const char *old_path, const char *new_path) {
    if (SD.rename(old_path, new_path)) {
        return strdup("{\"ok\":true,\"msg\":\"renamed/moved\"}");
    } else {
        return strdup("{\"error\":\"failed to rename/move\"}");
    }
}

static char* cmd_sd_copy(const char *src, const char *dest) {
    if (!SD.exists(src)) return strdup("{\"error\":\"source file not found\"}");
    File sFile = SD.open(src, FILE_READ);
    if (!sFile) return strdup("{\"error\":\"failed to open source file\"}");
    if (sFile.isDirectory()) {
        sFile.close();
        return strdup("{\"error\":\"cannot copy directories\"}");
    }
    File dFile = SD.open(dest, FILE_WRITE);
    if (!dFile) {
        sFile.close();
        return strdup("{\"error\":\"failed to create destination file\"}");
    }
    
    uint8_t *buffer = (uint8_t*)malloc(512);
    if (!buffer) {
        sFile.close();
        dFile.close();
        return strdup("{\"error\":\"out of memory\"}");
    }
    
    while (sFile.available()) {
        size_t bytesRead = sFile.read(buffer, 512);
        dFile.write(buffer, bytesRead);
    }
    
    free(buffer);
    sFile.close();
    dFile.close();
    return strdup("{\"ok\":true,\"msg\":\"file copied\"}");
}

static char* cmd_sd_read(const char *path) {
    if (!SD.exists(path)) return strdup("{\"error\":\"file not found\"}");
    File f = SD.open(path, FILE_READ);
    if (!f) return strdup("{\"error\":\"failed to open file\"}");
    if (f.isDirectory()) {
        f.close();
        return strdup("{\"error\":\"cannot read a directory\"}");
    }
    
    size_t fsize = f.size();
    if (fsize > 8192) {
        f.close();
        return strdup("{\"error\":\"file too large to view (max 8KB)\"}");
    }
    
    char *content = (char*)malloc(fsize + 1);
    if (!content) {
        f.close();
        return strdup("{\"error\":\"out of memory\"}");
    }
    f.readBytes(content, fsize);
    content[fsize] = 0;
    f.close();
    
    JsonDocument doc;
    doc["type"] = "sd_file_content";
    doc["path"] = path;
    doc["content"] = content;
    free(content);
    
    char *buf = (char*)malloc(fsize + 256);
    serializeJson(doc, buf, fsize + 256);
    return buf;
}

static char* cmd_sd_write(const char *path, const char *content) {
    File f = SD.open(path, FILE_WRITE);
    if (!f) return strdup("{\"error\":\"failed to open file for writing\"}");
    f.print(content);
    f.close();
    return strdup("{\"ok\":true,\"msg\":\"file written\"}");
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

static char* cmd_hid_list_scripts(void) {
    JsonDocument doc;
    doc["type"] = "hid_scripts";
    JsonArray arr = doc["scripts"].to<JsonArray>();

    if (SD.exists("/badusb")) {
        File dir = SD.open("/badusb");
        if (dir && dir.isDirectory()) {
            File f = dir.openNextFile();
            int count = 0;
            while (f && count < 32) {
                if (!f.isDirectory()) {
                    const char *fname = f.name();
                    const char *slash = strrchr(fname, '/');
                    if (slash) fname = slash + 1;

                    String name = fname;
                    if (name.endsWith(".txt") || name.endsWith(".ducky") || name.endsWith(".duck")) {
                        arr.add(name);
                        count++;
                    }
                }
                f = dir.openNextFile();
            }
            dir.close();
        }
    }

    char *buf = (char*)malloc(1024);
    serializeJson(doc, buf, 1024);
    return buf;
}

static char* cmd_help(void) {
    JsonDocument doc;
    doc["type"] = "help";
    JsonObject cats = doc["categories"].to<JsonObject>();

    JsonArray sys = cats["system"].to<JsonArray>();
    sys.add("status"); sys.add("version"); sys.add("brightness"); sys.add("haptic");
    sys.add("watchface"); sys.add("reboot"); sys.add("compass"); sys.add("sensor_data");

    JsonArray nfc = cats["nfc"].to<JsonArray>();
    nfc.add("nfc_scan"); nfc.add("nfc_stop"); nfc.add("nfc_save"); nfc.add("nfc_list");
    nfc.add("nfc_export"); nfc.add("nfc_delete"); nfc.add("nfc_download");

    JsonArray lora = cats["lora"].to<JsonArray>();
    lora.add("lora_start"); lora.add("lora_stop"); lora.add("lora_send"); lora.add("lora_advert");
    lora.add("lora_history");

    JsonArray recon = cats["recon"].to<JsonArray>();
    recon.add("recon_wifi"); recon.add("recon_ble"); recon.add("recon_stop"); recon.add("recon_deauth");
    recon.add("recon_results"); recon.add("deauth_all"); recon.add("sniffer_start"); recon.add("sniffer_stop");
    recon.add("deauth_detect"); recon.add("evil_twin"); recon.add("evil_twin_stop");

    JsonArray rf = cats["rf"].to<JsonArray>();
    rf.add("rf_jammer_start"); rf.add("rf_jammer_stop"); rf.add("rf_tesla_send"); rf.add("rf_status");

    JsonArray hid = cats["hid"].to<JsonArray>();
    hid.add("hid_start"); hid.add("hid_stop"); hid.add("hid_set_layout"); hid.add("hid_list_scripts");
    hid.add("hid_run_script"); hid.add("hid_run_instant"); hid.add("hid_abort_script"); hid.add("hid_status");

    JsonArray pet = cats["pet"].to<JsonArray>();
    pet.add("pet_feed"); pet.add("pet_heal"); pet.add("pet_clean"); pet.add("pet_status");

    char *buf = (char*)malloc(1536);
    serializeJson(doc, buf, 1536);
    return buf;
}

char* api_handle_command(const char *json_cmd) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_cmd);
    if (err) return strdup("{\"error\":\"invalid json\"}");

    const char *cmd = doc["cmd"] | "";

    if (strcmp(cmd, "help") == 0)       return cmd_help();
    if (strcmp(cmd, "status") == 0)     return cmd_status();
    if (strcmp(cmd, "version") == 0)    return cmd_version();
    if (strcmp(cmd, "brightness") == 0) return cmd_brightness(doc["params"]["v"] | 128);
    if (strcmp(cmd, "haptic") == 0)     return cmd_haptic();
    if (strcmp(cmd, "watchface") == 0)  return cmd_watchface(doc["params"]["style"] | "next");
    if (strcmp(cmd, "reboot") == 0)     return cmd_reboot();
    if (strcmp(cmd, "gps_on") == 0)     return cmd_gps_on();
    if (strcmp(cmd, "gps_off") == 0)    return cmd_gps_off();
    if (strcmp(cmd, "gps_status") == 0) return cmd_gps_status();
    if (strcmp(cmd, "gps_wardriving_start") == 0) { gps_app_set_wardriving(true); return strdup("{\"ok\":true,\"msg\":\"wardriving started\"}"); }
    if (strcmp(cmd, "gps_wardriving_stop") == 0)  { gps_app_set_wardriving(false); return strdup("{\"ok\":true,\"msg\":\"wardriving stopped\"}"); }
    if (strcmp(cmd, "compass") == 0)    return cmd_compass();
    if (strcmp(cmd, "sensor_data") == 0) return cmd_sensor_data();

    if (strcmp(cmd, "nfc_scan") == 0)   return cmd_nfc_scan();
    if (strcmp(cmd, "nfc_stop") == 0)   return cmd_nfc_stop();
    if (strcmp(cmd, "nfc_save") == 0)   return cmd_nfc_save();
    if (strcmp(cmd, "nfc_list") == 0)   return cmd_nfc_list();
    if (strcmp(cmd, "nfc_export") == 0) return cmd_nfc_export();
    if (strcmp(cmd, "nfc_delete") == 0) return cmd_nfc_delete(doc["params"]["idx"] | 0);
    if (strcmp(cmd, "nfc_download") == 0) return cmd_nfc_download(doc["params"]["idx"] | 0);
    if (strcmp(cmd, "nfc_emulate") == 0) return cmd_nfc_emulate();
    if (strcmp(cmd, "nfc_select_next") == 0) return cmd_nfc_select_next();
    if (strcmp(cmd, "nfc_status") == 0) return cmd_nfc_status();

    if (strcmp(cmd, "lora_start") == 0) {
        int mode = doc["params"]["mode"] | 0;
        if (doc["params"].containsKey("freq")) {
            float freq = doc["params"]["freq"] | 868.0f;
            lora_svc_set_freq(freq);
        }
        if (doc["params"].containsKey("ric")) {
            uint32_t ric = doc["params"]["ric"] | 1234567;
            lora_svc_set_ric(ric);
        }
        return cmd_lora_start(mode);
    }
    if (strcmp(cmd, "lora_stop") == 0)    return cmd_lora_stop();
    if (strcmp(cmd, "lora_send") == 0)    return cmd_lora_send(doc["params"]["text"] | "");
    if (strcmp(cmd, "lora_advert") == 0)  return cmd_lora_advert();
    if (strcmp(cmd, "lora_history") == 0) return cmd_lora_history();
    if (strcmp(cmd, "lora_set_ric") == 0) return cmd_lora_set_ric(doc["params"]["ric"] | 1234567);
    if (strcmp(cmd, "lora_set_freq") == 0) return cmd_lora_set_freq(doc["params"]["freq"] | 868.0f);
    if (strcmp(cmd, "lora_set_name") == 0) return cmd_lora_set_name(doc["params"]["name"] | "");

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
    if (strcmp(cmd, "recon_arp_start") == 0) return cmd_recon_arp_start();
    if (strcmp(cmd, "recon_arp_results") == 0) return cmd_recon_arp_results();
    if (strcmp(cmd, "recon_ip_sniff") == 0) return cmd_recon_ip_sniff(doc["params"]["ip"] | "");
    if (strcmp(cmd, "recon_ip_sniff_results") == 0) return cmd_recon_ip_sniff_results();
    if (strcmp(cmd, "recon_beacon_spam") == 0) return cmd_recon_beacon_spam(doc["params"]["ssids"].as<JsonArray>());
    if (strcmp(cmd, "recon_adsb_start") == 0) return cmd_recon_adsb_start(doc["params"]["lat"] | 0.0, doc["params"]["lon"] | 0.0, doc["params"]["name"] | "");
    if (strcmp(cmd, "recon_adsb_status") == 0) return cmd_recon_adsb_status();

    if (strcmp(cmd, "rf_jammer_start") == 0) {
        uint32_t freq = doc["params"]["freq"] | 433920000;
        int duration = doc["params"]["duration"] | 0;
        rf_jammer_start(freq);
        if (duration > 0) {
            jammer_has_timeout = true;
            jammer_end_time = millis() + (duration * 1000);
        } else {
            jammer_has_timeout = false;
        }
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
    if (strcmp(cmd, "hid_set_layout") == 0) {
        const char *layout_str = doc["params"]["layout"] | "US";
        KeyboardLayoutId layout = KB_LAYOUT_US;
        if (strcmp(layout_str, "US") == 0) layout = KB_LAYOUT_US;
        else if (strcmp(layout_str, "DK") == 0) layout = KB_LAYOUT_DK;
        else if (strcmp(layout_str, "UK") == 0) layout = KB_LAYOUT_UK;
        else if (strcmp(layout_str, "FR") == 0) layout = KB_LAYOUT_FR;
        else if (strcmp(layout_str, "DE") == 0) layout = KB_LAYOUT_DE;
        else if (strcmp(layout_str, "HU") == 0) layout = KB_LAYOUT_HU;
        else if (strcmp(layout_str, "IT") == 0) layout = KB_LAYOUT_IT;
        else if (strcmp(layout_str, "BR") == 0) layout = KB_LAYOUT_BR;
        else if (strcmp(layout_str, "PT") == 0) layout = KB_LAYOUT_PT;
        else if (strcmp(layout_str, "SI") == 0) layout = KB_LAYOUT_SI;
        else if (strcmp(layout_str, "ES") == 0) layout = KB_LAYOUT_ES;
        else if (strcmp(layout_str, "SV") == 0) layout = KB_LAYOUT_SV;
        else if (strcmp(layout_str, "TR") == 0) layout = KB_LAYOUT_TR;
        hid_svc_set_layout(layout);
        return strdup("{\"ok\":true,\"msg\":\"layout updated\"}");
    }
    if (strcmp(cmd, "hid_list_scripts") == 0) {
        return cmd_hid_list_scripts();
    }
    if (strcmp(cmd, "hid_run_script") == 0) {
        const char *path = doc["params"]["path"] | "";
        bool ble = doc["params"]["ble"] | false;

        char full_path[128];
        if (path[0] == '/') {
            strncpy(full_path, path, sizeof(full_path) - 1);
        } else {
            snprintf(full_path, sizeof(full_path), "/badusb/%s", path);
        }
        full_path[sizeof(full_path) - 1] = 0;

        if (!SD.exists(full_path)) {
            return strdup("{\"error\":\"script file not found on SD\"}");
        }

        if (doc["params"].containsKey("layout")) {
            const char *layout_str = doc["params"]["layout"] | "US";
            KeyboardLayoutId layout = KB_LAYOUT_US;
            if (strcmp(layout_str, "US") == 0) layout = KB_LAYOUT_US;
            else if (strcmp(layout_str, "DK") == 0) layout = KB_LAYOUT_DK;
            else if (strcmp(layout_str, "UK") == 0) layout = KB_LAYOUT_UK;
            else if (strcmp(layout_str, "FR") == 0) layout = KB_LAYOUT_FR;
            else if (strcmp(layout_str, "DE") == 0) layout = KB_LAYOUT_DE;
            else if (strcmp(layout_str, "HU") == 0) layout = KB_LAYOUT_HU;
            else if (strcmp(layout_str, "IT") == 0) layout = KB_LAYOUT_IT;
            else if (strcmp(layout_str, "BR") == 0) layout = KB_LAYOUT_BR;
            else if (strcmp(layout_str, "PT") == 0) layout = KB_LAYOUT_PT;
            else if (strcmp(layout_str, "SI") == 0) layout = KB_LAYOUT_SI;
            else if (strcmp(layout_str, "ES") == 0) layout = KB_LAYOUT_ES;
            else if (strcmp(layout_str, "SV") == 0) layout = KB_LAYOUT_SV;
            else if (strcmp(layout_str, "TR") == 0) layout = KB_LAYOUT_TR;
            hid_svc_set_layout(layout);
        }

        hid_svc_run_script(full_path, ble);
        return strdup("{\"ok\":true,\"msg\":\"script started\"}");
    }
    if (strcmp(cmd, "hid_run_instant") == 0) {
        const char *script_content = doc["params"]["script"] | "";
        bool ble = doc["params"]["ble"] | false;
        if (!SD.exists("/badusb")) {
            SD.mkdir("/badusb");
        }

        File f = SD.open("/badusb/temp_api.txt", FILE_WRITE);
        if (!f) {
            return strdup("{\"error\":\"failed to write temp script to SD\"}");
        }
        f.print(script_content);
        f.close();

        if (doc["params"].containsKey("layout")) {
            const char *layout_str = doc["params"]["layout"] | "US";
            KeyboardLayoutId layout = KB_LAYOUT_US;
            if (strcmp(layout_str, "US") == 0) layout = KB_LAYOUT_US;
            else if (strcmp(layout_str, "DK") == 0) layout = KB_LAYOUT_DK;
            else if (strcmp(layout_str, "UK") == 0) layout = KB_LAYOUT_UK;
            else if (strcmp(layout_str, "FR") == 0) layout = KB_LAYOUT_FR;
            else if (strcmp(layout_str, "DE") == 0) layout = KB_LAYOUT_DE;
            else if (strcmp(layout_str, "HU") == 0) layout = KB_LAYOUT_HU;
            else if (strcmp(layout_str, "IT") == 0) layout = KB_LAYOUT_IT;
            else if (strcmp(layout_str, "BR") == 0) layout = KB_LAYOUT_BR;
            else if (strcmp(layout_str, "PT") == 0) layout = KB_LAYOUT_PT;
            else if (strcmp(layout_str, "SI") == 0) layout = KB_LAYOUT_SI;
            else if (strcmp(layout_str, "ES") == 0) layout = KB_LAYOUT_ES;
            else if (strcmp(layout_str, "SV") == 0) layout = KB_LAYOUT_SV;
            else if (strcmp(layout_str, "TR") == 0) layout = KB_LAYOUT_TR;
            hid_svc_set_layout(layout);
        }

        hid_svc_run_script("/badusb/temp_api.txt", ble);
        return strdup("{\"ok\":true,\"msg\":\"instant script started\"}");
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
        hdoc["layout"] = hid_svc_get_layout_name(hid_svc_get_layout());
        char *buf = (char*)malloc(256);
        serializeJson(hdoc, buf, 256);
        return buf;
    }

    if (strcmp(cmd, "hid_save_script") == 0) {
        const char *name = doc["params"]["name"] | "";
        const char *content = doc["params"]["content"] | "";
        if (strlen(name) == 0) return strdup("{\"error\":\"empty name\"}");
        
        char full_path[128];
        if (name[0] == '/') {
            strncpy(full_path, name, sizeof(full_path) - 1);
        } else {
            snprintf(full_path, sizeof(full_path), "/badusb/%s", name);
        }
        full_path[sizeof(full_path) - 1] = 0;
        
        if (!SD.exists("/badusb")) {
            SD.mkdir("/badusb");
        }
        
        File f = SD.open(full_path, FILE_WRITE);
        if (!f) return strdup("{\"error\":\"failed to write to SD\"}");
        f.print(content);
        f.close();
        return strdup("{\"ok\":true,\"msg\":\"saved to SD\"}");
    }

    if (strcmp(cmd, "hid_read_script") == 0) {
        const char *name = doc["params"]["name"] | "";
        if (strlen(name) == 0) return strdup("{\"error\":\"empty name\"}");
        
        char full_path[128];
        if (name[0] == '/') {
            strncpy(full_path, name, sizeof(full_path) - 1);
        } else {
            snprintf(full_path, sizeof(full_path), "/badusb/%s", name);
        }
        full_path[sizeof(full_path) - 1] = 0;
        
        if (!SD.exists(full_path)) return strdup("{\"error\":\"file not found\"}");
        
        File f = SD.open(full_path, FILE_READ);
        if (!f) return strdup("{\"error\":\"failed to read file\"}");
        
        size_t fsize = f.size();
        if (fsize > 8192) {
            f.close();
            return strdup("{\"error\":\"file too large\"}");
        }
        
        char *content = (char*)malloc(fsize + 1);
        f.readBytes(content, fsize);
        content[fsize] = 0;
        f.close();
        
        JsonDocument rdoc;
        rdoc["ok"] = true;
        rdoc["content"] = content;
        free(content);
        
        char *buf = (char*)malloc(fsize + 128);
        serializeJson(rdoc, buf, fsize + 128);
        return buf;
    }

    if (strcmp(cmd, "hid_airmouse_start") == 0) return cmd_hid_airmouse_start();
    if (strcmp(cmd, "hid_airmouse_stop") == 0) return cmd_hid_airmouse_stop();
    if (strcmp(cmd, "hid_airmouse_calibrate") == 0) return cmd_hid_airmouse_calibrate();
    if (strcmp(cmd, "hid_media") == 0) return cmd_hid_media(doc["params"]["action"] | "");
    if (strcmp(cmd, "hid_mouse_click") == 0) return cmd_hid_mouse_click(doc["params"]["buttons"] | 1);
    if (strcmp(cmd, "hid_mouse_scroll") == 0) return cmd_hid_mouse_scroll(doc["params"]["wheel"] | 0);

    if (strcmp(cmd, "sd_list") == 0) return cmd_sd_list(doc["params"]["path"] | "/");
    if (strcmp(cmd, "sd_mkdir") == 0) return cmd_sd_mkdir(doc["params"]["path"] | "");
    if (strcmp(cmd, "sd_delete") == 0) return cmd_sd_delete(doc["params"]["path"] | "");
    if (strcmp(cmd, "sd_rename") == 0) return cmd_sd_rename(doc["params"]["old_path"] | "", doc["params"]["new_path"] | "");
    if (strcmp(cmd, "sd_copy") == 0) return cmd_sd_copy(doc["params"]["src"] | "", doc["params"]["dest"] | "");
    if (strcmp(cmd, "sd_read") == 0) return cmd_sd_read(doc["params"]["path"] | "");
    if (strcmp(cmd, "sd_write") == 0) return cmd_sd_write(doc["params"]["path"] | "", doc["params"]["content"] | "");

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
    if (jammer_has_timeout && rf_jammer_is_active()) {
        if (millis() >= jammer_end_time) {
            rf_jammer_stop();
            jammer_has_timeout = false;
            push_event("{\"event\":\"rf_jammer_stopped\"}");
        }
    }

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
