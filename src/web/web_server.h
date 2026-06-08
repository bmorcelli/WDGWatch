#pragma once

void web_server_init(void);
void web_server_loop(void);
void web_server_stop(void);
bool web_server_is_active(void);
const char* web_server_get_ip(void);

bool web_nfc_check_scan(void);
bool web_nfc_check_save(void);
bool web_nfc_check_stop(void);
bool web_nfc_check_export(void);
int  web_nfc_check_delete(void);

void web_push_nfc_tag(const char *uid, const char *ndef);
void web_push_log(const char *msg);
