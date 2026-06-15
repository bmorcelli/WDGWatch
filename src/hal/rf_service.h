#pragma once
#include <stdint.h>
#include <stdbool.h>

void rf_service_init(void);
void rf_service_loop(void);

bool rf_radio_wake(float freq_mhz);
void rf_radio_sleep(void);

bool rf_jammer_start(uint32_t freq_hz);
void rf_jammer_stop(void);
bool rf_jammer_is_active(void);
uint32_t rf_jammer_get_freq(void);

void rf_tesla_send(void);
bool rf_tesla_is_sending(void);
