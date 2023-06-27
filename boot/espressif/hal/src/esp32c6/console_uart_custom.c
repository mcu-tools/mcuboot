/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_rom_uart.h>
#include <hal/uart_ll.h>
#include <soc/uart_periph.h>

#if CONFIG_ESP_CONSOLE_UART_CUSTOM
static uart_dev_t *alt_console_uart_dev = (CONFIG_ESP_CONSOLE_UART_NUM == 0) ?
                                          &UART0 :
                                          &UART1;

void IRAM_ATTR esp_rom_uart_putc(char c)
{
    while (uart_ll_get_txfifo_len(alt_console_uart_dev) == 0);
    uart_ll_write_txfifo(alt_console_uart_dev, (const uint8_t *) &c, 1);
}
#endif

