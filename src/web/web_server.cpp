#include "web_server.h"
#include "web_ui.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LilyGoLib.h>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/time_sync.h"
#include "../hal/power_hal.h"
#include "../hal/nfc_service.h"
#include "../hal/lora_service.h"
#include "../hal/recon_service.h"
#include "../api/command_handler.h"
#include "../ui/watchface.h"
#include "../ui/action_overlay.h"
#include "../hal/hid_service.h"
#include "../apps/gps_app.h"
#include "../hal/rf_service.h"
#include <SD.h>
#include <FS.h>
#include <NimBLEDevice.h>
#include "../hal/ble_uart_service.h"

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static bool active = false;
static char ap_ip[20] = "";

static bool svc_wifi = false;
static bool svc_bt = false;
static bool svc_gps = false;
static bool svc_nfc = false;

static volatile bool show_overlay_scan = false;
static volatile bool show_overlay_emit = false;
static volatile bool hide_overlay_flag = false;

static volatile bool show_overlay_exfil = false;
static char exfil_overlay_title[32] = "";
static char exfil_overlay_status[64] = "";
static volatile bool exfil_overlay_finished = false;

static volatile bool flag_haptic = false;
static volatile bool flag_brightness = false;
static volatile int  pending_brightness = 128;
static volatile bool flag_ntp_retry = false;
static volatile bool flag_watchface_next = false;
static volatile bool flag_watchface_prev = false;
static volatile bool flag_wifi_scan = false;
static volatile bool flag_service_change = false;
static char pending_svc[16] = "";
static volatile bool pending_svc_en = false;
static volatile bool flag_reboot = false;
static volatile bool flag_lora_start = false;
static volatile bool flag_lora_stop = false;
static volatile bool flag_lora_advert = false;
static volatile bool flag_lora_send = false;
static char pending_lora_text[240] = "";

struct CachedWiFi {
    char ssid[33];
    char bssid[18];
    int8_t rssi;
    uint8_t ch;
};
static CachedWiFi scan_cache[30];
static int scan_cache_count = 0;
static uint32_t scan_cache_ts = 0;

static void ws_broadcast(const char *json) {
    ws.textAll(json);
}

static uint32_t last_status_push = 0;

static void push_status(void) {
    if (millis() - last_status_push < 1000) return;
    last_status_push = millis();

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
    doc["ntp"] = time_sync_is_synced();
    doc["heap"] = ESP.getFreeHeap() / 1024;
    doc["lora"] = lora_svc_is_running();
    doc["nfc"] = nfc_svc_is_scanning();
    doc["hid_active"] = hid_svc_is_active();
    doc["hid_conn"] = hid_svc_is_connected();
    doc["hid_usb"] = hid_svc_is_usb_connected();
    doc["hid_running"] = hid_svc_is_running_script();
    doc["hid_layout"] = hid_svc_get_layout_name(hid_svc_get_layout());

    if (instance.gps.location.isValid()) {
        char g[32]; snprintf(g, sizeof(g), "%.4f,%.4f", instance.gps.location.lat(), instance.gps.location.lng());
        doc["gps"] = g;
    } else if (gps_app_is_enabled() || instance.gps.charsProcessed() > 10) {
        doc["gps"] = "NO FIX";
    } else {
        doc["gps"] = "OFF";
    }

    uint32_t s = millis() / 1000;
    char up[16]; snprintf(up, sizeof(up), "%dh%02dm", s/3600, (s%3600)/60);
    doc["uptime"] = up;

    char buf[512];
    serializeJson(doc, buf, sizeof(buf));
    ws_broadcast(buf);
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WEB] WS client #%u connected\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WEB] WS client #%u disconnected\n", client->id());
    }
}

static void setup_routes(void) {

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html", WEB_INDEX_HTML);
    });

    server.on("/api/haptic", HTTP_POST, [](AsyncWebServerRequest *req) {
        flag_haptic = true;
        req->send(200, "application/json", "{\"msg\":\"Haptic OK\"}");
    });

    server.on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (req->hasParam("v", true)) {
            pending_brightness = req->getParam("v", true)->value().toInt();
            flag_brightness = true;
        }
        req->send(200, "application/json", "{\"msg\":\"OK\"}");
    });

    server.on("/api/ntp", HTTP_POST, [](AsyncWebServerRequest *req) {
        flag_ntp_retry = true;
        req->send(200, "application/json", "{\"msg\":\"NTP sync requested\"}");
    });

    server.on("/api/watchface", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (req->hasParam("style", true)) {
            String s = req->getParam("style", true)->value();
            if (s == "next") flag_watchface_next = true;
            else if (s == "prev") flag_watchface_prev = true;
        }
        req->send(200, "application/json", "{\"msg\":\"OK\"}");
    });

    server.on("/api/service", HTTP_POST, [](AsyncWebServerRequest *req) {
        String svc = req->hasParam("service", true) ? req->getParam("service", true)->value() : "";
        bool en = req->hasParam("enable", true) ? req->getParam("enable", true)->value() == "true" : false;
        strncpy(pending_svc, svc.c_str(), sizeof(pending_svc)-1);
        pending_svc[sizeof(pending_svc)-1] = '\0';
        pending_svc_en = en;
        flag_service_change = true;
        char r[64]; snprintf(r, sizeof(r), "{\"msg\":\"%s %s\"}", svc.c_str(), en?"ON":"OFF");
        req->send(200, "application/json", r);
    });

    server.on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest *req) {
        flag_wifi_scan = true;
        req->send(200, "application/json", "{\"msg\":\"Scan started - AP will restart in ~5s\"}");
    });

    server.on("/api/wifi/scan/last", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["ts"] = scan_cache_ts;
        doc["age_ms"] = (int)(millis() - scan_cache_ts);
        JsonArray nets = doc["networks"].to<JsonArray>();
        for (int i = 0; i < scan_cache_count; i++) {
            JsonObject o = nets.add<JsonObject>();
            o["ssid"] = scan_cache[i].ssid;
            o["bssid"] = scan_cache[i].bssid;
            o["rssi"] = scan_cache[i].rssi;
            o["ch"] = scan_cache[i].ch;
        }
        char buf[2048];
        serializeJson(doc, buf, sizeof(buf));
        req->send(200, "application/json", buf);
    });

    server.on("/api/nfc/scan", HTTP_POST, [](AsyncWebServerRequest *req) {
        nfc_svc_request_scan();
        show_overlay_scan = true;
        req->send(200, "application/json", "{\"msg\":\"NFC scanning\"}");
    });
    server.on("/api/nfc/stop", HTTP_POST, [](AsyncWebServerRequest *req) {
        nfc_svc_request_stop();
        hide_overlay_flag = true;
        req->send(200, "application/json", "{\"msg\":\"NFC stopped\"}");
    });
    server.on("/api/nfc/save", HTTP_POST, [](AsyncWebServerRequest *req) {
        nfc_svc_request_save();
        req->send(200, "application/json", "{\"msg\":\"Tag saved\"}");
    });
    server.on("/api/nfc/export", HTTP_POST, [](AsyncWebServerRequest *req) {
        nfc_svc_request_export();
        req->send(200, "application/json", "{\"msg\":\"Exported\"}");
    });
    server.on("/api/nfc/delete", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (req->hasParam("i", true)) {
            nfc_svc_request_delete(req->getParam("i", true)->value().toInt());
        }
        req->send(200, "application/json", "{\"msg\":\"Deleted\"}");
    });

    server.on("/api/recon/wifi", HTTP_POST, [](AsyncWebServerRequest *req) {
        recon_request_wifi_scan();
        req->send(200, "application/json", "{\"msg\":\"WiFi scan started\"}");
    });
    server.on("/api/recon/ble", HTTP_POST, [](AsyncWebServerRequest *req) {
        int dur = req->hasParam("duration", true) ? req->getParam("duration", true)->value().toInt() : 10;
        recon_request_ble_scan(dur);
        req->send(200, "application/json", "{\"msg\":\"BLE scan started\"}");
    });
    server.on("/api/recon/deauth", HTTP_POST, [](AsyncWebServerRequest *req) {
        String bssid = req->hasParam("bssid", true) ? req->getParam("bssid", true)->value() : "";
        int ch = req->hasParam("ch", true) ? req->getParam("ch", true)->value().toInt() : 0;
        if (bssid.length() > 0 && ch > 0) {
            recon_request_deauth(bssid.c_str(), ch);
        }
        req->send(200, "application/json", "{\"msg\":\"Deauth started\"}");
    });
    server.on("/api/recon/stop", HTTP_POST, [](AsyncWebServerRequest *req) {
        recon_request_stop();
        req->send(200, "application/json", "{\"msg\":\"Stopped\"}");
    });
    server.on("/api/recon/blackout", HTTP_POST, [](AsyncWebServerRequest *req) {
        recon_request_deauth_all();
        req->send(200, "application/json", "{\"msg\":\"Blackout started\"}");
    });
    server.on("/api/recon/sniffer", HTTP_POST, [](AsyncWebServerRequest *req) {
        int ch = req->hasParam("ch", true) ? req->getParam("ch", true)->value().toInt() : 0;
        recon_request_sniffer(ch);
        req->send(200, "application/json", "{\"msg\":\"Sniffer started\"}");
    });
    server.on("/api/recon/detect", HTTP_POST, [](AsyncWebServerRequest *req) {
        recon_request_deauth_detect();
        req->send(200, "application/json", "{\"msg\":\"Deauth detector started\"}");
    });
    server.on("/api/recon/evil_twin", HTTP_POST, [](AsyncWebServerRequest *req) {
        String ssid = req->hasParam("ssid", true) ? req->getParam("ssid", true)->value() : "FreeWiFi";
        int ch = req->hasParam("ch", true) ? req->getParam("ch", true)->value().toInt() : 6;
        recon_request_evil_twin(ssid.c_str(), ch);
        req->send(200, "application/json", "{\"msg\":\"Evil Twin started\"}");
    });
    server.on("/api/recon/results", HTTP_GET, [](AsyncWebServerRequest *req) {
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
        char buf[4096];
        serializeJson(doc, buf, sizeof(buf));
        req->send(200, "application/json", buf);
    });

    server.on("/api/cmd", HTTP_POST, [](AsyncWebServerRequest *req) {
        String json = req->hasParam("cmd", true) ? req->getParam("cmd", true)->value() : "{}";
        char *resp = api_handle_command(json.c_str());
        req->send(200, "application/json", resp ? resp : "{\"error\":\"null\"}");
        if (resp) free(resp);
    });

    
    server.on("/api/rf/jammer/start", HTTP_POST, [](AsyncWebServerRequest *req) {
        uint32_t freq = 433920000;
        if (req->hasParam("freq", true)) {
            freq = req->getParam("freq", true)->value().toInt();
        }
        rf_jammer_start(freq);
        req->send(200, "application/json", "{\"msg\":\"Jammer started\"}");
    });
    server.on("/api/rf/jammer/stop", HTTP_POST, [](AsyncWebServerRequest *req) {
        rf_jammer_stop();
        req->send(200, "application/json", "{\"msg\":\"Jammer stopped\"}");
    });
    server.on("/api/rf/tesla", HTTP_POST, [](AsyncWebServerRequest *req) {
        rf_tesla_send();
        req->send(200, "application/json", "{\"msg\":\"Tesla command sent\"}");
    });
    server.on("/api/rf/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["jammer"] = rf_jammer_is_active();
        doc["freq"] = rf_jammer_get_freq();
        doc["tesla"] = rf_tesla_is_sending();
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    
    server.on("/api/sd/list", HTTP_GET, [](AsyncWebServerRequest *req) {
        String path = "/";
        if (req->hasParam("path")) {
            path = req->getParam("path")->value();
        }
        if (!path.startsWith("/")) path = "/" + path;
        
        File root = SD.open(path);
        if (!root || !root.isDirectory()) {
            req->send(404, "application/json", "{\"error\":\"Directory not found\"}");
            return;
        }

        JsonDocument doc;
        doc["path"] = path;
        
        String parent = "/";
        if (path != "/") {
            int idx = path.lastIndexOf('/');
            if (idx > 0) {
                parent = path.substring(0, idx);
            } else {
                parent = "/";
            }
        }
        doc["parent"] = parent;

        JsonArray arr = doc["items"].to<JsonArray>();
        File file = root.openNextFile();
        while (file) {
            JsonObject item = arr.add<JsonObject>();
            item["name"] = String(file.name());
            item["isDir"] = file.isDirectory();
            if (!file.isDirectory()) {
                item["size"] = file.size();
            }
            file = root.openNextFile();
        }
        root.close();

        String response;
        serializeJson(doc, response);
        req->send(200, "application/json", response);
    });

    server.on("/api/sd/mkdir", HTTP_POST, [](AsyncWebServerRequest *req) {
        String path = "/";
        String name = "";
        if (req->hasParam("path", true)) path = req->getParam("path", true)->value();
        if (req->hasParam("name", true)) name = req->getParam("name", true)->value();

        if (name.length() == 0) {
            req->send(400, "application/json", "{\"error\":\"Invalid folder name\"}");
            return;
        }

        if (!path.endsWith("/")) path += "/";
        String fullPath = path + name;
        if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;

        if (SD.mkdir(fullPath)) {
            req->send(200, "application/json", "{\"ok\":true}");
        } else {
            req->send(500, "application/json", "{\"error\":\"mkdir failed\"}");
        }
    });

    server.on("/api/sd/delete", HTTP_POST, [](AsyncWebServerRequest *req) {
        String path = "";
        if (req->hasParam("path", true)) path = req->getParam("path", true)->value();

        if (path.length() == 0 || path == "/") {
            req->send(400, "application/json", "{\"error\":\"Invalid path\"}");
            return;
        }

        if (!path.startsWith("/")) path = "/" + path;

        bool success = false;
        File f = SD.open(path);
        if (f) {
            bool isDir = f.isDirectory();
            f.close();
            if (isDir) {
                success = SD.rmdir(path);
            } else {
                success = SD.remove(path);
            }
        } else {
            success = SD.remove(path) || SD.rmdir(path);
        }

        if (success) {
            req->send(200, "application/json", "{\"ok\":true}");
        } else {
            req->send(500, "application/json", "{\"error\":\"delete failed\"}");
        }
    });

    server.on("/api/sd/rename", HTTP_POST, [](AsyncWebServerRequest *req) {
        String path = "";
        String newPath = "";
        if (req->hasParam("path", true)) path = req->getParam("path", true)->value();
        if (req->hasParam("newPath", true)) newPath = req->getParam("newPath", true)->value();

        if (path.length() == 0 || newPath.length() == 0) {
            req->send(400, "application/json", "{\"error\":\"Invalid arguments\"}");
            return;
        }

        if (!path.startsWith("/")) path = "/" + path;
        if (!newPath.startsWith("/")) newPath = "/" + newPath;

        if (SD.rename(path, newPath)) {
            req->send(200, "application/json", "{\"ok\":true}");
        } else {
            req->send(500, "application/json", "{\"error\":\"rename failed\"}");
        }
    });

    server.on("/api/sd/download", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!req->hasParam("path")) {
            req->send(400, "text/plain", "Missing path");
            return;
        }
        String path = req->getParam("path")->value();
        if (!path.startsWith("/")) path = "/" + path;

        if (!SD.exists(path)) {
            req->send(404, "text/plain", "File not found");
            return;
        }
        
        
        snprintf(exfil_overlay_title, sizeof(exfil_overlay_title), "GET: %s", path.substring(path.lastIndexOf('/') + 1).c_str());
        snprintf(exfil_overlay_status, sizeof(exfil_overlay_status), "Downloading...");
        show_overlay_exfil = true;
        exfil_overlay_finished = true;
        
        AsyncWebServerResponse *response = req->beginResponse(SD, path, "application/octet-stream", true);
        req->send(response);
    });

    server.on("/api/sd/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->_tempObject == (void*)1) {
            request->send(500, "application/json", "{\"error\":\"Upload failed\"}");
        } else {
            request->send(200, "application/json", "{\"ok\":true}");
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        File *uploadFile = (File *)request->_tempObject;
        if(!index){
            String path = "/";
            if(request->hasParam("path")) {
                path = request->getParam("path")->value();
            }
            if(!path.endsWith("/")) path += "/";
            String fullPath = path + filename;
            if(!fullPath.startsWith("/")) fullPath = "/" + fullPath;
            
            
            String dirPath = path;
            if (dirPath.endsWith("/")) {
                dirPath = dirPath.substring(0, dirPath.length() - 1);
            }
            if (dirPath.length() > 0 && !SD.exists(dirPath)) {
                SD.mkdir(dirPath);
            }

            if (SD.exists(fullPath)) {
                SD.remove(fullPath);
            }
            
            uploadFile = new File(SD.open(fullPath, FILE_WRITE));
            if (!uploadFile || !(*uploadFile)) {
                if (uploadFile) {
                    delete uploadFile;
                }
                request->_tempObject = (void*)1;
                uploadFile = nullptr;
            } else {
                request->_tempObject = (void*)uploadFile;
            }
            
            
            snprintf(exfil_overlay_title, sizeof(exfil_overlay_title), "UPLOAD: %s", filename.c_str());
            snprintf(exfil_overlay_status, sizeof(exfil_overlay_status), "Uploading...");
            show_overlay_exfil = true;
            exfil_overlay_finished = false;
        }
        
        if (uploadFile && *uploadFile && request->_tempObject != (void*)1) {
            if (uploadFile->write(data, len) != len) {
                request->_tempObject = (void*)1;
            }
        }
        
        if(final){
            if (uploadFile) {
                if (*uploadFile) {
                    uploadFile->close();
                }
                delete uploadFile;
            }
            
            if (request->_tempObject != (void*)1) {
                request->_tempObject = nullptr;
                snprintf(exfil_overlay_status, sizeof(exfil_overlay_status), "Done 100%%");
            } else {
                snprintf(exfil_overlay_status, sizeof(exfil_overlay_status), "Failed");
            }
            show_overlay_exfil = true;
            exfil_overlay_finished = true;
        }
    });

    server.on("/exfil", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "OK");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        File *exfilFile = (File *)request->_tempObject;
        if (!index) {
            if (!SD.exists("/exfil")) {
                SD.mkdir("/exfil");
            }
            
            String filename = "/exfil/data_" + String(millis()) + ".txt";
            exfilFile = new File(SD.open(filename, FILE_WRITE));
            request->_tempObject = (void *)exfilFile;
            
            snprintf(exfil_overlay_title, sizeof(exfil_overlay_title), "POST EXFIL");
            snprintf(exfil_overlay_status, sizeof(exfil_overlay_status), "Receiving: 0%%");
            show_overlay_exfil = true;
            exfil_overlay_finished = false;
        }
        
        if (exfilFile && *exfilFile) {
            exfilFile->write(data, len);
        }
        
        if (total > 0) {
            int progress = (int)((float)(index + len) / total * 100.0f);
            snprintf(exfil_overlay_status, sizeof(exfil_overlay_status), "Receiving: %d%%", progress);
            show_overlay_exfil = true; 
        }
        
        if (index + len == total) {
            if (exfilFile) {
                exfilFile->close();
                delete exfilFile;
                request->_tempObject = nullptr;
            }
            snprintf(exfil_overlay_status, sizeof(exfil_overlay_status), "Done 100%%");
            show_overlay_exfil = true;
            exfil_overlay_finished = true;
        }
    });

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *req) {
        flag_reboot = true;
        req->send(200, "application/json", "{\"msg\":\"Rebooting...\"}");
    });

    server.on("/api/lora/start", HTTP_POST, [](AsyncWebServerRequest *req) {
        flag_lora_start = true;
        req->send(200, "application/json", "{\"msg\":\"MeshCore started\"}");
    });
    server.on("/api/lora/stop", HTTP_POST, [](AsyncWebServerRequest *req) {
        flag_lora_stop = true;
        req->send(200, "application/json", "{\"msg\":\"MeshCore stopped\"}");
    });
    server.on("/api/lora/send", HTTP_POST, [](AsyncWebServerRequest *req) {
        String txt = req->hasParam("text", true) ? req->getParam("text", true)->value() : "";
        if (txt.length() > 0) {
            strncpy(pending_lora_text, txt.c_str(), sizeof(pending_lora_text)-1);
            pending_lora_text[sizeof(pending_lora_text)-1] = '\0';
            flag_lora_send = true;
        }
        req->send(200, "application/json", "{\"ok\":true}");
    });
    server.on("/api/lora/advert", HTTP_POST, [](AsyncWebServerRequest *req) {
        flag_lora_advert = true;
        req->send(200, "application/json", "{\"msg\":\"Advert sent\"}");
    });

    server.on("/api/lora/history", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc["messages"].to<JsonArray>();
        int count = lora_svc_message_count();
        for (int i = 0; i < count && i < 20; i++) {
            const MeshMsg *m = lora_svc_get_message(i);
            if (!m) continue;
            JsonObject obj = arr.add<JsonObject>();
            obj["ch"] = m->channel;
            obj["text"] = m->text;
            obj["hops"] = m->hops;
            obj["rssi"] = (int)m->rssi;
            obj["ts"] = m->timestamp;
        }
        char buf[2048];
        serializeJson(doc, buf, sizeof(buf));
        req->send(200, "application/json", buf);
    });
}

void web_server_init(void) {
    if (active) return;

    WiFi.mode(WIFI_AP_STA);
    IPAddress defaultIP(192, 168, 4, 1);
    WiFi.softAPConfig(defaultIP, defaultIP, IPAddress(255, 255, 255, 0));
    for (int attempt = 0; attempt < 3; attempt++) {
        if (WiFi.softAP("SCR Terminal", "pip12345")) break;
        Serial.printf("[WEB] AP attempt %d failed, retrying...\n", attempt);
        WiFi.softAPdisconnect(true);
        delay(500);
    }
    delay(200);
    IPAddress ip = WiFi.softAPIP();
    snprintf(ap_ip, sizeof(ap_ip), "%s", ip.toString().c_str());
    Serial.printf("[WEB] AP started: SCR Terminal, IP: %s\n", ap_ip);

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    setup_routes();

    server.begin();
    active = true;
    Serial.println("[WEB] Server started on port 80");
}

static bool scan_running = false;

static void process_wifi_scan(void) {
    if (flag_wifi_scan && !scan_running) {
        flag_wifi_scan = false;
        Serial.println("[WEB] Starting async WiFi scan (AP_STA mode)...");

        bool ble_was_adv = false;
        if (NimBLEDevice::isInitialized() && NimBLEDevice::getAdvertising()->isAdvertising()) {
            NimBLEDevice::getAdvertising()->stop();
            ble_was_adv = true;
        }

        WiFi.scanNetworks(true, true, false, 300);
        scan_running = true;
    }

    if (scan_running) {
        int16_t n = WiFi.scanComplete();
        if (n >= 0) {
            Serial.printf("[WEB] Async WiFi scan done: %d networks found\n", n);
            scan_running = false;

            if (NimBLEDevice::isInitialized() && !ble_uart_is_connected()) {
                NimBLEDevice::getAdvertising()->start();
            }

            JsonDocument doc;
            doc["type"] = "wifi_scan";
            JsonArray nets = doc["networks"].to<JsonArray>();
            scan_cache_count = 0;
            if (n > 0) {
                int max = n > 30 ? 30 : n;
                for (int i = 0; i < max; i++) {
                    strncpy(scan_cache[i].ssid, WiFi.SSID(i).c_str(), 32);
                    scan_cache[i].ssid[32] = '\0';
                    strncpy(scan_cache[i].bssid, WiFi.BSSIDstr(i).c_str(), 17);
                    scan_cache[i].bssid[17] = '\0';
                    scan_cache[i].rssi = WiFi.RSSI(i);
                    scan_cache[i].ch = WiFi.channel(i);

                    JsonObject o = nets.add<JsonObject>();
                    o["ssid"] = scan_cache[i].ssid;
                    o["rssi"] = scan_cache[i].rssi;
                    o["ch"] = scan_cache[i].ch;
                }
                scan_cache_count = max;
            }
            scan_cache_ts = millis();
            char buf[2048];
            serializeJson(doc, buf, sizeof(buf));
            ws_broadcast(buf);
            WiFi.scanDelete();
        } else if (n == WIFI_SCAN_FAILED) {
            Serial.println("[WEB] Async WiFi scan failed.");
            scan_running = false;

            if (NimBLEDevice::isInitialized() && !ble_uart_is_connected()) {
                NimBLEDevice::getAdvertising()->start();
            }

            JsonDocument doc;
            doc["type"] = "wifi_scan";
            doc["networks"] = JsonArray();
            char buf[128];
            serializeJson(doc, buf, sizeof(buf));
            ws_broadcast(buf);
        }
    }
}

static void process_hw_flags(void) {
    if (flag_haptic) {
        flag_haptic = false;
        haptic_buzz();
    }
    if (flag_brightness) {
        flag_brightness = false;
        instance.setBrightness(pending_brightness);
    }
    if (flag_ntp_retry) {
        flag_ntp_retry = false;
        time_sync_force_retry();
    }
    if (flag_watchface_next) { flag_watchface_next = false; watchface_next(); }
    if (flag_watchface_prev) { flag_watchface_prev = false; watchface_prev(); }
    if (flag_service_change) {
        flag_service_change = false;
        if (strcmp(pending_svc, "gps") == 0)    { gps_app_set_enabled(pending_svc_en); svc_gps = pending_svc_en; }
        else if (strcmp(pending_svc, "nfc") == 0) { instance.powerControl(POWER_NFC, pending_svc_en); svc_nfc = pending_svc_en; }
        else if (strcmp(pending_svc, "haptic") == 0) { haptic_set_enabled(pending_svc_en); }
    }
    if (flag_lora_start) { flag_lora_start = false; lora_svc_start(MODE_MESHCORE); }
    if (flag_lora_stop)  { flag_lora_stop  = false; lora_svc_stop(); }
    if (flag_lora_advert){ flag_lora_advert= false; lora_svc_send_advert(); }
    if (flag_lora_send)  { flag_lora_send  = false; lora_svc_send_message(pending_lora_text); }
    if (flag_reboot) {
        flag_reboot = false;
        delay(300);
        ESP.restart();
    }
}

void web_server_loop(void) {
    if (!active) return;
    ws.cleanupClients();
    push_status();

    process_hw_flags();
    process_wifi_scan();

    if (show_overlay_scan) {
        show_overlay_scan = false;
        action_overlay_show("NFC SCAN");
        action_overlay_set_status("Hold tag near watch...");
        power_hal_reset_activity();
    }
    if (show_overlay_emit) {
        show_overlay_emit = false;
        action_overlay_show("NFC EMULATE");
        action_overlay_set_status("Emitting tag...");
        power_hal_reset_activity();
    }
    if (hide_overlay_flag) {
        hide_overlay_flag = false;
        action_overlay_hide();
    }

    if (nfc_svc_tag_detected_web()) {
        const char *uid = nfc_svc_last_uid();
        const char *ndef = nfc_svc_last_ndef();
        web_push_nfc_tag(uid, ndef);

        if (action_overlay_is_active()) {
            char b[80]; snprintf(b, sizeof(b), "TAG FOUND!\nUID: %s", uid);
            action_overlay_set_status(b);
        }
    }

    if (lora_svc_has_new_message_web()) {
        const MeshMsg *m = lora_svc_last_message();
        if (m) {
            JsonDocument doc;
            doc["type"] = "lora_msg";
            doc["from"] = m->channel;
            doc["text"] = m->text;
            doc["hops"] = m->hops;
            doc["rssi"] = (int)m->rssi;
            doc["ts"] = m->timestamp;
            char buf[400];
            serializeJson(doc, buf, sizeof(buf));
            ws_broadcast(buf);
        }
    }

    if (show_overlay_exfil) {
        show_overlay_exfil = false;
        if (!action_overlay_is_active()) {
            action_overlay_show(exfil_overlay_title);
        }
        action_overlay_set_status(exfil_overlay_status);
        power_hal_reset_activity();
    }

    static uint32_t overlay_finished_time = 0;
    if (exfil_overlay_finished) {
        if (overlay_finished_time == 0) {
            overlay_finished_time = millis();
        } else if (millis() - overlay_finished_time > 4000) { 
            exfil_overlay_finished = false;
            overlay_finished_time = 0;
            action_overlay_hide();
        }
    } else {
        overlay_finished_time = 0;
    }

    if (action_overlay_is_active()) {
        power_hal_reset_activity();
    }
}

void web_server_stop(void) {
    if (!active) return;
    server.end();
    WiFi.softAPdisconnect(true);
    active = false;
    Serial.println("[WEB] Server stopped");
}

bool web_server_is_active(void) {
    return active;
}

const char* web_server_get_ip(void) {
    return ap_ip;
}

void web_push_nfc_tag(const char *uid, const char *ndef) {
    if (!active) return;
    JsonDocument doc;
    doc["type"] = "nfc_tag";
    doc["uid"] = uid;
    doc["ndef"] = ndef ? ndef : "";
    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    ws_broadcast(buf);
}

void web_push_log(const char *msg) {
    if (!active || !msg) return;

    size_t len = strlen(msg);
    if (len > 3800) return;
    ws_broadcast(msg);
}
