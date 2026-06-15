/*
 * Copyright (c) 2017-2023 Nordic Semiconductor ASA
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

#if defined(CONFIG_BOOT_SERIAL_UART) && defined(CONFIG_UART_CONSOLE) && \
    (!DT_HAS_CHOSEN(zephyr_uart_mcumgr) ||                              \
     DT_SAME_NODE(DT_CHOSEN(zephyr_uart_mcumgr), DT_CHOSEN(zephyr_console)))
#error Zephyr UART console must be disabled if serial_adapter module is used.
#endif

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM) && \
    defined(CONFIG_UART_CONSOLE) && (!DT_HAS_CHOSEN(zephyr_uart_mcumgr) || \
    DT_SAME_NODE(DT_CHOSEN(zephyr_uart_mcumgr), DT_CHOSEN(zephyr_console)))
#error Zephyr UART console must be disabled if CDC ACM is enabled and MCUmgr \
       has not been redirected to other UART with DTS chosen zephyr,uart-mcumgr.
#endif

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM) && CONFIG_MAIN_THREAD_PRIORITY < 0
#error CONFIG_MAIN_THREAD_PRIORITY must be preemptible to support USB CDC ACM \
       (0 or above)
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
static struct line_input line_bufs[CONFIG_BOOT_LINE_BUFS];

static sys_slist_t avail_queue;
static sys_slist_t lines_queue;

static uint16_t cur;
#ifndef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
static uint8_t rx_batch_buf[CONFIG_BOOT_SERIAL_UART_RX_BATCH_SIZE];
#endif

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

#ifdef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
/*
 * Raw protocol read: the data is binary, so it is returned without a NUL
 * terminator. A fragment may be larger than the caller's buffer, so retain the
 * unconsumed tail for the next call rather than dropping it (the fragment's
 * buffer is not recycled by boot_uart_fifo_getline() until it is fully consumed
 * here). Returns the number of bytes copied into "str".
 */
static int
console_read_raw(char *str, int str_size, int *newline)
{
	static char *pending;
	static int pending_len;
	int n;

	if (pending_len == 0) {
		pending_len = boot_uart_fifo_getline(&pending);
		if (pending == NULL) {
			*newline = 0;
			return 0;
		}
	}

	n = MIN(pending_len, str_size);
	memcpy(str, pending, n);
	pending += n;
	pending_len -= n;
	*newline = 1;
	return n;
}
#endif

int
console_read(char *str, int str_size, int *newline)
{
#ifdef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
	return console_read_raw(str, str_size, newline);
#else
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
#endif
}

int
boot_console_init(void)
{
	static bool initialized;
	int i;

	/* WAIT_FOR_DFU initializes the console early, then boot_serial_enter()
	 * initializes it again. Re-running would call usb_enable() a second time
	 * (returning -EALREADY) and reset the line queues, dropping any bytes
	 * buffered while waiting. Initialize only once.
	 */
	if (initialized) {
		return 0;
	}

	/* Zephyr UART handler takes an empty buffer from avail_queue,
	 * stores UART input in it until EOL, and then puts it into
	 * lines_queue.
	 */
	sys_slist_init(&avail_queue);
	sys_slist_init(&lines_queue);

	for (i = 0; i < ARRAY_SIZE(line_bufs); i++) {
		sys_slist_append(&avail_queue, &line_bufs[i].node);
	}

	int rc = boot_uart_fifo_init();

	if (rc == 0) {
		initialized = true;
	}

	return rc;
}

static void
boot_uart_fifo_callback(const struct device *dev, void *user_data)
{
	static struct line_input *cmd;
	int rx;

	uart_irq_update(uart_dev);

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	while (true) {
#ifdef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
		uint8_t byte;

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

		cmd->line[cur++] = byte;

		if (cur >= CONFIG_BOOT_MAX_LINE_INPUT_LEN) {
			cmd->len = cur;
			sys_slist_append(&lines_queue, &cmd->node);
			cur = 0;
			cmd = NULL;
		}
#else
		rx = uart_fifo_read(uart_dev, rx_batch_buf, sizeof(rx_batch_buf));
		if (rx <= 0) {
			break;
		}

		const uint8_t *p = rx_batch_buf;
		const uint8_t *batch_end = p + rx;

		while (p < batch_end) {
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

			const uint8_t *nl = memchr(p, '\n', batch_end - p);
			size_t chunk = nl ? (size_t)(nl - p + 1) : (size_t)(batch_end - p);
			size_t room = CONFIG_BOOT_MAX_LINE_INPUT_LEN - cur;
			size_t copy_len = MIN(chunk, room);

			memcpy(&cmd->line[cur], p, copy_len);
			cur += copy_len;
			p += chunk;

			if (nl) {
				cmd->len = cur;
				sys_slist_append(&lines_queue, &cmd->node);
				cur = 0;
				cmd = NULL;
			}
		}
#endif
	}

#ifdef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
	/*
	 * No line delimiter exists in raw mode, so deliver whatever has been
	 * received during this interrupt as a fragment; the boot serial reader
	 * reassembles full packets using the SMP header length field.
	 */
	if (cmd != NULL && cur > 0) {
		cmd->len = cur;
		sys_slist_append(&lines_queue, &cmd->node);
		cur = 0;
		cmd = NULL;
	}
#endif
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
#if defined(CONFIG_BOOT_SERIAL_UART)

#if DT_HAS_CHOSEN(zephyr_uart_mcumgr)
	uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_uart_mcumgr));
#else
	uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
#endif

#elif defined(CONFIG_BOOT_SERIAL_CDC_ACM)
        uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
#else
#error No serial recovery device selected
#endif


	if (!device_is_ready(uart_dev)) {
		return (-1);
	}

#if CONFIG_BOOT_SERIAL_CDC_ACM
	int rc = usb_enable(NULL);
	if (rc) {
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
