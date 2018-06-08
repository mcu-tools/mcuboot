/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <uart.h>
#include <assert.h>
#include <string.h>
#include <zephyr.h>

#ifdef CONFIG_UART_CONSOLE
#error Zephyr UART console must been disabled if serial_adapter module is used.
#endif

/** @brief Console input representation
 *
 * This struct is used to represent an input line from a serial interface.
 */
struct line_input {
    /** FIFO uses first 4 bytes itself, reserve space */
    int _unused;

    int len;
    /** Buffer where the input line is recorded */
    char line[CONFIG_BOOT_MAX_LINE_INPUT_LEN];
};

static struct device *uart_dev;
static struct line_input line_bufs[2];

static K_FIFO_DEFINE(free_queue);
static K_FIFO_DEFINE(used_queue);

static struct k_fifo *avail_queue;
static struct k_fifo *lines_queue;
static u16_t cur;

static int boot_uart_fifo_getline(char **line);
static int boot_uart_fifo_init(void);

int
console_out(int c)
{
    uart_poll_out(uart_dev, c);

    return c;
}

void
console_write(const char *str, int cnt)
{
    int i;

    for (i = 0; i < cnt; i++) {
        if (console_out((int)str[i]) == EOF) {
            break;
        }
    }
}

int
console_read(char *str, int str_size, int *newline)
{
    char * line;
    int len;

    len = boot_uart_fifo_getline(&line);

    if (line == NULL) {
        *newline = 0;
        return 0;
    }

    if (len > str_size - 1) {
        len = str_size - 1;
    }

    memcpy(str, line, len);
    str[len] = '\0';
    *newline = 1;
    return len + 1;
}

int
boot_console_init(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(line_bufs); i++) {
        k_fifo_put(&free_queue, &line_bufs[i]);
    }

    /* Zephyr UART handler takes an empty buffer from free_queue,
     * stores UART input in it until EOL, and then puts it into
     * used_queue.
     */
    avail_queue = &free_queue;
    lines_queue = &used_queue;

    return boot_uart_fifo_init();
}

static void
boot_uart_fifo_callback(struct device *dev)
{
    static struct line_input *cmd;
    u8_t byte;
    int rx;

    while (uart_irq_update(uart_dev) &&
           uart_irq_rx_ready(uart_dev)) {

        rx = uart_fifo_read(uart_dev, &byte, 1);

        if (!cmd) {
            cmd = k_fifo_get(avail_queue, K_NO_WAIT);
            if (!cmd) {
                return;
            }
        }

        if (cur < CONFIG_BOOT_MAX_LINE_INPUT_LEN) {
            cmd->line[cur++] = byte;
        }

        if (byte ==  '\n') {
            cmd->len = cur;
            k_fifo_put(lines_queue, cmd);
            cur = 0;
        }

    }
}

static int
boot_uart_fifo_getline(char **line)
{
    static struct line_input *cmd;

    /* Recycle cmd buffer returned previous time */
    if (cmd != NULL) {
        k_fifo_put(&free_queue, cmd);
    }

    cmd = k_fifo_get(&used_queue, K_NO_WAIT);

    if (cmd == NULL) {
        *line = NULL;
        return 0;
    }

    *line = cmd->line;
    return cmd->len;
}

static int
boot_uart_fifo_init(void)
{
    uart_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
    u8_t c;

    if (!uart_dev) {
        return (-1);
    }

    uart_irq_callback_set(uart_dev, boot_uart_fifo_callback);

    /* Drain the fifo */
    while (uart_irq_rx_ready(uart_dev)) {
        uart_fifo_read(uart_dev, &c, 1);
    }

    cur = 0;

    uart_irq_rx_enable(uart_dev);

    return 0;
}
