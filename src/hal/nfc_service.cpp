#include "nfc_service.h"
#include <Preferences.h>
#include <FS.h>
#include <SD.h>
#include <cstdio>
#include <cstring>
#include "../config.h"
#include "haptic.h"

#if defined(USING_ST25R3916)
#include "nfc_hal.h"
#include "nfc_emu.h"
#endif

enum NfcSvcMode { SVC_IDLE, SVC_SCANNING, SVC_EMULATING };
static NfcSvcMode svc_mode = SVC_IDLE;
static uint32_t last_poll = 0;

static volatile bool cmd_scan = false;
static volatile bool cmd_stop = false;
static volatile bool cmd_save = false;
static volatile bool cmd_export = false;
static volatile bool cmd_emulate = false;
static volatile bool cmd_select_next = false;
static volatile int  cmd_delete_idx = -1;

static char last_uid[64] = "";
static char last_ndef_text[256] = "";
static uint8_t last_nfcid[10] = {};
static uint8_t last_nfcid_len = 0;
static volatile bool tag_found = false;
static volatile bool tag_found_web = false;
static volatile bool tag_found_ble = false;
static int tag_count = 0;

#define MAX_SAVED_TAGS 8
struct SavedTag {
    char name[32];
    uint8_t uid[10];
    uint8_t uid_len;
    char ndef[128];
};
static SavedTag saved_tags[MAX_SAVED_TAGS];
static int saved_count = 0;
static int selected_tag = -1;
static Preferences nfc_prefs;

static void load_tags(void) {
    if (nfc_prefs.begin("nfc_tags", true)) {
        saved_count = nfc_prefs.getInt("count", 0);
        if (saved_count > MAX_SAVED_TAGS) saved_count = MAX_SAVED_TAGS;
        for (int i = 0; i < saved_count; i++) {
            char key[16]; snprintf(key, sizeof(key), "tag%d", i);
            nfc_prefs.getBytes(key, &saved_tags[i], sizeof(SavedTag));
        }
        nfc_prefs.end();
    }
    if (saved_count > 0) selected_tag = 0;
}

static void save_tags(void) {
    if (nfc_prefs.begin("nfc_tags", false)) {
        nfc_prefs.putInt("count", saved_count);
        for (int i = 0; i < saved_count; i++) {
            char key[16]; snprintf(key, sizeof(key), "tag%d", i);
            nfc_prefs.putBytes(key, &saved_tags[i], sizeof(SavedTag));
        }
        nfc_prefs.end();
    }
}

static bool export_flipper(const SavedTag &tag, int idx) {
    if (!SD.exists("/nfc")) SD.mkdir("/nfc");
    char fn[64]; snprintf(fn, sizeof(fn), "/nfc/tag_%d.nfc", idx);
    File f = SD.open(fn, FILE_WRITE);
    if (!f) return false;

    f.println("Filetype: Flipper NFC device");
    f.println("Version: 4");
    f.println(tag.uid_len == 7 ? "Device type: NTAG215" : "Device type: ISO14443-3A");
    f.print("UID:");
    for (int i = 0; i < tag.uid_len; i++) f.printf(" %02X", tag.uid[i]);
    f.println();
    f.println("ATQA: 00 44");
    f.println("SAK: 00");

    if (tag.uid_len == 7) {
        f.println("Data format version: 2");
        f.println("NTAG type: NTAG215");
        f.println();
        f.print("Signature:"); for (int i = 0; i < 32; i++) f.print(" 00"); f.println();
        f.println("Mifare version: 00 04 04 02 01 00 11 03");
        f.println();
        for (int c = 0; c < 3; c++) { f.printf("Counter %d: 0\n", c); f.printf("Tearing %d: 00\n", c); }
        f.println();
        f.println("Pages total: 135");
        f.println("Pages read: 135");
        f.printf("Page 0: %02X %02X %02X %02X\n", tag.uid[0], tag.uid[1], tag.uid[2],
            (uint8_t)(tag.uid[0]^tag.uid[1]^tag.uid[2]^0x88));
        f.printf("Page 1: %02X %02X %02X %02X\n", tag.uid[3], tag.uid[4], tag.uid[5], tag.uid[6]);
        f.printf("Page 2: %02X 48 00 00\n", (uint8_t)(tag.uid[3]^tag.uid[4]^tag.uid[5]^tag.uid[6]));
        f.println("Page 3: E1 10 3E 00");

        uint8_t pages[135][4]; memset(pages, 0, sizeof(pages));
        uint8_t ndef[256]; int np = 0;
        if (tag.ndef[0]) {
            int tl = strlen(tag.ndef); if (tl > 200) tl = 200;
            ndef[np++]=0xD1; ndef[np++]=0x01; ndef[np++]=tl+3; ndef[np++]='T';
            ndef[np++]=0x02; ndef[np++]='e'; ndef[np++]='n';
            memcpy(&ndef[np], tag.ndef, tl); np += tl;
        }
        int pi=4, bi=0;
        uint8_t tlv[2]={0x03,(uint8_t)np};
        for (int i=0;i<2&&pi<135;i++){pages[pi][bi++]=tlv[i];if(bi==4){bi=0;pi++;}}
        for (int i=0;i<np&&pi<135;i++){pages[pi][bi++]=ndef[i];if(bi==4){bi=0;pi++;}}
        if(pi<135) pages[pi][bi]=0xFE;
        for (int p=4;p<135;p++) f.printf("Page %d: %02X %02X %02X %02X\n",p,pages[p][0],pages[p][1],pages[p][2],pages[p][3]);
    }
    f.close();
    return true;
}

#if defined(USING_ST25R3916)
static void on_tag_found(void) {
    rfalNfcDevice *dev;
    NFCReader.rfalNfcGetActiveDevice(&dev);
    last_nfcid_len = dev->nfcidLen;
    if (last_nfcid_len > 10) last_nfcid_len = 10;
    memcpy(last_nfcid, dev->nfcid, last_nfcid_len);
    int pos = 0;
    for (int i = 0; i < last_nfcid_len && pos < 60; i++)
        pos += snprintf(last_uid+pos, sizeof(last_uid)-pos, "%02X%s", last_nfcid[i], i<last_nfcid_len-1?":":"");
    last_ndef_text[0] = 0;
    tag_found = true;
    tag_found_web = true;
    tag_found_ble = true;
    tag_count++;
}

static void on_ndef(ndefTypeId id, void *data) {
    if (id == NDEF_TYPE_RTD_TEXT) {
        ndefRtdText *t = (ndefRtdText*)data;
        if (t && t->bufSentence.buffer) {
            int l = t->bufSentence.length > 250 ? 250 : t->bufSentence.length;
            memcpy(last_ndef_text, t->bufSentence.buffer, l); last_ndef_text[l] = 0;
        }
    } else if (id == NDEF_TYPE_RTD_URI) {
        ndefRtdUri *u = (ndefRtdUri*)data;
        if (u && u->bufUriString.buffer) {
            int l = u->bufUriString.length > 250 ? 250 : u->bufUriString.length;
            memcpy(last_ndef_text, u->bufUriString.buffer, l); last_ndef_text[l] = 0;
        }
    }
}
#endif

static void stop_current(void) {
#if defined(USING_ST25R3916)
    if (svc_mode == SVC_SCANNING) deinitNFC();
    if (svc_mode == SVC_EMULATING) nfc_emu_stop();
#endif
    svc_mode = SVC_IDLE;
}

void nfc_service_init(void) {
    load_tags();
    Serial.printf("[NFC-SVC] Loaded %d tags\n", saved_count);
}

static void do_start_scan(void);
static void do_stop(void);
static void do_save(void);
static void do_delete(int idx);
static void do_export(void);
static void do_emulate(void);
static void check_nfc_init(void);

void nfc_service_loop(void) {

    if (cmd_scan)        { cmd_scan = false; do_start_scan(); }
    if (cmd_stop)        { cmd_stop = false; do_stop(); }
    if (cmd_save)        { cmd_save = false; do_save(); }
    if (cmd_export)      { cmd_export = false; do_export(); }
    if (cmd_emulate)     { cmd_emulate = false; do_emulate(); }
    if (cmd_select_next) { cmd_select_next = false;
        if (saved_count > 0) selected_tag = (selected_tag + 1) % saved_count; }
    if (cmd_delete_idx >= 0) { int i = cmd_delete_idx; cmd_delete_idx = -1; do_delete(i); }

    check_nfc_init();

#if defined(USING_ST25R3916)
    uint32_t now = millis();
    if (now - last_poll < 30) return;
    last_poll = now;

    if (svc_mode == SVC_SCANNING) loopNFCReader();
    if (svc_mode == SVC_EMULATING) {
        nfc_emu_loop();
        if (!nfc_emu_is_active()) svc_mode = SVC_IDLE;
    }
#endif
}

static uint8_t nfc_init_stage = 0;
static uint32_t nfc_init_time = 0;

static void do_start_scan(void) {
    stop_current();
#if defined(USING_ST25R3916)
    instance.powerControl(POWER_NFC, false);
    nfc_init_stage = 1;
    nfc_init_time = millis();
#endif
}

static void check_nfc_init(void) {
#if defined(USING_ST25R3916)
    if (nfc_init_stage == 0) return;
    uint32_t elapsed = millis() - nfc_init_time;
    if (nfc_init_stage == 1 && elapsed >= 50) {
        instance.powerControl(POWER_NFC, true);
        nfc_init_stage = 2;
        nfc_init_time = millis();
    } else if (nfc_init_stage == 2 && elapsed >= 50) {
        if (beginNFC(on_tag_found, on_ndef)) {
            svc_mode = SVC_SCANNING;
            Serial.println("[NFC-SVC] Scanning");
        }
        nfc_init_stage = 0;
    }
#endif
}

static void do_stop(void) {
    stop_current();
#if defined(USING_ST25R3916)
    instance.powerControl(POWER_NFC, false);
#endif
}

static void do_save(void) {
    if (last_uid[0] == 0) return;
    if (saved_count >= MAX_SAVED_TAGS) {
        memmove(&saved_tags[0], &saved_tags[1], sizeof(SavedTag)*(MAX_SAVED_TAGS-1));
        saved_count = MAX_SAVED_TAGS - 1;
    }
    SavedTag &t = saved_tags[saved_count];
    snprintf(t.name, sizeof(t.name), "Tag-%s", last_uid+(strlen(last_uid)>8?strlen(last_uid)-8:0));
    memcpy(t.uid, last_nfcid, last_nfcid_len);
    t.uid_len = last_nfcid_len;
    strncpy(t.ndef, last_ndef_text, sizeof(t.ndef)-1);
    t.ndef[sizeof(t.ndef)-1] = 0;
    saved_count++;
    save_tags();
    export_flipper(saved_tags[saved_count-1], saved_count-1);
    haptic_success();

    last_uid[0] = 0; last_ndef_text[0] = 0; last_nfcid_len = 0;
    Serial.printf("[NFC-SVC] Saved tag #%d\n", saved_count);
}

static void do_delete(int idx) {
    if (idx < 0 || idx >= saved_count) return;
    for (int i = idx; i < saved_count-1; i++) saved_tags[i] = saved_tags[i+1];
    saved_count--;
    save_tags();
    if (selected_tag >= saved_count) selected_tag = saved_count-1;
}

static void do_export(void) {
    for (int i = 0; i < saved_count; i++) export_flipper(saved_tags[i], i);
}

static void do_emulate(void) {
    if (selected_tag < 0 || selected_tag >= saved_count) return;
    stop_current();
#if defined(USING_ST25R3916)
    instance.powerControl(POWER_NFC, false); delay(50);
    instance.powerControl(POWER_NFC, true); delay(50);
    NFCReader.getRfalRf()->rfalInitialize(); delay(10);
    SavedTag &t = saved_tags[selected_tag];
    if (nfc_emu_start(t.uid, t.uid_len, t.ndef)) {
        svc_mode = SVC_EMULATING;
        haptic_buzz();
    }
#endif
}

void nfc_svc_request_scan(void)          { cmd_scan = true; }
void nfc_svc_request_stop(void)          { cmd_stop = true; }
void nfc_svc_request_save(void)          { cmd_save = true; }
void nfc_svc_request_delete(int idx)     { cmd_delete_idx = idx; }
void nfc_svc_request_export(void)        { cmd_export = true; }
void nfc_svc_request_select_next(void)   { cmd_select_next = true; }
void nfc_svc_request_emulate(void)       { cmd_emulate = true; }

bool nfc_svc_is_scanning(void) { return svc_mode == SVC_SCANNING; }
bool nfc_svc_is_emulating(void) { return svc_mode == SVC_EMULATING; }
const char* nfc_svc_last_uid(void) { return last_uid; }
const char* nfc_svc_last_ndef(void) { return last_ndef_text; }
int nfc_svc_saved_count(void) { return saved_count; }
int nfc_svc_selected_idx(void) { return selected_tag; }
const char* nfc_svc_tag_name(int idx) { return (idx>=0&&idx<saved_count)?saved_tags[idx].name:""; }
bool nfc_svc_tag_detected(void) { bool r=tag_found; tag_found=false; return r; }
bool nfc_svc_tag_detected_web(void) { bool r=tag_found_web; tag_found_web=false; return r; }
bool nfc_svc_tag_detected_ble(void) { bool r=tag_found_ble; tag_found_ble=false; return r; }
