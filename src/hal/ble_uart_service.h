#pragma once

void ble_uart_init(void);
void ble_uart_stop(void);
void ble_uart_loop(void);
bool ble_uart_is_active(void);
bool ble_uart_is_connected(void);
void ble_uart_send(const char *data);
const char* ble_uart_get_pin(void);
