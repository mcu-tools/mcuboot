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
#include <zephyr/drivers/uart.h>
#include <assert.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "bootutil/bootutil_log.h"
#include <zephyr/usb/usb_device.h>

#if defined(CONFIG_BOOT_SERIAL_UART) && defined(CONFIG_UART_CONSOLE)
#error Zephyr UART console must been disabled if serial_adapter module is used.
#endif

BOOT_LOG_MODULE_REGISTER(serial_adapter);

/** @brief Console input representation
 *
 * This struct is used to represent an input line from a serial interface.
 */
struct line_input {
	/** Required to use sys_slist */
	sys_snode_t node;

	int len;
	/** Buffer where the input line is recorded */
	char line[CONFIG_BOOT_MAX_LINE_INPUT_LEN];
};

static struct device const *uart_dev;
static struct line_input line_bufs[2];

static sys_slist_t avail_queue;
static sys_slist_t lines_queue;

static uint16_t cur;

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
	char *line;
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

	/* Zephyr UART handler takes an empty buffer from avail_queue,
	 * stores UART input in it until EOL, and then puts it into
	 * lines_queue.
	 */
	sys_slist_init(&avail_queue);
	sys_slist_init(&lines_queue);

	for (i = 0; i < ARRAY_SIZE(line_bufs); i++) {
		sys_slist_append(&avail_queue, &line_bufs[i].node);
	}

	return boot_uart_fifo_init();
}

static void
boot_uart_fifo_callback(const struct device *dev, void *user_data)
{
	static struct line_input *cmd;
	uint8_t byte;
	int rx;

	uart_irq_update(uart_dev);

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	while (true) {
		rx = uart_fifo_read(uart_dev, &byte, 1);
		if (rx != 1) {
			break;
		}

		if (!cmd) {
			sys_snode_t *node;

			node = sys_slist_get(&avail_queue);
			if (!node) {
				BOOT_LOG_ERR("Not enough memory to store"
					     " incoming data!");
				return;
			}
			cmd = CONTAINER_OF(node, struct line_input, node);
		}

		if (cur < CONFIG_BOOT_MAX_LINE_INPUT_LEN) {
			cmd->line[cur++] = byte;
		}

		if (byte ==  '\n') {
			cmd->len = cur;
			sys_slist_append(&lines_queue, &cmd->node);
			cur = 0;
			cmd = NULL;
		}
	}
}

static int
boot_uart_fifo_getline(char **line)
{
	static struct line_input *cmd;
	sys_snode_t *node;
	int key;

	key = irq_lock();
	/* Recycle cmd buffer returned previous time */
	if (cmd != NULL) {
		if (sys_slist_peek_tail(&avail_queue) != &cmd->node) {
			sys_slist_append(&avail_queue, &cmd->node);
		}
	}

	node = sys_slist_get(&lines_queue);
	irq_unlock(key);

	if (node == NULL) {
		cmd = NULL;
		*line = NULL;

		return 0;
	}

	cmd = CONTAINER_OF(node, struct line_input, node);
	*line = cmd->line;
	return cmd->len;
}

static int
boot_uart_fifo_init(void)
{
#ifdef CONFIG_BOOT_SERIAL_UART
	uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
#elif CONFIG_BOOT_SERIAL_CDC_ACM
	uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
#endif

	if (!device_is_ready(uart_dev)) {
		return (-1);
	}

#if CONFIG_BOOT_SERIAL_CDC_ACM
	int rc = usb_enable(NULL);
	if (rc && rc != -EALREADY) {
		return (-1);
	}
#endif

	uart_irq_callback_set(uart_dev, boot_uart_fifo_callback);

	/* Drain the fifo */
	if (uart_irq_rx_ready(uart_dev)) {
		uint8_t c;

		while (uart_fifo_read(uart_dev, &c, 1)) {
			;
		}
	}

	cur = 0;

	uart_irq_rx_enable(uart_dev);

	return 0;
}
