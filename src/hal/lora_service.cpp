#include "lora_service.h"
#include <RadioLib.h>
#include <LilyGoLib.h>
#include <LilyGo_LoRa_Pager.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#define ED25519_NO_SEED 1
#include <ed_25519.h>
#include <Preferences.h>
#include <FS.h>
#include <SD.h>
#include <cstdio>
#include <cstring>
#include "../config.h"
#include "haptic.h"

#define MC_FREQ         869.618f
#define MC_BW           62.5f
#define MC_SF           8
#define MC_CR           5
#define MC_SYNC_WORD    0x1424
#define MC_PREAMBLE     16
#define MC_PUBLIC_HASH  0x11
#define MC_TCXO_VOLTAGE 3.0f

static const uint8_t MC_PUBLIC_SECRET[32] = {
    0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
    0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define MC_TYPE_ADVERT   0x04
#define MC_TYPE_GRPTXT   0x05

static bool running = false;
static bool radio_ok = false;
static char node_name[32] = "";
static volatile bool new_msg_flag = false;
static volatile bool new_msg_web_flag = false;
static volatile bool new_msg_ble_flag = false;
static volatile bool rx_flag = false;

ICACHE_RAM_ATTR void lora_rx_isr(void) { rx_flag = true; }

#define MAX_MESSAGES 20
static MeshMsg messages[MAX_MESSAGES];
static int msg_count = 0;
static int msg_write = 0;

#define MAX_NODES 16
static MeshNode nodes[MAX_NODES];
static int node_count = 0;

#define DEDUP_SIZE 32
static uint32_t dedup_hashes[DEDUP_SIZE];
static int dedup_idx = 0;

static volatile bool tx_pending = false;
static uint8_t tx_buf[256];
static int tx_len = 0;

static char pending_msg[200] = "";
static volatile bool msg_send_requested = false;
static volatile bool advert_requested = false;
static volatile bool start_requested = false;
static volatile bool stop_requested = false;

static uint8_t ed25519_pk[32];
static uint8_t ed25519_sk[64];
static bool keypair_loaded = false;

static void load_or_create_keypair(void) {
    if (keypair_loaded) return;
    Preferences prefs;
    prefs.begin("meshcore", true);

    if (prefs.getBytesLength("sk") == 64 && prefs.getUChar("kv", 0) == 2) {
        prefs.getBytes("sk", ed25519_sk, 64);
        prefs.getBytes("pk", ed25519_pk, 32);
        Serial.println("[MC] Keypair loaded from NVS");
    } else {
        prefs.end();

        uint8_t seed[32];
        esp_fill_random(seed, 32);

        ed25519_create_keypair(ed25519_pk, ed25519_sk, seed);
        prefs.begin("meshcore", false);
        prefs.putBytes("sk", ed25519_sk, 64);
        prefs.putBytes("pk", ed25519_pk, 32);
        prefs.putUChar("kv", 2);
        Serial.println("[MC] New keypair generated (Orlp ed25519)");
    }
    prefs.end();
    keypair_loaded = true;
    Serial.printf("[MC] PubKey: %02X%02X%02X%02X...\n",
        ed25519_pk[0], ed25519_pk[1], ed25519_pk[2], ed25519_pk[3]);
}

static void aes_ecb_decrypt(const uint8_t *key, const uint8_t *in, uint8_t *out, int len) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, 128);
    for (int i = 0; i < len; i += 16)
        mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, in + i, out + i);
    mbedtls_aes_free(&ctx);
}

static void aes_ecb_encrypt(const uint8_t *key, const uint8_t *in, uint8_t *out, int len) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 128);
    for (int i = 0; i < len; i += 16)
        mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, in + i, out + i);
    mbedtls_aes_free(&ctx);
}

static void hmac_sha256(const uint8_t *key, int kl, const uint8_t *data, int dl, uint8_t *out) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, key, kl);
    mbedtls_md_hmac_update(&ctx, data, dl);
    mbedtls_md_hmac_finish(&ctx, out);
    mbedtls_md_free(&ctx);
}

static uint32_t dedup_times[DEDUP_SIZE];

static bool is_duplicate_hp(uint8_t header, const uint8_t *payload, int plen) {
    uint32_t h = 0x811c9dc5;
    h ^= header; h *= 0x01000193;
    for (int i = 0; i < plen; i++) { h ^= payload[i]; h *= 0x01000193; }

    uint32_t now = millis();
    for (int i = 0; i < DEDUP_SIZE; i++) {
        if (dedup_hashes[i] == h && (now - dedup_times[i]) < 30000) return true;
    }
    dedup_hashes[dedup_idx] = h;
    dedup_times[dedup_idx] = now;
    dedup_idx = (dedup_idx + 1) % DEDUP_SIZE;
    return false;
}

static void dedup_preseed(const uint8_t *pkt, int len) {
    if (len < 3) return;
    is_duplicate_hp(pkt[0], &pkt[2], len - 2);
}

static uint32_t get_unix_timestamp(void);

static void store_message(const char *ch, const char *txt, float rssi, int hops) {
    MeshMsg &m = messages[msg_write];
    strncpy(m.channel, ch, sizeof(m.channel) - 1);
    strncpy(m.text, txt, sizeof(m.text) - 1);
    m.rssi = rssi; m.hops = hops;
    m.timestamp = get_unix_timestamp();
    msg_write = (msg_write + 1) % MAX_MESSAGES;
    if (msg_count < MAX_MESSAGES) msg_count++;
    new_msg_flag = true;
    new_msg_web_flag = true;
    new_msg_ble_flag = true;
    haptic_buzz();
    Serial.printf("[MC] MSG [%s] %dhop: %s\n", ch, hops, txt);

    File f = SD.open("/meshcore_log.txt", FILE_APPEND);
    if (f) {
        f.printf("%lu|%s|%d|%.0f|%s\n", (unsigned long)m.timestamp, ch, hops, rssi, txt);
        f.close();
    }
}

static void decode_packet(uint8_t *data, int len, float rssi, float snr) {
    if (len < 3) return;

    uint8_t header = data[0];
    uint8_t route = header & 0x03;
    uint8_t ptype = (header >> 2) & 0x0F;

    int offset = 1;
    if (route == 0 || route == 3) offset += 4;

    if (offset >= len) return;

    uint8_t path_byte = data[offset];
    int hash_size = ((path_byte >> 6) & 0x03) + 1;
    int hash_count = path_byte & 0x3F;
    offset += 1;
    offset += hash_count * hash_size;

    int hops = hash_count;
    uint8_t *payload = &data[offset];
    int plen = len - offset;
    if (plen <= 0) return;

    if (is_duplicate_hp(header, payload, plen)) return;

    Serial.printf("[MC] RX: %d bytes, type=%d, hops=%d, RSSI=%.0f\n", len, ptype, hops, rssi);

    if (ptype == MC_TYPE_GRPTXT && plen >= 4) {
        uint8_t ch_hash = payload[0];
        uint8_t mac[2] = { payload[1], payload[2] };
        uint8_t *ct = &payload[3];
        int ct_len = plen - 3;
        if (ct_len <= 0) return;

        if (ch_hash != MC_PUBLIC_HASH) {
            Serial.printf("[MC] Unknown ch 0x%02X\n", ch_hash);
            return;
        }

        uint8_t hmac_out[32];
        hmac_sha256(MC_PUBLIC_SECRET, 32, ct, ct_len, hmac_out);
        if (hmac_out[0] != mac[0] || hmac_out[1] != mac[1]) {
            Serial.println("[MC] MAC fail");
            return;
        }

        int pad = (16 - ct_len % 16) % 16;
        int dec_len = ct_len + pad;
        if (dec_len > 256) return;
        uint8_t padded[256], plain[256];
        memcpy(padded, ct, ct_len);
        memset(padded + ct_len, 0, pad);
        aes_ecb_decrypt(MC_PUBLIC_SECRET, padded, plain, dec_len);

        char *msg = (char*)&plain[5];
        int ml = 0;
        for (int i = 5; i < dec_len && plain[i]; i++) ml++;
        if (ml > 199) ml = 199;
        char buf[200];
        memcpy(buf, msg, ml); buf[ml] = 0;
        store_message("public", buf, rssi, hops);

    } else if (ptype == MC_TYPE_ADVERT && plen >= 100) {
        uint8_t *pubkey = payload;
        uint8_t *appdata = payload + 100;
        int alen = plen - 100;
        if (alen < 1) return;

        uint8_t flags = appdata[0];
        int off = 1;
        float lat = 0, lon = 0;
        if ((flags & 0x10) && alen >= off + 8) {
            int32_t li, lo;
            memcpy(&li, appdata + off, 4);
            memcpy(&lo, appdata + off + 4, 4);
            lat = li / 1e6f; lon = lo / 1e6f;
            off += 8;
        }
        char name[32] = "";
        for (int i = off; i < alen && appdata[i] && (i-off) < 30; i++)
            name[i-off] = appdata[i];

        const char *types[] = {"?","Client","Repeater","Room"};
        int nt = flags & 0x0F; if (nt > 3) nt = 0;

        bool found = false;
        for (int i = 0; i < node_count; i++) {
            if (memcmp(nodes[i].pubkey, pubkey, 32) == 0) {
                strncpy(nodes[i].name, name, 31);
                nodes[i].lat = lat; nodes[i].lon = lon;
                nodes[i].rssi = rssi; nodes[i].snr = snr;
                found = true; break;
            }
        }
        if (!found && node_count < MAX_NODES) {
            MeshNode &n = nodes[node_count++];
            strncpy(n.name, name, 31);
            strncpy(n.type, types[nt], 15);
            n.lat = lat; n.lon = lon;
            n.rssi = rssi; n.snr = snr;
            memcpy(n.pubkey, pubkey, 32);
        }
        Serial.printf("[MC] ADVERT: %s [%s]\n", name, types[nt]);
    }
}

static uint32_t get_unix_timestamp(void) {
    time_t now; time(&now);
    if (now > 1700000000) return (uint32_t)now;

    RTC_DateTime dt = instance.rtc.getDateTime();
    struct tm t = {};
    t.tm_year = dt.getYear() - 1900;
    t.tm_mon = dt.getMonth() - 1;
    t.tm_mday = dt.getDay();
    t.tm_hour = dt.getHour();
    t.tm_min = dt.getMinute();
    t.tm_sec = dt.getSecond();
    return (uint32_t)mktime(&t);
}

static int build_group_text(const char *text, uint8_t *out) {
    uint32_t ts = get_unix_timestamp();
    char msg[180];
    snprintf(msg, sizeof(msg), "%s: %s", node_name, text);
    int ml = strlen(msg) + 1;

    uint8_t pt[256];
    memcpy(pt, &ts, 4);
    pt[4] = 0x00;
    memcpy(pt + 5, msg, ml);
    int ptl = 5 + ml;
    int pad = (16 - ptl % 16) % 16;
    memset(pt + ptl, 0, pad);
    ptl += pad;

    uint8_t ct[256];
    aes_ecb_encrypt(MC_PUBLIC_SECRET, pt, ct, ptl);

    uint8_t hm[32];
    hmac_sha256(MC_PUBLIC_SECRET, 32, ct, ptl, hm);

    out[0] = 0x15;
    out[1] = 0x00;
    out[2] = MC_PUBLIC_HASH;
    out[3] = hm[0]; out[4] = hm[1];
    memcpy(out + 5, ct, ptl);
    return 5 + ptl;
}

static bool configure_radio(void) {

    int err = radio.begin(MC_FREQ, MC_BW, MC_SF, MC_CR,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                          14, MC_PREAMBLE, 1.6f);

    if (err == RADIOLIB_ERR_SPI_CMD_FAILED || err == RADIOLIB_ERR_SPI_CMD_INVALID) {
        Serial.println("[MC] Retry with TCXO=0");
        err = radio.begin(MC_FREQ, MC_BW, MC_SF, MC_CR,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                          14, MC_PREAMBLE, 0.0f);
    }

    if (err != RADIOLIB_ERR_NONE) {
        Serial.printf("[MC] Retry with TCXO=3.0 (prev err=%d)\n", err);
        err = radio.begin(MC_FREQ, MC_BW, MC_SF, MC_CR,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                          14, MC_PREAMBLE, 3.0f);
    }

    if (err != RADIOLIB_ERR_NONE) {
        Serial.printf("[MC] Radio begin failed: %d\n", err);
        return false;
    }

    radio.setCRC(1);

    radio.setDio2AsRfSwitch(true);

    radio.setRxBoostedGainMode(true);

    Serial.printf("[MC] Radio OK: %.3fMHz SF%d BW%.1fk CR%d\n", MC_FREQ, MC_SF, MC_BW, MC_CR);
    return true;
}

static void do_start(void);
static void do_stop(void);
static void do_send_advert(void);
static void check_lora_init(void);

static void generate_node_name(void) {
    if (node_name[0]) return;

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);

    const char charset[] = "0123456789abcdefghjkmnpqrstuvwxyz";
    uint32_t seed = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
    char suffix[6];
    for (int i = 0; i < 5; i++) {
        suffix[i] = charset[seed % 32];
        seed /= 32;
    }
    suffix[5] = 0;
    snprintf(node_name, sizeof(node_name), "SCRW-%s", suffix);
}

void lora_service_init(void) {
    generate_node_name();
    memset(dedup_hashes, 0, sizeof(dedup_hashes));
    msg_count = 0; msg_write = 0; node_count = 0;
    Serial.printf("[MC] Node name: %s\n", node_name);
    lora_svc_load_history();
}

void lora_service_loop(void) {

    if (start_requested) { start_requested = false; do_start(); }
    if (stop_requested)  { stop_requested = false; do_stop(); }

    check_lora_init();

    if (!running || !radio_ok) return;

    if (msg_send_requested) {
        msg_send_requested = false;
        tx_len = build_group_text(pending_msg, tx_buf);
        tx_pending = true;
        char own[200]; snprintf(own, sizeof(own), "%s: %s", node_name, pending_msg);
        store_message("public", own, 0, 0);
    }
    if (advert_requested) {
        advert_requested = false;
        do_send_advert();
    }

    if (tx_pending) {
        tx_pending = false;

        dedup_preseed(tx_buf, tx_len);

        Serial.printf("[MC] TX %d bytes: ", tx_len);
        for (int i = 0; i < tx_len; i++) Serial.printf("%02X", tx_buf[i]);
        Serial.println();

        radio.clearPacketReceivedAction();
        int err = radio.transmit(tx_buf, tx_len);
        Serial.printf("[MC] TX: %d\n", err);

        radio.setFrequency(MC_FREQ);
        radio.setBandwidth(MC_BW);
        radio.setSpreadingFactor(MC_SF);
        radio.setCodingRate(MC_CR);
        radio.setSyncWord(RADIOLIB_SX126X_SYNC_WORD_PRIVATE);
        radio.setPreambleLength(MC_PREAMBLE);
        radio.setCRC(1);
        radio.setDio2AsRfSwitch(true);
        radio.setPacketReceivedAction(lora_rx_isr);
        radio.startReceive();
    }

    if (rx_flag) {
        rx_flag = false;

        uint8_t buf[256];
        int len = radio.getPacketLength();
        if (len > 0 && len <= 256) {
            int err = radio.readData(buf, len);
            if (err == RADIOLIB_ERR_NONE) {
                float rssi = radio.getRSSI();
                float snr = radio.getSNR();
                decode_packet(buf, len, rssi, snr);
            }
        }

        radio.startReceive();
    }
}

static uint8_t lora_init_stage = 0;
static uint32_t lora_init_time = 0;

static void do_start(void) {
    if (running) return;
    load_or_create_keypair();
    instance.powerControl(POWER_RADIO, true);
    lora_init_stage = 1;
    lora_init_time = millis();
}

static void check_lora_init(void) {
    if (lora_init_stage == 0) return;
    if (millis() - lora_init_time < 60) return;

    lora_init_stage = 0;
    if (!configure_radio()) {
        Serial.println("[MC] Config failed");
        return;
    }

    radio.setPacketReceivedAction(lora_rx_isr);

    int err = radio.startReceive();
    if (err == RADIOLIB_ERR_NONE) {
        radio_ok = true;
        running = true;
        Serial.println("[MC] MeshCore RX started!");
    } else {
        Serial.printf("[MC] startReceive err: %d\n", err);
    }
}

static void do_stop(void) {
    if (!running) return;
    radio.standby();
    instance.powerControl(POWER_RADIO, false);
    running = false; radio_ok = false;
}

void lora_svc_start(void) { start_requested = true; }
void lora_svc_stop(void)  { stop_requested = true; }

void lora_svc_send_message(const char *text) {
    if (!running) return;
    strncpy(pending_msg, text, sizeof(pending_msg) - 1);
    pending_msg[sizeof(pending_msg) - 1] = 0;
    msg_send_requested = true;
}

static void do_send_advert(void) {
    if (!running) return;
    load_or_create_keypair();

    uint8_t appdata[64];
    int alen = 0;
    uint8_t flags = 0x01;
    flags |= 0x80;

    float lat = 0, lon = 0;
    if (instance.gps.location.isValid()) {
        lat = instance.gps.location.lat();
        lon = instance.gps.location.lng();
        flags |= 0x10;
    }
    appdata[alen++] = flags;
    if (flags & 0x10) {
        int32_t li = (int32_t)(lat * 1e6), lo = (int32_t)(lon * 1e6);
        memcpy(appdata + alen, &li, 4); alen += 4;
        memcpy(appdata + alen, &lo, 4); alen += 4;
    }
    int nl = strlen(node_name);
    memcpy(appdata + alen, node_name, nl); alen += nl;
    appdata[alen++] = 0;

    uint32_t ts = get_unix_timestamp();
    uint8_t ts_bytes[4];
    memcpy(ts_bytes, &ts, 4);

    uint8_t sign_data[128];
    memcpy(sign_data, ed25519_pk, 32);
    memcpy(sign_data + 32, ts_bytes, 4);
    memcpy(sign_data + 36, appdata, alen);
    int sign_len = 36 + alen;

    uint8_t signature[64];

    ed25519_sign(signature, sign_data, sign_len, ed25519_pk, ed25519_sk);

    uint8_t pkt[180];
    int pos = 0;
    pkt[pos++] = 0x11;
    pkt[pos++] = 0x00;
    memcpy(pkt + pos, ed25519_pk, 32); pos += 32;
    memcpy(pkt + pos, ts_bytes, 4); pos += 4;
    memcpy(pkt + pos, signature, 64); pos += 64;
    memcpy(pkt + pos, appdata, alen); pos += alen;

    memcpy(tx_buf, pkt, pos);
    tx_len = pos;
    tx_pending = true;
    Serial.printf("[MC] Advert queued: %s (%d bytes, signed)\n", node_name, pos);
}

void lora_svc_send_advert(void) { advert_requested = true; }

void lora_svc_set_node_name(const char *name) {
    strncpy(node_name, name, sizeof(node_name) - 1);
}

bool lora_svc_is_running(void) { return running; }
bool lora_svc_has_new_message(void) { bool r = new_msg_flag; new_msg_flag = false; return r; }
bool lora_svc_has_new_message_web(void) { bool r = new_msg_web_flag; new_msg_web_flag = false; return r; }
bool lora_svc_has_new_message_ble(void) { bool r = new_msg_ble_flag; new_msg_ble_flag = false; return r; }
const MeshMsg* lora_svc_last_message(void) {
    if (msg_count == 0) return nullptr;
    return &messages[(msg_write - 1 + MAX_MESSAGES) % MAX_MESSAGES];
}
const MeshMsg* lora_svc_get_message(int idx) {

    if (idx < 0 || idx >= msg_count) return nullptr;
    int pos = (msg_write - 1 - idx + MAX_MESSAGES * 2) % MAX_MESSAGES;
    return &messages[pos];
}
int lora_svc_message_count(void) { return msg_count; }
int lora_svc_node_count(void) { return node_count; }

void lora_svc_save_history(void) {

}

void lora_svc_load_history(void) {
    File f = SD.open("/meshcore_log.txt", FILE_READ);
    if (!f) { Serial.println("[MC] No history on SD"); return; }

    int loaded = 0;
    while (f.available() && loaded < MAX_MESSAGES) {
        String line = f.readStringUntil('\n');
        if (line.length() < 5) continue;

        int p1 = line.indexOf('|');
        int p2 = line.indexOf('|', p1 + 1);
        int p3 = line.indexOf('|', p2 + 1);
        int p4 = line.indexOf('|', p3 + 1);
        if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) continue;

        MeshMsg &m = messages[msg_write];
        m.timestamp = line.substring(0, p1).toInt();
        strncpy(m.channel, line.substring(p1+1, p2).c_str(), sizeof(m.channel)-1);
        m.hops = line.substring(p2+1, p3).toInt();
        m.rssi = line.substring(p3+1, p4).toFloat();
        strncpy(m.text, line.substring(p4+1).c_str(), sizeof(m.text)-1);

        msg_write = (msg_write + 1) % MAX_MESSAGES;
        if (msg_count < MAX_MESSAGES) msg_count++;
        loaded++;
    }
    f.close();
    Serial.printf("[MC] Loaded %d messages from SD\n", loaded);
}
