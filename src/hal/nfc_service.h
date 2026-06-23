#pragma once
#include <LilyGoLib.h>

void nfc_service_init(void);
void nfc_service_loop(void);

void nfc_svc_request_scan(void);
void nfc_svc_request_stop(void);
void nfc_svc_request_save(void);
void nfc_svc_request_delete(int idx);
void nfc_svc_request_export(void);
void nfc_svc_request_select_next(void);
void nfc_svc_request_emulate(void);
void nfc_svc_request_dict_attack(void);
void nfc_svc_request_emv_read(void);
void nfc_svc_request_load_from_sd(void);

bool nfc_svc_is_scanning(void);
bool nfc_svc_is_emulating(void);
bool nfc_svc_is_dict_attacking(void);
bool nfc_svc_is_emv_reading(void);
const char* nfc_svc_last_uid(void);
const char* nfc_svc_last_ndef(void);
const char* nfc_svc_last_type(void);
int nfc_svc_saved_count(void);
int nfc_svc_selected_idx(void);
const char* nfc_svc_tag_name(int idx);
bool nfc_svc_tag_detected(void);
bool nfc_svc_tag_detected_web(void);
bool nfc_svc_tag_detected_ble(void);

int nfc_svc_dict_sectors_found(void);
int nfc_svc_dict_sectors_total(void);
int nfc_svc_dict_keys_tried(void);
int nfc_svc_dict_keys_total(void);
const char* nfc_svc_dict_status_text(void);

const char* nfc_svc_emv_pan(void);
const char* nfc_svc_emv_expiry(void);
const char* nfc_svc_emv_cardholder(void);
const char* nfc_svc_emv_brand(void);
bool nfc_svc_emv_found(void);

int nfc_svc_sd_file_count(void);
const char* nfc_svc_sd_file_name(int idx);
