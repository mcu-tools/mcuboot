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
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "os/endian.h"
#include "base64/base64.h"
#include "crc/crc16.h"
#include "testutil/testutil.h"
#include "hal/hal_flash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "boot_serial/boot_serial.h"
#include "boot_serial_priv.h"

TEST_CASE_DECL(boot_serial_setup)
TEST_CASE_DECL(boot_serial_empty_msg)
TEST_CASE_DECL(boot_serial_empty_img_msg)
TEST_CASE_DECL(boot_serial_img_msg)
TEST_CASE_DECL(boot_serial_upload_bigger_image)
TEST_CASE_DECL(boot_serial_upload_unaligned_tail)

/* Serial output produced in response to the last tx_msg() */
static char test_uart_buf[512];
static int test_uart_len;
static bool test_uart_overflow;

static void
test_uart_write(const char *str, int len)
{
    if (len > (int)sizeof(test_uart_buf) - test_uart_len) {
        test_uart_overflow = true;
        return;
    }
    memcpy(&test_uart_buf[test_uart_len], str, len);
    test_uart_len += len;
}

static const struct boot_uart_funcs test_uart = {
    .write = test_uart_write
};

void
tx_msg(void *src, int len)
{
    test_uart_len = 0;
    test_uart_overflow = false;
    boot_serial_input(src, len);
}

/*
 * Decode the serial framing of the response to the last tx_msg(); copies the
 * newtmgr header and payload to buf and returns their length, or -1 if the
 * response is malformed or does not fit.
 */
int
rx_msg(void *buf, int maxlen)
{
    char enc[sizeof(test_uart_buf) + 1];
    uint8_t dec[sizeof(test_uart_buf)];
    uint16_t totlen;
    int enc_len;
    int dec_len;
    int off;

    if (test_uart_overflow) {
        return -1;
    }

    /* Each frame is a start marker, base64 data and a newline */
    enc_len = 0;
    off = 0;
    while (off < test_uart_len) {
        if (test_uart_len - off < 2) {
            return -1;
        }
        if (off == 0 ?
            (test_uart_buf[off] != SHELL_NLIP_PKT_START1 ||
             test_uart_buf[off + 1] != SHELL_NLIP_PKT_START2) :
            (test_uart_buf[off] != SHELL_NLIP_DATA_START1 ||
             test_uart_buf[off + 1] != SHELL_NLIP_DATA_START2)) {
            return -1;
        }
        off += 2;
        while (off < test_uart_len && test_uart_buf[off] != '\n') {
            enc[enc_len++] = test_uart_buf[off++];
        }
        if (off == test_uart_len) {
            return -1;
        }
        off++;
    }
    enc[enc_len] = '\0';

    /* Total length, then header and payload, then CRC over both */
    dec_len = base64_decode_maxlen(enc, dec, sizeof(dec));
    if (dec_len < (int)(sizeof(totlen) + sizeof(struct nmgr_hdr) + sizeof(uint16_t))) {
        return -1;
    }
    memcpy(&totlen, dec, sizeof(totlen));
    totlen = ntohs(totlen);
    if (totlen != dec_len - sizeof(totlen) ||
        crc16_ccitt(CRC16_INITIAL_CRC, &dec[sizeof(totlen)], totlen) != 0) {
        return -1;
    }
    totlen -= sizeof(uint16_t);
    if (totlen > maxlen) {
        return -1;
    }
    memcpy(buf, &dec[sizeof(totlen)], totlen);

    return totlen;
}

TEST_SUITE(boot_serial_suite)
{
    boot_serial_setup();
    boot_serial_empty_msg();
    boot_serial_empty_img_msg();
    boot_serial_img_msg();
    boot_serial_upload_bigger_image();
    boot_serial_upload_unaligned_tail();
}

int
boot_serial_test(void)
{
    boot_uf = &test_uart;
    boot_serial_suite();
    return tu_any_failed;
}

#if MYNEWT_VAL(SELFTEST)
int
main(void)
{
    sysinit();

    boot_serial_test();

    return tu_any_failed;
}
#endif
