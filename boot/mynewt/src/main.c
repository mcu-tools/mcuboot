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

#include "mcuboot_config/mcuboot_config.h"

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdio.h>

#include <syscfg/syscfg.h>
#include <flash_map_backend/flash_map_backend.h>
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_bsp.h>
#include <hal/hal_system.h>
#include <hal/hal_flash.h>
#include <hal/hal_watchdog.h>
#include <sysinit/sysinit.h>
#ifdef MCUBOOT_SERIAL
#include <hal/hal_gpio.h>
#include <hal/hal_nvreg.h>
#include <boot_serial/boot_serial.h>
#endif
#if defined(MCUBOOT_SERIAL)
#include <boot_uart/boot_uart.h>
#endif
#include <console/console.h>
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"

#if defined(MCUBOOT_SERIAL)
#define BOOT_SERIAL_REPORT_DUR  \
    (MYNEWT_VAL(OS_CPUTIME_FREQ) / MYNEWT_VAL(BOOT_SERIAL_REPORT_FREQ))
#define BOOT_SERIAL_INPUT_MAX (512)

static int boot_read(char *str, int cnt, int *newline);
static const struct boot_uart_funcs boot_uart_funcs = {
    .read = boot_read,
    .write = boot_uart_write
};

static int
boot_read(char *str, int cnt, int *newline)
{
#if MYNEWT_VAL(BOOT_SERIAL_REPORT_PIN) != -1
    static uint32_t tick = 0;

    if (tick == 0) {
        /*
         * Configure GPIO line as output. This is a pin we toggle at the
         * given frequency.
         */
        hal_gpio_init_out(MYNEWT_VAL(BOOT_SERIAL_REPORT_PIN), 0);
        tick = os_cputime_get32();
    } else {
        if (os_cputime_get32() - tick > BOOT_SERIAL_REPORT_DUR) {
            hal_gpio_toggle(MYNEWT_VAL(BOOT_SERIAL_REPORT_PIN));
            tick = os_cputime_get32();
        }
    }
#endif
    hal_watchdog_tickle();

    return boot_uart_read(str, cnt, newline);
}

#if MYNEWT_VAL(BOOT_SERIAL_DETECT_TIMEOUT) != 0

/** Don't include null-terminator in comparison. */
#define BOOT_SERIAL_DETECT_STRING_LEN \
    (sizeof MYNEWT_VAL(BOOT_SERIAL_DETECT_STRING) - 1)

/**
 * Listens on the UART for the management string.  Blocks for up to
 * BOOT_SERIAL_DETECT_TIMEOUT milliseconds.
 *
 * @return                      true if the management string was received;
 *                              false if the management string was not received
 *                                  before the UART listen timeout expired.
 */
static bool
serial_detect_uart_string(void)
{
    uint32_t start_tick;
    char buf[BOOT_SERIAL_DETECT_STRING_LEN] = { 0 };
    char ch;
    int newline;
    int rc;

    /* Calculate the timeout duration in OS cputime ticks. */
    static const uint32_t timeout_dur =
        MYNEWT_VAL(BOOT_SERIAL_DETECT_TIMEOUT) /
        (1000.0 / MYNEWT_VAL(OS_CPUTIME_FREQ));

    rc = boot_uart_open();
    assert(rc == 0);

    start_tick = os_cputime_get32();

    while (1) {
        /* Read a single character from the UART. */
        rc = boot_uart_read(&ch, 1, &newline);
        if (rc > 0) {
            /* Eliminate the oldest character in the buffer to make room for
             * the new one.
             */
            memmove(buf, buf + 1, BOOT_SERIAL_DETECT_STRING_LEN - 1);
            buf[BOOT_SERIAL_DETECT_STRING_LEN - 1] = ch;

            /* If the full management string has been received, indicate that
             * the serial boot loader should start.
             */
            rc = memcmp(buf,
                        MYNEWT_VAL(BOOT_SERIAL_DETECT_STRING),
                        BOOT_SERIAL_DETECT_STRING_LEN);
            if (rc == 0) {
                boot_uart_close();
                return true;
            }
        }

        /* Abort the listen on timeout. */
        if (os_cputime_get32() >= start_tick + timeout_dur) {
            boot_uart_close();
            return false;
        }
    }
}
#endif

static void
serial_boot_detect(void)
{
    /*
     * Read retained register and compare with expected magic value.
     * If it matches, await for download commands from serial.
     */
#if MYNEWT_VAL(BOOT_SERIAL_NVREG_INDEX) != -1
    if (hal_nvreg_read(MYNEWT_VAL(BOOT_SERIAL_NVREG_INDEX)) ==
        MYNEWT_VAL(BOOT_SERIAL_NVREG_MAGIC)) {

        hal_nvreg_write(MYNEWT_VAL(BOOT_SERIAL_NVREG_INDEX), 0);
        goto serial_boot;
    }
#endif

    /*
     * Configure a GPIO as input, and compare it against expected value.
     * If it matches, await for download commands from serial.
     */
#if MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN) != -1
    hal_gpio_init_in(MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN),
                     MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN_CFG));
    if (hal_gpio_read(MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN)) ==
                      MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN_VAL)) {
        goto serial_boot;
    }
#endif

    /*
     * Listen for management pattern in UART input.  If detected, await for
     * download commands from serial.
     */
#if MYNEWT_VAL(BOOT_SERIAL_DETECT_TIMEOUT) != 0
    if (serial_detect_uart_string()) {
        goto serial_boot;
    }
#endif
    return;
serial_boot:
    boot_uart_open();
    boot_serial_start(&boot_uart_funcs);
    assert(0);
}
#endif

/*
 * Temporary flash_device_base() implementation.
 *
 * TODO: remove this when mynewt needs to support flash_device_base()
 * for devices with nonzero base addresses.
 */
int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    *ret = 0;
    return 0;
}

int
main(void)
{
    struct boot_rsp rsp;
    uintptr_t flash_base;
    int rc;

    hal_bsp_init();

#if !MYNEWT_VAL(OS_SCHEDULING) && MYNEWT_VAL(WATCHDOG_INTERVAL)
    rc = hal_watchdog_init(MYNEWT_VAL(WATCHDOG_INTERVAL));
    assert(rc == 0);
#endif

#if defined(MCUBOOT_SERIAL) || defined(MCUBOOT_HAVE_LOGGING)
    /* initialize uart without os */
    os_dev_initialize_all(OS_DEV_INIT_PRIMARY);
    os_dev_initialize_all(OS_DEV_INIT_SECONDARY);
    sysinit();
    console_blocking_mode();
#if defined(MCUBOOT_SERIAL)
    serial_boot_detect();
    hal_timer_deinit(MYNEWT_VAL(OS_CPUTIME_TIMER_NUM));
#endif
#else
    flash_map_init();
#endif

    rc = boot_go(&rsp);
    assert(rc == 0);

    rc = flash_device_base(rsp.br_flash_dev_id, &flash_base);
    assert(rc == 0);

    hal_system_start((void *)(flash_base + rsp.br_image_off +
                              rsp.br_hdr->ih_hdr_size));

    return 0;
}
