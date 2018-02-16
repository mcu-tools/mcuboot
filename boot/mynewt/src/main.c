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

#ifdef MCUBOOT_MYNEWT
#include "mcuboot_config/mcuboot_config.h"
#endif

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include "syscfg/syscfg.h"
#include <flash_map/flash_map.h>
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_bsp.h>
#include <hal/hal_system.h>
#include <hal/hal_flash.h>
#ifdef MCUBOOT_SERIAL
#include <hal/hal_gpio.h>
#include <boot_serial/boot_serial.h>
#include <sysinit/sysinit.h>
#endif
#include <console/console.h>
#include "bootutil/image.h"
#include "bootutil/bootutil.h"

#define BOOT_AREA_DESC_MAX  (256)
#define AREA_DESC_MAX       (BOOT_AREA_DESC_MAX)

#ifdef MCUBOOT_SERIAL
#define BOOT_SER_CONS_INPUT         128
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
    os_dev_initialize_all(OS_DEV_INIT_PRIMARY);
    os_dev_initialize_all(OS_DEV_INIT_SECONDARY);
#ifdef MCUBOOT_SERIAL
    sysinit();
#else
    flash_map_init();
#endif

#ifdef MCUBOOT_SERIAL
    /*
     * Configure a GPIO as input, and compare it against expected value.
     * If it matches, await for download commands from serial.
     */
    hal_gpio_init_in(MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN),
                     MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN_CFG));
    if (hal_gpio_read(MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN)) ==
                      MYNEWT_VAL(BOOT_SERIAL_DETECT_PIN_VAL)) {
        boot_serial_start(BOOT_SER_CONS_INPUT);
        assert(0);
    }
#endif
    rc = boot_go(&rsp);
    assert(rc == 0);

    rc = flash_device_base(rsp.br_flash_dev_id, &flash_base);
    assert(rc == 0);

    hal_system_start((void *)(flash_base + rsp.br_image_off +
                              rsp.br_hdr->ih_hdr_size));

    return 0;
}
