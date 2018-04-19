/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
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

#include <assert.h>
#include <zephyr.h>
#include <gpio.h>
#include <misc/__assert.h>
#include <flash.h>
#include <drivers/system_timer.h>

#include "target.h"

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_INFO
#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "flash_map/flash_map.h"

#ifdef CONFIG_MCUBOOT_SERIAL
#include <boot_serial/boot_serial.h>
#endif

struct device *boot_flash_device;

void os_heap_init(void);

extern void zephyr_flash_area_warn_on_open(void);

#if defined(CONFIG_ARM)
struct arm_vector_table {
    uint32_t msp;
    uint32_t reset;
};

static void do_boot(struct boot_rsp *rsp)
{
    struct arm_vector_table *vt;
    uintptr_t flash_base;
    int rc;

    /* The beginning of the image is the ARM vector table, containing
     * the initial stack pointer address and the reset vector
     * consecutively. Manually set the stack pointer and jump into the
     * reset vector
     */
    rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
    assert(rc == 0);

    vt = (struct arm_vector_table *)(flash_base +
                                     rsp->br_image_off +
                                     rsp->br_hdr->ih_hdr_size);
    irq_lock();
    sys_clock_disable();
    __set_MSP(vt->msp);
    ((void (*)(void))vt->reset)();
}
#else
/* Default: Assume entry point is at the very beginning of the image. Simply
 * lock interrupts and jump there. This is the right thing to do for X86 and
 * possibly other platforms.
 */
static void do_boot(struct boot_rsp *rsp)
{
    uintptr_t flash_base;
    void *start;
    int rc;

    rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
    assert(rc == 0);

    start = (void *)(flash_base + rsp->br_image_off +
                     rsp->br_hdr->ih_hdr_size);

    /* Lock interrupts and dive into the entry point */
    irq_lock();
    ((void (*)(void))start)();
}
#endif

void main(void)
{
    struct boot_rsp rsp;
    int rc;

    BOOT_LOG_INF("Starting bootloader");

    os_heap_init();

    boot_flash_device = device_get_binding(FLASH_DEV_NAME);
    if (!boot_flash_device) {
        BOOT_LOG_ERR("Flash device %s not found", FLASH_DEV_NAME);
        while (1)
            ;
    }

#ifdef CONFIG_MCUBOOT_SERIAL

    struct device *detect_port;
    u32_t detect_value;

    detect_port = device_get_binding(CONFIG_BOOT_SERIAL_DETECT_PORT);
    __ASSERT(detect_port, "Error: Bad port for boot serial detection.\n");

    rc = gpio_pin_configure(detect_port, CONFIG_BOOT_SERIAL_DETECT_PIN,
                            GPIO_DIR_IN | GPIO_PUD_PULL_UP);
    __ASSERT(rc, "Error of boot detect pin initialization.\n");

    rc = gpio_pin_read(detect_port, CONFIG_BOOT_SERIAL_DETECT_PIN, 
                       &detect_value);
    __ASSERT(rc, "Error of the reading the detect pin.\n");

    if (detect_value == CONFIG_BOOT_SERIAL_DETECT_PIN_VAL) {
        BOOT_LOG_INF("Enter the serial recovery mode");
        boot_serial_start(CONFIG_BOOT_MAX_LINE_INPUT_LEN + 1);
        __ASSERT(0, "Bootloader serial process was terminated unexpectedly.\n");
    }
#endif

    rc = boot_go(&rsp);
    if (rc != 0) {
        BOOT_LOG_ERR("Unable to find bootable image");
        while (1)
            ;
    }

    BOOT_LOG_INF("Bootloader chainload address offset: 0x%x",
                 (u32_t)rsp.br_image_off);
    zephyr_flash_area_warn_on_open();
    BOOT_LOG_INF("Jumping to the first image slot");
    do_boot(&rsp);

    BOOT_LOG_ERR("Never should get here");
    while (1)
        ;
}
