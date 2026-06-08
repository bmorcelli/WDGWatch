#include "nfc_emu.h"

#if defined(ARDUINO) && defined(USING_ST25R3916)

#define T2T_CMD_READ       0x30
#define T2T_CMD_WRITE      0xA2
#define T2T_CMD_SECTOR_SEL 0xC2
#define T2T_CMD_HALT       0x50

#define T2T_NUM_PAGES   64
#define T2T_PAGE_SIZE   4
#define T2T_MEM_SIZE    (T2T_NUM_PAGES * T2T_PAGE_SIZE)

static uint8_t t2t_mem[T2T_MEM_SIZE];
static uint8_t rx_buf[64];
static uint16_t rx_len = 0;
static bool emu_active = false;
static bool reader_detected = false;
static bool using_discover_fallback = false;

static const uint8_t T2T_ACK = 0x0A;
static const uint8_t T2T_NAK = 0x00;

static void build_t2t_memory(const uint8_t *uid, uint8_t uid_len, const char *ndef_text) {
    memset(t2t_mem, 0, T2T_MEM_SIZE);

    if (uid_len >= 7) {
        t2t_mem[0] = uid[0]; t2t_mem[1] = uid[1]; t2t_mem[2] = uid[2];
        t2t_mem[3] = uid[0] ^ uid[1] ^ uid[2] ^ 0x88;
        t2t_mem[4] = uid[3]; t2t_mem[5] = uid[4]; t2t_mem[6] = uid[5]; t2t_mem[7] = uid[6];
    } else {
        memcpy(t2t_mem, uid, uid_len);
        t2t_mem[uid_len] = 0x00;
    }

    t2t_mem[8]  = uid_len >= 7 ? (uid[3] ^ uid[4] ^ uid[5] ^ uid[6]) : 0x00;
    t2t_mem[9]  = 0x48;
    t2t_mem[10] = 0x00;
    t2t_mem[11] = 0x00;

    t2t_mem[12] = 0xE1;
    t2t_mem[13] = 0x10;
    t2t_mem[14] = (T2T_NUM_PAGES - 4) / 2;
    t2t_mem[15] = 0x00;

    uint8_t *p = &t2t_mem[16];
    int max_ndef = T2T_MEM_SIZE - 16 - 4;

    if (ndef_text && ndef_text[0]) {
        int text_len = strlen(ndef_text);
        if (text_len > max_ndef - 12) text_len = max_ndef - 12;

        uint8_t ndef_rec[256];
        int pos = 0;

        ndef_rec[pos++] = 0xD1;
        ndef_rec[pos++] = 0x01;
        ndef_rec[pos++] = text_len + 3;
        ndef_rec[pos++] = 'T';

        ndef_rec[pos++] = 0x02;
        ndef_rec[pos++] = 'e';
        ndef_rec[pos++] = 'n';
        memcpy(&ndef_rec[pos], ndef_text, text_len);
        pos += text_len;

        int ndef_total = pos;

        *p++ = 0x03;
        if (ndef_total < 0xFF) {
            *p++ = ndef_total;
        } else {
            *p++ = 0xFF;
            *p++ = (ndef_total >> 8) & 0xFF;
            *p++ = ndef_total & 0xFF;
        }
        memcpy(p, ndef_rec, ndef_total);
        p += ndef_total;

        *p++ = 0xFE;
    } else {

        *p++ = 0x03;
        *p++ = 0x00;
        *p++ = 0xFE;
    }
}

bool nfc_emu_start(const uint8_t *uid, uint8_t uid_len, const char *ndef_text) {

    build_t2t_memory(uid, uid_len, ndef_text);

    rfalLmConfPA confA;
    memset(&confA, 0, sizeof(confA));
    memcpy(confA.nfcid, uid, uid_len);
    confA.nfcidLen = (uid_len <= 4) ? RFAL_LM_NFCID_LEN_04 : RFAL_LM_NFCID_LEN_07;
    confA.SENS_RES[0] = 0x44;
    confA.SENS_RES[1] = 0x00;
    confA.SEL_RES = 0x00;

    rx_len = 0;
    reader_detected = false;

    ReturnCode err = NFCReader.getRfalRf()->rfalListenStart(
        RFAL_LM_MASK_NFCA,
        &confA, nullptr, nullptr,
        rx_buf, sizeof(rx_buf), &rx_len
    );

    if (err == ST_ERR_NONE) {
        emu_active = true;
        Serial.printf("[NFC-EMU] OK! UID=%02X:%02X:%02X:%02X\n",
            uid[0], uid[1], uid[2], uid[3]);
        return true;
    }

    Serial.printf("[NFC-EMU] ListenStart err=%d SetMode was %d\n", err,
        NFCReader.getRfalRf()->rfalSetMode(RFAL_MODE_LISTEN_NFCA, RFAL_BR_106, RFAL_BR_106));
    return false;
}

bool nfc_emu_loop(void) {
    if (!emu_active) return false;

#ifdef USE_RFAL_FORK
    NFCReader.getRfalRf()->rfalRunListenModeWorker();
#else
    emu_active = false;
    return false;
#endif

    bool dataFlag = false;
    rfalBitRate lastBR;
    rfalLmState state = NFCReader.getRfalRf()->rfalListenGetState(&dataFlag, &lastBR);

    if (dataFlag && rx_len > 0) {

        reader_detected = true;
        uint8_t cmd = rx_buf[0];
        Serial.printf("[NFC-EMU] CMD: 0x%02X len=%d\n", cmd, rx_len);

        uint8_t tx_buf[18];
        uint16_t tx_len = 0;

        switch (cmd) {
            case T2T_CMD_READ: {

                uint8_t page = rx_buf[1];
                Serial.printf("[NFC-EMU] READ page %d\n", page);

                for (int i = 0; i < 16; i++) {
                    int addr = (page * T2T_PAGE_SIZE + i) % T2T_MEM_SIZE;
                    tx_buf[i] = t2t_mem[addr];
                }
                tx_len = 16;
                break;
            }

            case T2T_CMD_WRITE: {

                uint8_t page = rx_buf[1];
                if (page >= 4 && page < T2T_NUM_PAGES) {
                    memcpy(&t2t_mem[page * T2T_PAGE_SIZE], &rx_buf[2], 4);
                    tx_buf[0] = T2T_ACK;
                    tx_len = 1;
                    Serial.printf("[NFC-EMU] WRITE page %d OK\n", page);
                } else {
                    tx_buf[0] = T2T_NAK;
                    tx_len = 1;
                }
                break;
            }

            case T2T_CMD_HALT:

                Serial.println("[NFC-EMU] HALT received");
                NFCReader.getRfalRf()->rfalListenSetState(RFAL_LM_STATE_IDLE);
                rx_len = 0;
                return true;

            default:

                tx_buf[0] = T2T_NAK;
                tx_len = 1;
                break;
        }

        if (tx_len > 0) {
            uint16_t actLen = 0;
            ReturnCode err = NFCReader.getRfalRf()->rfalTransceiveBlockingTxRx(
                tx_buf, tx_len,
                rx_buf, sizeof(rx_buf), &actLen,
                RFAL_TXRX_FLAGS_DEFAULT,
                1000
            );

            if (err == ST_ERR_NONE) {
                rx_len = actLen;
                Serial.printf("[NFC-EMU] Response sent OK, got %d bytes back\n", actLen);
            } else {
                Serial.printf("[NFC-EMU] TX err: %d\n", err);
                rx_len = 0;

                NFCReader.getRfalRf()->rfalListenSetState(RFAL_LM_STATE_IDLE);
            }
        }

        return true;
    }

    if (state == RFAL_LM_STATE_NOT_INIT) {
        Serial.println("[NFC-EMU] State NOT_INIT - restarting");

        NFCReader.getRfalRf()->rfalListenSetState(RFAL_LM_STATE_POWER_OFF);
    }

    return false;
}

void nfc_emu_stop(void) {
    if (emu_active) {
        NFCReader.getRfalRf()->rfalListenStop();
        emu_active = false;
        Serial.println("[NFC-EMU] Stopped");
    }
}

bool nfc_emu_is_active(void) {
    return emu_active;
}

#endif
