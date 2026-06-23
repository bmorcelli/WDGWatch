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

enum NfcSvcMode { SVC_IDLE, SVC_SCANNING, SVC_EMULATING, SVC_DICT_ATTACK, SVC_EMV_READ };
static NfcSvcMode svc_mode = SVC_IDLE;
static uint32_t last_poll = 0;

static volatile bool cmd_scan = false;
static volatile bool cmd_stop = false;
static volatile bool cmd_save = false;
static volatile bool cmd_export = false;
static volatile bool cmd_emulate = false;
static volatile bool cmd_select_next = false;
static volatile int  cmd_delete_idx = -1;
static volatile bool cmd_dict_attack = false;
static volatile bool cmd_emv_read = false;
static volatile bool cmd_load_sd = false;

static char last_uid[64] = "";
static char last_ndef_text[256] = "";
static char last_tag_type[32] = "";
static uint8_t last_nfcid[10] = {};
static uint8_t last_nfcid_len = 0;
static uint8_t last_sak = 0;
static uint8_t last_atqa[2] = {};
static volatile bool tag_found = false;
static volatile bool tag_found_web = false;
static volatile bool tag_found_ble = false;
static int tag_count = 0;

#define MAX_SAVED_TAGS 16
struct SavedTag {
    char name[32];
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t sak;
    uint8_t atqa[2];
    char ndef[128];
    char type[16];
    uint8_t sector_data[16][64];
    uint8_t sector_keys[16][2][6];
    uint8_t sectors_dumped;
    bool has_sectors;
};
static SavedTag saved_tags[MAX_SAVED_TAGS];
static int saved_count = 0;
static int selected_tag = -1;
static Preferences nfc_prefs;

static const uint8_t DEFAULT_KEYS[][6] = {
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
    {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5},
    {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5},
    {0x4D,0x3A,0x99,0xC3,0x51,0xDD},
    {0x1A,0x98,0x2C,0x7E,0x45,0x9A},
    {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7},
    {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF},
    {0x00,0x00,0x00,0x00,0x00,0x00},
    {0x71,0x4C,0x5C,0x88,0x6E,0x97},
    {0x58,0x7E,0xE5,0xF9,0x35,0x0F},
    {0xA0,0x47,0x8C,0xC3,0x90,0x91},
    {0x53,0x3C,0xB6,0xC7,0x23,0xF6},
    {0x8F,0xD0,0xA4,0xF2,0x56,0xE9},
    {0xFC,0x00,0x01,0x87,0x78,0xF7},
    {0x00,0x01,0x02,0x03,0x04,0x05},
    {0x06,0x07,0x08,0x09,0x0A,0x0B},
};
static int dict_key_count = 0;
static uint8_t dict_custom_keys[64][6];
static int dict_custom_count = 0;
static int dict_current_sector = 0;
static int dict_current_key = 0;
static int dict_sectors_found = 0;
static int dict_total_sectors = 16;
static char dict_status[64] = "";
static bool dict_sector_found[16] = {};
static uint8_t dict_found_keys[16][2][6];

static char emv_pan[24] = "";
static char emv_expiry[8] = "";
static char emv_cardholder[32] = "";
static char emv_brand[16] = "";
static volatile bool emv_found_flag = false;

#define MAX_SD_FILES 32
static char sd_files[MAX_SD_FILES][32];
static int sd_file_count = 0;

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

static void detect_tag_type(uint8_t sak, uint8_t *atqa, uint8_t uid_len) {
    if (sak == 0x08) strncpy(last_tag_type, "MF Classic 1K", sizeof(last_tag_type));
    else if (sak == 0x18) strncpy(last_tag_type, "MF Classic 4K", sizeof(last_tag_type));
    else if (sak == 0x09) strncpy(last_tag_type, "MF Mini", sizeof(last_tag_type));
    else if (sak == 0x00 && uid_len == 7) strncpy(last_tag_type, "NTAG/Ultralight", sizeof(last_tag_type));
    else if (sak == 0x20) strncpy(last_tag_type, "ISO14443-4/DESFire", sizeof(last_tag_type));
    else if (sak == 0x28 || sak == 0x38) strncpy(last_tag_type, "EMV/SmartCard", sizeof(last_tag_type));
    else snprintf(last_tag_type, sizeof(last_tag_type), "SAK:0x%02X", sak);
}

static bool export_flipper(const SavedTag &tag, int idx) {
    if (!SD.exists("/nfc")) SD.mkdir("/nfc");
    char fn[64]; snprintf(fn, sizeof(fn), "/nfc/tag_%d.nfc", idx);
    File f = SD.open(fn, FILE_WRITE);
    if (!f) return false;

    f.println("Filetype: Flipper NFC device");
    f.println("Version: 4");

    if (strcmp(tag.type, "MF Classic 1K") == 0) {
        f.println("Device type: Mifare Classic");
        f.print("UID:"); for (int i=0;i<tag.uid_len;i++) f.printf(" %02X",tag.uid[i]); f.println();
        f.printf("ATQA: %02X %02X\n", tag.atqa[0], tag.atqa[1]);
        f.printf("SAK: %02X\n", tag.sak);
        f.println("Mifare Classic type: 1K");
        f.printf("Data format version: 2\n");
        if (tag.has_sectors) {
            for (int s=0; s<16; s++) {
                f.printf("Block %d:", s*4);
                for (int b=0;b<16;b++) f.printf(" %02X",tag.sector_data[s][b]);
                f.println();
                for (int bl=1;bl<4;bl++) {
                    f.printf("Block %d:", s*4+bl);
                    for (int b=0;b<16;b++) f.printf(" %02X",tag.sector_data[s][bl*16+b]);
                    f.println();
                }
            }
        }
    } else {
        f.println(tag.uid_len == 7 ? "Device type: NTAG215" : "Device type: ISO14443-3A");
        f.print("UID:"); for (int i=0;i<tag.uid_len;i++) f.printf(" %02X",tag.uid[i]); f.println();
        f.printf("ATQA: %02X %02X\n", tag.atqa[0], tag.atqa[1]);
        f.printf("SAK: %02X\n", tag.sak);

        if (tag.uid_len == 7) {
            f.println("Data format version: 2");
            f.println("NTAG type: NTAG215");
            f.println();
            f.print("Signature:"); for (int i=0;i<32;i++) f.print(" 00"); f.println();
            f.println("Mifare version: 00 04 04 02 01 00 11 03");
            f.println();
            for (int c=0;c<3;c++){f.printf("Counter %d: 0\n",c);f.printf("Tearing %d: 00\n",c);}
            f.println();
            f.println("Pages total: 135");
            f.println("Pages read: 135");
            f.printf("Page 0: %02X %02X %02X %02X\n",tag.uid[0],tag.uid[1],tag.uid[2],
                (uint8_t)(tag.uid[0]^tag.uid[1]^tag.uid[2]^0x88));
            f.printf("Page 1: %02X %02X %02X %02X\n",tag.uid[3],tag.uid[4],tag.uid[5],tag.uid[6]);
            f.printf("Page 2: %02X 48 00 00\n",(uint8_t)(tag.uid[3]^tag.uid[4]^tag.uid[5]^tag.uid[6]));
            f.println("Page 3: E1 10 3E 00");
            for (int p=4;p<135;p++) f.printf("Page %d: 00 00 00 00\n",p);
        }
    }
    f.close();
    return true;
}

static void scan_sd_files(void) {
    sd_file_count = 0;
    if (!SD.exists("/nfc")) return;
    File dir = SD.open("/nfc");
    if (!dir) return;
    File entry;
    while ((entry = dir.openNextFile()) && sd_file_count < MAX_SD_FILES) {
        if (!entry.isDirectory()) {
            const char *name = entry.name();
            if (name) {
                strncpy(sd_files[sd_file_count], name, 31);
                sd_files[sd_file_count][31] = 0;
                sd_file_count++;
            }
        }
        entry.close();
    }
    dir.close();
}

static void load_dict_from_sd(void) {
    dict_custom_count = 0;
    if (!SD.exists("/nfc/assets/keys.txt")) return;
    File f = SD.open("/nfc/assets/keys.txt", FILE_READ);
    if (!f) return;
    char line[16];
    while (f.available() && dict_custom_count < 64) {
        int len = 0;
        while (f.available() && len < 15) {
            char c = f.read();
            if (c == '\n' || c == '\r') break;
            line[len++] = c;
        }
        line[len] = 0;
        if (len >= 12) {
            for (int i = 0; i < 6; i++) {
                char hex[3] = { line[i*2], line[i*2+1], 0 };
                dict_custom_keys[dict_custom_count][i] = strtol(hex, NULL, 16);
            }
            dict_custom_count++;
        }
    }
    f.close();
    Serial.printf("[NFC-SVC] Loaded %d custom keys\n", dict_custom_count);
}

#if defined(USING_ST25R3916)
static void on_tag_found(void) {
    rfalNfcDevice *dev;
    NFCReader.rfalNfcGetActiveDevice(&dev);
    last_nfcid_len = dev->nfcidLen;
    if (last_nfcid_len > 10) last_nfcid_len = 10;
    memcpy(last_nfcid, dev->nfcid, last_nfcid_len);

    if (dev->type == RFAL_NFC_LISTEN_TYPE_NFCA) {
        last_sak = dev->dev.nfca.selRes.sak;
        last_atqa[0] = dev->dev.nfca.sensRes.anticollisionInfo;
        last_atqa[1] = dev->dev.nfca.sensRes.platformInfo;
    } else {
        last_sak = 0;
        last_atqa[0] = 0; last_atqa[1] = 0;
    }
    detect_tag_type(last_sak, last_atqa, last_nfcid_len);

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

#if defined(USING_ST25R3916)
static bool mf_auth_sector(uint8_t block, uint8_t key_type, const uint8_t *key) {
    uint8_t txBuf[12];
    uint8_t rxBuf[16];
    uint16_t rxLen = 0;

    txBuf[0] = (key_type == 0) ? 0x60 : 0x61; 
    txBuf[1] = block;

    ReturnCode err = NFCReader.getRfalRf()->rfalTransceiveBlockingTxRx(
        txBuf, 2, rxBuf, sizeof(rxBuf), &rxLen,
        RFAL_TXRX_FLAGS_DEFAULT, 100);

    if (err != ST_ERR_NONE) return false;
    return true;
}

static bool mf_read_block(uint8_t block, uint8_t *out16) {
    uint8_t txBuf[2] = { 0x30, block };
    uint8_t rxBuf[18];
    uint16_t rxLen = 0;

    ReturnCode err = NFCReader.getRfalRf()->rfalTransceiveBlockingTxRx(
        txBuf, 2, rxBuf, sizeof(rxBuf), &rxLen,
        RFAL_TXRX_FLAGS_DEFAULT, 100);

    if (err == ST_ERR_NONE && rxLen >= 16) {
        memcpy(out16, rxBuf, 16);
        return true;
    }
    return false;
}

static bool emv_send_apdu(const uint8_t *apdu, uint16_t apdu_len, uint8_t *resp, uint16_t *resp_len) {
    *resp_len = 0;
    ReturnCode err = NFCReader.getRfalRf()->rfalTransceiveBlockingTxRx(
        (uint8_t*)apdu, apdu_len,
        resp, 256, resp_len,
        RFAL_TXRX_FLAGS_DEFAULT, 2000);

    if (err == ST_ERR_NONE && *resp_len > 2) {
        return true;
    }
    Serial.printf("[NFC-EMV] APDU err=%d rxLen=%d\n", err, *resp_len);
    return false;
}

static void parse_emv_response(const uint8_t *data, uint16_t len) {
    
    for (uint16_t i = 0; i + 2 < len; i++) {
        if (data[i] == 0x5A) {
            uint8_t pan_len = data[i+1];
            if (pan_len > 0 && pan_len <= 10 && i+2+pan_len <= len) {
                int pos = 0;
                for (int j = 0; j < pan_len && pos < 22; j++) {
                    uint8_t hi = (data[i+2+j] >> 4) & 0x0F;
                    uint8_t lo = data[i+2+j] & 0x0F;
                    if (hi <= 9) emv_pan[pos++] = '0'+hi;
                    if (lo <= 9 && lo != 0x0F) emv_pan[pos++] = '0'+lo;
                    if ((j+1) % 4 == 0 && j+1 < pan_len) emv_pan[pos++] = ' ';
                }
                emv_pan[pos] = 0;
                
                int plen = strlen(emv_pan);
                if (plen > 8) {
                    for (int k = 4; k < plen - 4; k++) {
                        if (emv_pan[k] != ' ') emv_pan[k] = '*';
                    }
                }
            }
        }
        
        if (data[i] == 0x5F && i+1 < len && data[i+1] == 0x24 && i+5 < len) {
            uint8_t elen = data[i+2];
            if (elen >= 3 && i+5 <= len) {
                snprintf(emv_expiry, sizeof(emv_expiry), "%02X/%02X", data[i+4], data[i+3]);
            }
        }
    }
    
    if (emv_pan[0] == '4') strncpy(emv_brand, "VISA", sizeof(emv_brand));
    else if (emv_pan[0] == '5' || emv_pan[0] == '2') strncpy(emv_brand, "MASTERCARD", sizeof(emv_brand));
    else if (emv_pan[0] == '3') strncpy(emv_brand, "AMEX", sizeof(emv_brand));
    else if (emv_pan[0] == '9') strncpy(emv_brand, "TROY", sizeof(emv_brand));
    else strncpy(emv_brand, "UNKNOWN", sizeof(emv_brand));
}
#endif

static void stop_current(void) {
#if defined(USING_ST25R3916)
    if (svc_mode == SVC_SCANNING || svc_mode == SVC_DICT_ATTACK || svc_mode == SVC_EMV_READ) deinitNFC();
    if (svc_mode == SVC_EMULATING) nfc_emu_stop();
#endif
    svc_mode = SVC_IDLE;
    dict_current_sector = 0;
    dict_current_key = 0;
}

void nfc_service_init(void) {
    load_tags();
    load_dict_from_sd();
    scan_sd_files();
    dict_key_count = sizeof(DEFAULT_KEYS)/6 + dict_custom_count;
    Serial.printf("[NFC-SVC] Loaded %d tags, %d keys (%d default + %d custom)\n",
        saved_count, dict_key_count, (int)(sizeof(DEFAULT_KEYS)/6), dict_custom_count);
}

static void do_start_scan(void);
static void do_stop(void);
static void do_save(void);
static void do_delete(int idx);
static void do_export(void);
static void do_emulate(void);
static void do_dict_attack(void);
static void do_emv_read(void);
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
    if (cmd_dict_attack) { cmd_dict_attack = false; do_dict_attack(); }
    if (cmd_emv_read)    { cmd_emv_read = false; do_emv_read(); }
    if (cmd_load_sd)     { cmd_load_sd = false; scan_sd_files(); }

    check_nfc_init();

#if defined(USING_ST25R3916)
    if (svc_mode == SVC_SCANNING || svc_mode == SVC_EMV_READ) loopNFCReader();
    if (svc_mode == SVC_EMULATING) {
        nfc_emu_loop();
        if (!nfc_emu_is_active()) svc_mode = SVC_IDLE;
    }

    uint32_t now = millis();
    if (now - last_poll < 30) return;
    last_poll = now;

    
    if (svc_mode == SVC_DICT_ATTACK) {
        if (dict_current_sector < dict_total_sectors) {
            if (!dict_sector_found[dict_current_sector]) {
                const uint8_t *try_key;
                int default_count = sizeof(DEFAULT_KEYS) / 6;
                if (dict_current_key < default_count)
                    try_key = DEFAULT_KEYS[dict_current_key];
                else
                    try_key = dict_custom_keys[dict_current_key - default_count];

                uint8_t block = dict_current_sector * 4;
                bool key_a_ok = mf_auth_sector(block, 0, try_key);
                bool key_b_ok = false;
                if (!key_a_ok) key_b_ok = mf_auth_sector(block, 1, try_key);

                if (key_a_ok || key_b_ok) {
                    dict_sector_found[dict_current_sector] = true;
                    memcpy(dict_found_keys[dict_current_sector][key_a_ok ? 0 : 1], try_key, 6);
                    dict_sectors_found++;
                    haptic_click();
                    Serial.printf("[NFC-DICT] Sector %d cracked with key %s\n",
                        dict_current_sector, key_a_ok ? "A" : "B");

                    
                    for (int bl = 0; bl < 4; bl++) {
                        mf_read_block(block + bl, &saved_tags[saved_count > 0 ? saved_count-1 : 0].sector_data[dict_current_sector][bl*16]);
                    }
                }

                snprintf(dict_status, sizeof(dict_status), "S:%d/%d K:%d/%d",
                    dict_current_sector, dict_total_sectors,
                    dict_current_key+1, dict_key_count);
            }

            dict_current_key++;
            if (dict_current_key >= dict_key_count) {
                dict_current_key = 0;
                dict_current_sector++;
            }
        } else {
            
            snprintf(dict_status, sizeof(dict_status), "DONE! %d/%d CRACKED",
                dict_sectors_found, dict_total_sectors);
            haptic_success();
            svc_mode = SVC_IDLE;
            Serial.printf("[NFC-DICT] Complete: %d/%d sectors\n", dict_sectors_found, dict_total_sectors);
        }
    }

    
    if (svc_mode == SVC_EMV_READ && tag_found) {
        tag_found = false;
        Serial.println("[NFC-EMV] Card detected, sending APDU...");

        
        static const uint8_t SELECT_PSE[] = {
            0x00, 0xA4, 0x04, 0x00, 0x0E,
            '2','P','A','Y','.','S','Y','S','.','D','D','F','0','1', 0x00
        };
        uint8_t resp[256];
        uint16_t rlen = 0;

        bool ok = emv_send_apdu(SELECT_PSE, sizeof(SELECT_PSE), resp, &rlen);
        if (ok && rlen > 2) {
            
            
            static const uint8_t VISA_AID[] = { 0x00,0xA4,0x04,0x00,0x07, 0xA0,0x00,0x00,0x00,0x03,0x10,0x10, 0x00 };
            static const uint8_t MC_AID[]   = { 0x00,0xA4,0x04,0x00,0x07, 0xA0,0x00,0x00,0x00,0x04,0x10,0x10, 0x00 };

            rlen = 0;
            ok = emv_send_apdu(VISA_AID, sizeof(VISA_AID), resp, &rlen);
            if (!ok || rlen <= 2) {
                rlen = 0;
                ok = emv_send_apdu(MC_AID, sizeof(MC_AID), resp, &rlen);
            }

            if (ok && rlen > 2) {
                
                static const uint8_t GPO[] = { 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 };
                rlen = 0;
                ok = emv_send_apdu(GPO, sizeof(GPO), resp, &rlen);

                
                static const uint8_t READ_REC[] = { 0x00, 0xB2, 0x01, 0x0C, 0x00 };
                rlen = 0;
                ok = emv_send_apdu(READ_REC, sizeof(READ_REC), resp, &rlen);
                if (ok && rlen > 4) {
                    parse_emv_response(resp, rlen);
                }

                
                for (int rec = 2; rec <= 4 && emv_pan[0] == 0; rec++) {
                    uint8_t read_cmd[] = { 0x00, 0xB2, (uint8_t)rec, 0x0C, 0x00 };
                    rlen = 0;
                    ok = emv_send_apdu(read_cmd, sizeof(read_cmd), resp, &rlen);
                    if (ok && rlen > 4) parse_emv_response(resp, rlen);
                }

                
                for (int rec = 1; rec <= 4 && emv_pan[0] == 0; rec++) {
                    uint8_t read_cmd[] = { 0x00, 0xB2, (uint8_t)rec, 0x14, 0x00 };
                    rlen = 0;
                    ok = emv_send_apdu(read_cmd, sizeof(read_cmd), resp, &rlen);
                    if (ok && rlen > 4) parse_emv_response(resp, rlen);
                }
            }
        }

        if (emv_pan[0]) {
            emv_found_flag = true;
            haptic_success();
            Serial.printf("[NFC-EMV] PAN: %s EXP: %s Brand: %s\n", emv_pan, emv_expiry, emv_brand);
        } else {
            snprintf(emv_pan, sizeof(emv_pan), "READ FAILED");
            Serial.println("[NFC-EMV] No PAN found");
        }
        svc_mode = SVC_IDLE;
    }
#endif
}

static void do_start_scan(void) {
    stop_current();
#if defined(USING_ST25R3916)
    
    instance.powerControl(POWER_NFC, true);
    pinMode(NFC_CS, OUTPUT);
    digitalWrite(NFC_CS, HIGH);
    delay(50);
    if (beginNFC(on_tag_found, on_ndef)) {
        svc_mode = SVC_SCANNING;
        Serial.println("[NFC-SVC] Scanning started");
    } else {
        Serial.println("[NFC-SVC] Scanning start failed!");
    }
#endif
}

static void check_nfc_init(void) {
    
}

static void do_stop(void) {
    stop_current();
#if defined(USING_ST25R3916)
    
    deinitNFC();
#endif
}

static void do_save(void) {
    if (last_uid[0] == 0) return;
    if (saved_count >= MAX_SAVED_TAGS) {
        memmove(&saved_tags[0], &saved_tags[1], sizeof(SavedTag)*(MAX_SAVED_TAGS-1));
        saved_count = MAX_SAVED_TAGS - 1;
    }
    SavedTag &t = saved_tags[saved_count];
    memset(&t, 0, sizeof(SavedTag));
    snprintf(t.name, sizeof(t.name), "Tag-%s", last_uid+(strlen(last_uid)>8?strlen(last_uid)-8:0));
    memcpy(t.uid, last_nfcid, last_nfcid_len);
    t.uid_len = last_nfcid_len;
    t.sak = last_sak;
    memcpy(t.atqa, last_atqa, 2);
    strncpy(t.ndef, last_ndef_text, sizeof(t.ndef)-1);
    t.ndef[sizeof(t.ndef)-1] = 0;
    strncpy(t.type, last_tag_type, sizeof(t.type)-1);
    t.type[sizeof(t.type)-1] = 0;
    t.has_sectors = false;
    t.sectors_dumped = 0;
    saved_count++;
    save_tags();
    export_flipper(saved_tags[saved_count-1], saved_count-1);
    scan_sd_files();
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
    scan_sd_files();
}

static void do_emulate(void) {
    if (selected_tag < 0 || selected_tag >= saved_count) return;
    stop_current();
#if defined(USING_ST25R3916)
    instance.powerControl(POWER_NFC, true);
    pinMode(NFC_CS, OUTPUT);
    digitalWrite(NFC_CS, HIGH);
    delay(50);
    NFCReader.getRfalRf()->rfalInitialize(); delay(10);
    SavedTag &t = saved_tags[selected_tag];
    if (nfc_emu_start(t.uid, t.uid_len, t.ndef)) {
        svc_mode = SVC_EMULATING;
        haptic_buzz();
    }
#endif
}

static void do_dict_attack(void) {
    if (last_nfcid_len == 0) {
        snprintf(dict_status, sizeof(dict_status), "NO TAG - SCAN FIRST");
        return;
    }
    if (last_sak != 0x08 && last_sak != 0x18 && last_sak != 0x09) {
        snprintf(dict_status, sizeof(dict_status), "NOT MIFARE CLASSIC");
        return;
    }
    dict_total_sectors = (last_sak == 0x18) ? 40 : 16;
    dict_sectors_found = 0;
    dict_current_sector = 0;
    dict_current_key = 0;
    memset(dict_sector_found, 0, sizeof(dict_sector_found));
    snprintf(dict_status, sizeof(dict_status), "ATTACKING S:0 K:0/%d", dict_key_count);
    svc_mode = SVC_DICT_ATTACK;
    Serial.printf("[NFC-DICT] Starting attack on %s, %d sectors, %d keys\n",
        last_tag_type, dict_total_sectors, dict_key_count);
}

static void do_emv_read(void) {
    stop_current();
    emv_pan[0] = 0; emv_expiry[0] = 0; emv_cardholder[0] = 0; emv_brand[0] = 0;
    emv_found_flag = false;
#if defined(USING_ST25R3916)
    instance.powerControl(POWER_NFC, true);
    pinMode(NFC_CS, OUTPUT);
    digitalWrite(NFC_CS, HIGH);
    delay(50);
    if (beginNFC(on_tag_found, on_ndef)) {
        svc_mode = SVC_SCANNING; 
        svc_mode = SVC_EMV_READ;
        Serial.println("[NFC-EMV] EMV reading started");
    } else {
        Serial.println("[NFC-EMV] EMV reading failed to start");
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
void nfc_svc_request_dict_attack(void)   { cmd_dict_attack = true; }
void nfc_svc_request_emv_read(void)      { cmd_emv_read = true; }
void nfc_svc_request_load_from_sd(void)  { cmd_load_sd = true; }

bool nfc_svc_is_scanning(void) { return svc_mode == SVC_SCANNING; }
bool nfc_svc_is_emulating(void) { return svc_mode == SVC_EMULATING; }
bool nfc_svc_is_dict_attacking(void) { return svc_mode == SVC_DICT_ATTACK; }
bool nfc_svc_is_emv_reading(void) { return svc_mode == SVC_EMV_READ; }
const char* nfc_svc_last_uid(void) { return last_uid; }
const char* nfc_svc_last_ndef(void) { return last_ndef_text; }
const char* nfc_svc_last_type(void) { return last_tag_type; }
int nfc_svc_saved_count(void) { return saved_count; }
int nfc_svc_selected_idx(void) { return selected_tag; }
const char* nfc_svc_tag_name(int idx) { return (idx>=0&&idx<saved_count)?saved_tags[idx].name:""; }
bool nfc_svc_tag_detected(void) { bool r=tag_found; tag_found=false; return r; }
bool nfc_svc_tag_detected_web(void) { bool r=tag_found_web; tag_found_web=false; return r; }
bool nfc_svc_tag_detected_ble(void) { bool r=tag_found_ble; tag_found_ble=false; return r; }

int nfc_svc_dict_sectors_found(void) { return dict_sectors_found; }
int nfc_svc_dict_sectors_total(void) { return dict_total_sectors; }
int nfc_svc_dict_keys_tried(void) { return dict_current_sector * dict_key_count + dict_current_key; }
int nfc_svc_dict_keys_total(void) { return dict_total_sectors * dict_key_count; }
const char* nfc_svc_dict_status_text(void) { return dict_status; }

const char* nfc_svc_emv_pan(void) { return emv_pan; }
const char* nfc_svc_emv_expiry(void) { return emv_expiry; }
const char* nfc_svc_emv_cardholder(void) { return emv_cardholder; }
const char* nfc_svc_emv_brand(void) { return emv_brand; }
bool nfc_svc_emv_found(void) { return emv_found_flag; }

int nfc_svc_sd_file_count(void) { return sd_file_count; }
const char* nfc_svc_sd_file_name(int idx) { return (idx>=0&&idx<sd_file_count)?sd_files[idx]:""; }
