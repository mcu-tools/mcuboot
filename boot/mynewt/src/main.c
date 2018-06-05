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
#include "syscfg/syscfg.h"
#include <flash_map_backend/flash_map_backend.h>
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_bsp.h>
#include <hal/hal_system.h>
#include <hal/hal_flash.h>
#include <sysinit/sysinit.h>
#ifdef MCUBOOT_SERIAL
#include <hal/hal_gpio.h>
#include <hal/hal_nvreg.h>
#include <boot_serial/boot_serial.h>
#endif
#include <console/console.h>
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"

#define BOOT_AREA_DESC_MAX    (256)
#define AREA_DESC_MAX         (BOOT_AREA_DESC_MAX)

#ifdef MCUBOOT_SERIAL
#define BOOT_SERIAL_INPUT_MAX (512)
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

#ifdef MCUBOOT_SERIAL
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

        boot_serial_start(BOOT_SERIAL_INPUT_MAX);
        assert(0);
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
        boot_serial_start(BOOT_SERIAL_INPUT_MAX);
        assert(0);
    }
#endif

    /*
     * Listen for management pattern in UART input.  If detected, await for
     * download commands from serial.
     */
#if MYNEWT_VAL(BOOT_SERIAL_DETECT_TIMEOUT) != 0
    if (boot_serial_detect_uart_string()) {
        boot_serial_start(BOOT_SERIAL_INPUT_MAX);
        assert(0);
    }
#endif
}
#endif

int
main(void)
{
    struct boot_rsp rsp;
    uintptr_t flash_base;
    int rc;

    hal_bsp_init();

#if defined(MCUBOOT_SERIAL) || defined(MCUBOOT_HAVE_LOGGING)
    /* initialize uart without os */
    os_dev_initialize_all(OS_DEV_INIT_PRIMARY);
    os_dev_initialize_all(OS_DEV_INIT_SECONDARY);
    sysinit();
    console_blocking_mode();
#if defined(MCUBOOT_SERIAL)
    serial_boot_detect();
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
