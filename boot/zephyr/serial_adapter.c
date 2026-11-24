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
#include "mcuboot_config/mcuboot_config.h"
#include <zephyr/usb/usbd.h>

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

/* Non-RAW CDC ACM selects the line-based backpressure path in this file. */
#if defined(CONFIG_BOOT_SERIAL_CDC_ACM) && \
    !defined(CONFIG_BOOT_SERIAL_RAW_PROTOCOL)
#define BOOT_SERIAL_CDC_ACM_NON_RAW
#endif

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM)
#include "usbd_cdc_serial.h"

/* How often to wake while waiting for the USB host to open the port. The
 * serial recovery loop that normally feeds the watchdog is not reached until
 * that wait returns, so this has to stay well below any watchdog timeout.
 */
#define BOOT_CDC_ACM_POLL_MS 100

#if defined(CONFIG_BOOT_SERIAL_WAIT_FOR_DFU)
/* The caller only intends to wait this long in total, so waiting for the host
 * must not outlast it.
 */
#define BOOT_CDC_ACM_READY_TIMEOUT_MS CONFIG_BOOT_SERIAL_WAIT_FOR_DFU_TIMEOUT
#else
/* Recovery was asked for explicitly; wait for the host indefinitely. */
#define BOOT_CDC_ACM_READY_TIMEOUT_MS 0
#endif

/* Bound on the wait for VBUS before enabling USB regardless, so a controller
 * that misses the event behaves as it would without the gate.
 */
#define BOOT_CDC_ACM_VBUS_TIMEOUT_MS 1000
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
static struct line_input *cmd;
#ifdef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
static uint8_t rx_buf[CONFIG_BOOT_MAX_LINE_INPUT_LEN];
#else
static uint8_t rx_batch_buf[CONFIG_BOOT_SERIAL_UART_RX_BATCH_SIZE];

/*
 * Number of valid bytes in rx_batch_buf and how many of them have been
 * stored into line buffers.  When a batch cannot be fully stored because
 * no line buffer is free, the remainder stays pending here until the
 * serial recovery code recycles a buffer.
 */
static size_t rx_batch_len;
static size_t rx_batch_off;
#endif

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM) || defined(CONFIG_BOOT_SERIAL_RAW_PROTOCOL)
/*
 * Due to USB CDC ACM having a large write size, which might be more than the available buffers
 * can hold, this will be set to true when the UART is blocked and should not be read from until
 * the buffers have been read by the serial recovery code
 */
static bool uart_blocked = false;
#endif

static int boot_uart_fifo_getline(char **line);
static int boot_uart_fifo_init(void);
#ifndef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
static bool boot_uart_fifo_consume_batch(void);
#endif

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
	 * initializes it again. Re-running would initialize USB a second time
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

#ifndef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
/*
 * Store the bytes already sitting in rx_batch_buf into line buffers,
 * completing a line at every newline.  Returns true once every buffered
 * byte has been consumed.  When no line buffer is available, keeps the
 * unconsumed tail pending and returns false; the caller must then stop
 * reading the FIFO and retry once the serial recovery code recycles a
 * buffer, instead of dropping the tail.
 */
static bool
boot_uart_fifo_consume_batch(void)
{
	const uint8_t *p = rx_batch_buf + rx_batch_off;
	const uint8_t *batch_end = rx_batch_buf + rx_batch_len;

	while (p < batch_end) {
		if (!cmd) {
			sys_snode_t *node;

			node = sys_slist_get(&avail_queue);
			if (!node) {
				rx_batch_off = (size_t)(p - rx_batch_buf);
				return false;
			}
			cmd = CONTAINER_OF(node, struct line_input, node);
		}

		const uint8_t *nl = memchr(p, '\n', batch_end - p);
		size_t chunk;
		size_t room = (size_t)(CONFIG_BOOT_MAX_LINE_INPUT_LEN - cur);
		size_t copy_len;

		if (nl != NULL) {
			chunk = (size_t)(nl - p + 1);
		} else {
			chunk = (size_t)(batch_end - p);
		}
		copy_len = MIN(chunk, room);

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

	rx_batch_off = rx_batch_len;

	return true;
}
#endif

#if defined(BOOT_SERIAL_CDC_ACM_NON_RAW)
/*
 * Drain the pending batch into line buffers, blocking reception while it
 * does not fit.
 *
 * Must be called with interrupts locked: consume_batch() mutates cmd, cur,
 * rx_batch_off/rx_batch_len and the line queues, which
 * boot_uart_fifo_getline() also updates under irq_lock(), and the blocked
 * state transitions must be serialized against it.
 *
 * When the batch does not fit, uart_blocked is set and RX is disabled, then
 * the drain is retried once so that a line buffer recycled by
 * boot_uart_fifo_getline() before uart_blocked became visible (and which
 * therefore did not trigger an unblock) is noticed instead of leaving the
 * pending tail stalled with a free buffer available.  If the retry drains
 * the batch, reception is resumed rather than staying blocked.
 *
 * Returns true when the batch was fully stored and reception may continue,
 * false when the UART was left blocked and the caller must return without
 * reading more.
 */
static bool
boot_uart_fifo_drain_or_block(void)
{
	if (boot_uart_fifo_consume_batch()) {
		return true;
	}

	uart_blocked = true;
	uart_irq_rx_disable(uart_dev);

	if (!boot_uart_fifo_consume_batch()) {
		return false;
	}

	uart_blocked = false;
	uart_irq_rx_enable(uart_dev);
	return true;
}
#endif

static void
boot_uart_fifo_callback(const struct device *dev, void *user_data)
{
	int rx;

	uart_irq_update(uart_dev);

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM) || defined(CONFIG_BOOT_SERIAL_RAW_PROTOCOL)
	/*
	 * Checked without irq_lock() on purpose: a stale read is benign.
	 * Returning early on a stale true is covered by the unblock path
	 * in boot_uart_fifo_getline(), and proceeding on a stale false
	 * only costs one extra locked consume attempt below.
	 */
	if (uart_blocked == true) {
		return;
	}
#endif

	while (true) {
#if defined(CONFIG_BOOT_SERIAL_RAW_PROTOCOL) || \
    defined(CONFIG_BOOT_SERIAL_CDC_ACM)
		size_t copy_len = CONFIG_BOOT_MAX_LINE_INPUT_LEN - 1 - cur;
		/*
		 * Only a hint for sizing the next read, not a correctness
		 * boundary: in the CDC ACM path it is computed before the
		 * consume_batch() call below, which may pop a buffer into
		 * cmd, so the estimate can already be stale when used.
		 * Backpressure, not this count, covers overflow.
		 */
		size_t free_buffers = sys_slist_len(&avail_queue);

		if (free_buffers > 0 && cmd == NULL) {
			--free_buffers;
		}
#endif

#ifdef CONFIG_BOOT_SERIAL_RAW_PROTOCOL
		if (cur == 0 && free_buffers == 0) {
			uart_blocked = true;
			uart_irq_rx_disable(uart_dev);
			return;
		}

		rx = uart_fifo_read(uart_dev, rx_buf, copy_len);
		if (rx <= 0) {
			if (cmd && cur > 0) {
				/*
				 * No more data to read, so we assume that it means the packet is
				 * complete
				 */
				cmd->len = cur;
				sys_slist_append(&lines_queue, &cmd->node);
				cur = 0;
				cmd = NULL;
			}

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

		memcpy(&cmd->line[cur], rx_buf, rx);
		cur += rx;

		if (cur >= CONFIG_BOOT_MAX_LINE_INPUT_LEN) {
			cmd->len = cur;
			sys_slist_append(&lines_queue, &cmd->node);
			cur = 0;
			cmd = NULL;
		}
#else
#if defined(BOOT_SERIAL_CDC_ACM_NON_RAW)
		size_t max_read_size;
		bool drained;
		int key;

		/*
		 * A previous batch may still hold bytes that could not be
		 * stored because no line buffer was free; finish storing
		 * them before pulling any new data out of the FIFO.
		 */
		key = irq_lock();
		drained = boot_uart_fifo_drain_or_block();
		irq_unlock(key);
		if (!drained) {
			return;
		}

		max_read_size = free_buffers *
				(CONFIG_BOOT_MAX_LINE_INPUT_LEN - 1) + copy_len;

		if (max_read_size > sizeof(rx_batch_buf)) {
			max_read_size = sizeof(rx_batch_buf);
		} else if (max_read_size == 0) {
			/*
			 * No room for even one byte: block, then re-check
			 * for a line buffer recycled before the block took
			 * effect, all in one locked section so the
			 * transition is serialized against
			 * boot_uart_fifo_getline().
			 */
			key = irq_lock();
			uart_blocked = true;
			uart_irq_rx_disable(uart_dev);
			if (sys_slist_is_empty(&avail_queue)) {
				irq_unlock(key);
				return;
			}
			uart_blocked = false;
			uart_irq_rx_enable(uart_dev);
			irq_unlock(key);
			continue;
		}

		rx = uart_fifo_read(uart_dev, rx_batch_buf, max_read_size);
#else
		rx = uart_fifo_read(uart_dev, rx_batch_buf, sizeof(rx_batch_buf));
#endif
		if (rx <= 0) {
			break;
		}

#if defined(BOOT_SERIAL_CDC_ACM_NON_RAW)
		/*
		 * No line buffer for the batch tail: keep it pending in
		 * rx_batch_buf and stop reading.  The FIFO backs up until
		 * the serial recovery code recycles a buffer, so no
		 * already-received byte is lost.
		 *
		 * rx_batch_len/rx_batch_off are assigned inside the locked
		 * section because boot_uart_fifo_getline() may run
		 * consume_batch() concurrently and the state is cursor
		 * based.
		 */
		key = irq_lock();
		rx_batch_len = rx;
		rx_batch_off = 0;
		drained = boot_uart_fifo_drain_or_block();
		irq_unlock(key);
		if (!drained) {
			return;
		}
#else
		rx_batch_len = rx;
		rx_batch_off = 0;

		if (!boot_uart_fifo_consume_batch()) {
			/*
			 * Plain UART has no backpressure: the tail stays
			 * pending in rx_batch_* only until the next receive
			 * callback overwrites it.  Dropping it here is
			 * intentional and matches the behavior from before
			 * the rx_batch_* state existed.
			 */
			BOOT_LOG_ERR("Not enough memory to store"
				     " incoming data!");
			return;
		}
#endif
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
	static struct line_input *input_cmd;
	sys_snode_t *node;
	int key;

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM) || defined(CONFIG_BOOT_SERIAL_RAW_PROTOCOL)
	bool unblock = false;
#endif
#if defined(BOOT_SERIAL_CDC_ACM_NON_RAW)
	bool drained = false;
#endif

	key = irq_lock();
	/* Recycle cmd buffer returned previous time */
	if (input_cmd != NULL) {
		if (sys_slist_peek_tail(&avail_queue) != &input_cmd->node) {
			sys_slist_append(&avail_queue, &input_cmd->node);

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM) || defined(CONFIG_BOOT_SERIAL_RAW_PROTOCOL)
			if (uart_blocked) {
				unblock = true;
			}
#endif
		}
	}

#if defined(BOOT_SERIAL_CDC_ACM_NON_RAW)
	if (unblock) {
		/*
		 * The recycled buffer may unblock a batch that was kept
		 * pending in rx_batch_buf; store the remainder before
		 * resuming reception.  This cannot wait for the next
		 * receive interrupt: the FIFO can stay empty while bytes
		 * are still pending, e.g. when the sender is waiting for
		 * a response.  The receive callback runs on a work queue
		 * thread that can preempt this one as USB interrupts are
		 * handled, so the drain takes the same lock as the other
		 * queue operations above.  If the remainder still does
		 * not fit, stay blocked; the next recycled buffer retries
		 * the drain.
		 */
		drained = boot_uart_fifo_consume_batch();
	}
#endif

	node = sys_slist_get(&lines_queue);

#if defined(BOOT_SERIAL_CDC_ACM_NON_RAW)
	/*
	 * Unblock inside the locked section so this enable is fully
	 * serialized against the disable in the receive callback: the
	 * enable cannot be overwritten by a stale disable.
	 */
	if (unblock && drained) {
		uart_blocked = false;
		uart_irq_rx_enable(uart_dev);
	}
#elif defined(CONFIG_BOOT_SERIAL_RAW_PROTOCOL)
	if (unblock) {
		uart_blocked = false;
		uart_irq_rx_enable(uart_dev);
	}
#endif
	irq_unlock(key);

	if (node == NULL) {
		input_cmd = NULL;
		*line = NULL;

		return 0;
	}

	input_cmd = CONTAINER_OF(node, struct line_input, node);
	*line = input_cmd->line;
	return input_cmd->len;
}

#if defined(CONFIG_BOOT_SERIAL_CDC_ACM)
/* Wait for @p sem, feeding the watchdog meanwhile: the serial recovery loop
 * that would otherwise feed it is not reached until this returns.
 * @p waited_ms accumulates across calls; a @p limit_ms of 0 waits forever.
 */
static bool
boot_cdc_acm_wait(struct k_sem *sem, uint32_t limit_ms, uint32_t *waited_ms)
{
	const k_timeout_t poll = K_MSEC(BOOT_CDC_ACM_POLL_MS);

	while (k_sem_take(sem, poll) != 0) {
#if CONFIG_BOOT_WATCHDOG_FEED
		MCUBOOT_WATCHDOG_FEED();
#endif
		*waited_ms += BOOT_CDC_ACM_POLL_MS;

		if (limit_ms != 0 && *waited_ms >= limit_ms) {
			return false;
		}
	}

	return true;
}
#endif

static int
boot_uart_fifo_init(void)
{
#if defined(CONFIG_BOOT_SERIAL_UART)

#if DT_HAS_CHOSEN(zephyr_uart_mcumgr)
	uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_uart_mcumgr));
#else
	uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
#endif

	if (!device_is_ready(uart_dev)) {
		return (-1);
	}

	if (uart_irq_callback_set(uart_dev, boot_uart_fifo_callback) != 0) {
		return (-1);
	}

	/* Drain the fifo */
	if (uart_irq_rx_ready(uart_dev)) {
		uint8_t c;

		while (uart_fifo_read(uart_dev, &c, 1)) {
			;
		}
	}

	cur = 0;

	uart_irq_rx_enable(uart_dev);

#elif defined(CONFIG_BOOT_SERIAL_CDC_ACM)

	struct usbd_context *usbd = boot_usb_cdc_serial_get_context();
	uint32_t vbus_limit_ms = BOOT_CDC_ACM_VBUS_TIMEOUT_MS;
	uint32_t waited_ms = 0;
	int rc;

	rc = boot_usb_cdc_serial_init();
	if (rc) {
		return (-1);
	}

	/* The USB device stack expects a controller that can detect VBUS to be
	 * enabled only after it reports VBUS.
	 */
	if (usbd_can_detect_vbus(usbd)) {
		if (BOOT_CDC_ACM_READY_TIMEOUT_MS != 0 &&
		    BOOT_CDC_ACM_READY_TIMEOUT_MS < vbus_limit_ms) {
			vbus_limit_ms = BOOT_CDC_ACM_READY_TIMEOUT_MS;
		}

		if (!boot_cdc_acm_wait(&boot_usbd_vbus_ready, vbus_limit_ms,
				       &waited_ms)) {
			BOOT_LOG_WRN("No VBUS in %u ms, enabling USB anyway",
				     (unsigned int)waited_ms);
		}
	}

	rc = usbd_enable(usbd);
	if (rc) {
		return (-1);
	}

	/* Wait for the USB host to open the serial port. Time spent waiting
	 * for VBUS counts against the same limit.
	 */
	(void)boot_cdc_acm_wait(&boot_cdc_acm_ready,
				BOOT_CDC_ACM_READY_TIMEOUT_MS, &waited_ms);

	uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
	if (!device_is_ready(uart_dev)) {
		return (-1);
	}

	if (uart_irq_callback_set(uart_dev, boot_uart_fifo_callback) != 0) {
		return (-1);
	}
	cur = 0;
	uart_irq_rx_enable(uart_dev);

#else
#error No serial recovery device selected
#endif

	return 0;
}
