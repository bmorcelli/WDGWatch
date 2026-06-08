#pragma once
#include <Arduino.h>

typedef void (*api_event_cb_t)(const char *json_event);

void api_init(void);
char* api_handle_command(const char *json_cmd);
void api_set_event_callback(api_event_cb_t cb);
void api_loop(void);
