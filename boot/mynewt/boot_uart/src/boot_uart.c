/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include "os/mynewt.h"
#include <uart/uart.h>

/*
 * RX is a ring buffer, which gets drained constantly.
 * TX blocks until buffer has been completely transmitted.
 */
#define CONSOLE_HEAD_INC(cr) (((cr)->head + 1) & (sizeof((cr)->buf) - 1))
#define CONSOLE_TAIL_INC(cr) (((cr)->tail + 1) & (sizeof((cr)->buf) - 1))

struct {
    uint8_t head;
    uint8_t tail;
    uint8_t buf[16];
} bs_uart_rx;

struct {
    uint8_t *ptr;
    int cnt;
} volatile bs_uart_tx;

static struct uart_dev *bs_uart;

static int bs_rx_char(void *arg, uint8_t byte);
static int bs_tx_char(void *arg);

int
boot_uart_open(void)
{
    struct uart_conf uc = {
        .uc_speed = MYNEWT_VAL(CONSOLE_UART_BAUD),
        .uc_databits = 8,
        .uc_stopbits = 1,
        .uc_parity = UART_PARITY_NONE,
        .uc_flow_ctl = MYNEWT_VAL(CONSOLE_UART_FLOW_CONTROL),
        .uc_tx_char = bs_tx_char,
        .uc_rx_char = bs_rx_char,
    };

    bs_uart = (struct uart_dev *)os_dev_open(MYNEWT_VAL(CONSOLE_UART_DEV),
      OS_TIMEOUT_NEVER, &uc);
    if (!bs_uart) {
        return -1;
    }
    return 0;
}

void
boot_uart_close(void)
{
    os_dev_close(&bs_uart->ud_dev);
    bs_uart_rx.head = 0;
    bs_uart_rx.tail = 0;
    bs_uart_tx.cnt = 0;
}

static int
bs_rx_char(void *arg, uint8_t byte)
{
    if (CONSOLE_HEAD_INC(&bs_uart_rx) == bs_uart_rx.tail) {
        /*
         * RX queue full. Reader must drain this.
         */
        return -1;
    }
    bs_uart_rx.buf[bs_uart_rx.head] = byte;
    bs_uart_rx.head = CONSOLE_HEAD_INC(&bs_uart_rx);
    return 0;
}

static uint8_t
bs_pull_char(void)
{
    uint8_t ch;

    ch = bs_uart_rx.buf[bs_uart_rx.tail];
    bs_uart_rx.tail = CONSOLE_TAIL_INC(&bs_uart_rx);
    return ch;
}

int
boot_uart_read(char *str, int cnt, int *newline)
{
    int i;
    int sr;
    uint8_t ch;

    *newline = 0;
    OS_ENTER_CRITICAL(sr);
    for (i = 0; i < cnt; i++) {
        if (bs_uart_rx.head == bs_uart_rx.tail) {
            break;
        }

        ch = bs_pull_char();
        if (ch == '\n') {
            *str = '\0';
            *newline = 1;
            break;
        }
        *str++ = ch;
    }
    OS_EXIT_CRITICAL(sr);
    if (i > 0 || *newline) {
        uart_start_rx(bs_uart);
    }
    return i;
}

static int
bs_tx_char(void *arg)
{
    uint8_t ch;

    if (!bs_uart_tx.cnt) {
        return -1;
    }

    bs_uart_tx.cnt--;
    ch = *bs_uart_tx.ptr;
    bs_uart_tx.ptr++;
    return ch;
}

#if MYNEWT_VAL(SELFTEST)
/*
 * OS is not running, so native uart 'driver' cannot run either.
 * At the moment unit tests don't check the responses to messages it
 * sends, so we can drop the outgoing data here.
 */
void
boot_uart_write(const char *ptr, int cnt)
{
}

#else

void
boot_uart_write(const char *ptr, int cnt)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    bs_uart_tx.ptr = (uint8_t *)ptr;
    bs_uart_tx.cnt = cnt;
    OS_EXIT_CRITICAL(sr);

    uart_start_tx(bs_uart);
    while (bs_uart_tx.cnt);
}

#endif
