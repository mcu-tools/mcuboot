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
#include <usb/usb_device.h>
#include <soc.h>

#include "target.h"

#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "flash_map_backend/flash_map_backend.h"

#ifdef CONFIG_MCUBOOT_SERIAL
#include "boot_serial/boot_serial.h"
#include "serial_adapter/serial_adapter.h"

const struct boot_uart_funcs boot_funcs = {
    .read = console_read,
    .write = console_write
};
#endif

MCUBOOT_LOG_MODULE_REGISTER(mcuboot);

void os_heap_init(void);

#if defined(CONFIG_ARM)
struct arm_vector_table {
    uint32_t msp;
    uint32_t reset;
};

extern void sys_clock_disable(void);

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
#ifdef CONFIG_BOOT_SERIAL_CDC_ACM
    /* Disable the USB to prevent it from firing interrupts */
    usb_disable();
#endif
    __set_MSP(vt->msp);
    ((void (*)(void))vt->reset)();
}

#elif defined(CONFIG_XTENSA)
#define SRAM_BASE_ADDRESS	0xBE030000

static void copy_img_to_SRAM(int slot, unsigned int hdr_offset)
{
    const struct flash_area *fap;
    int area_id;
    int rc;
    unsigned char *dst = (unsigned char *)(SRAM_BASE_ADDRESS + hdr_offset);

    BOOT_LOG_INF("Copying image to SRAM");

    area_id = flash_area_id_from_image_slot(slot);
    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
	BOOT_LOG_ERR("flash_area_open failed with %d\n", rc);
        goto done;
    }

    rc = flash_area_read(fap, hdr_offset, dst, fap->fa_size - hdr_offset);
    if (rc != 0) {
	BOOT_LOG_ERR("flash_area_read failed with %d\n", rc);
        goto done;
    }

done:
    flash_area_close(fap);
}

/* Entry point (.ResetVector) is at the very beginning of the image.
 * Simply copy the image to a suitable location and jump there.
 */
static void do_boot(struct boot_rsp *rsp)
{
    void *start;

    BOOT_LOG_INF("br_image_off = 0x%x\n", rsp->br_image_off);
    BOOT_LOG_INF("ih_hdr_size = 0x%x\n", rsp->br_hdr->ih_hdr_size);

    /* Copy from the flash to HP SRAM */
    copy_img_to_SRAM(0, rsp->br_hdr->ih_hdr_size);

    /* Jump to entry point */
    start = (void *)(SRAM_BASE_ADDRESS + rsp->br_hdr->ih_hdr_size);
    ((void (*)(void))start)();
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

#if (!defined(CONFIG_XTENSA) && defined(DT_FLASH_DEV_NAME))
    if (!flash_device_get_binding(DT_FLASH_DEV_NAME)) {
        BOOT_LOG_ERR("Flash device %s not found", DT_FLASH_DEV_NAME);
        while (1)
            ;
    }
#elif (defined(CONFIG_XTENSA) && defined(DT_JEDEC_SPI_NOR_0_LABEL))
    if (!flash_device_get_binding(DT_JEDEC_SPI_NOR_0_LABEL)) {
        BOOT_LOG_ERR("Flash device %s not found", DT_JEDEC_SPI_NOR_0_LABEL);
        while (1)
            ;
    }
#endif

#ifdef CONFIG_MCUBOOT_SERIAL

    struct device *detect_port;
    u32_t detect_value;

    detect_port = device_get_binding(CONFIG_BOOT_SERIAL_DETECT_PORT);
    __ASSERT(detect_port, "Error: Bad port for boot serial detection.\n");

    rc = gpio_pin_configure(detect_port, CONFIG_BOOT_SERIAL_DETECT_PIN,
                            GPIO_DIR_IN | GPIO_PUD_PULL_UP);
    __ASSERT(rc == 0, "Error of boot detect pin initialization.\n");

    rc = gpio_pin_read(detect_port, CONFIG_BOOT_SERIAL_DETECT_PIN,
                       &detect_value);
    __ASSERT(rc == 0, "Error of the reading the detect pin.\n");

    if (detect_value == CONFIG_BOOT_SERIAL_DETECT_PIN_VAL) {
        BOOT_LOG_INF("Enter the serial recovery mode");
        rc = boot_console_init();
        __ASSERT(rc == 0, "Error initializing boot console.\n");
        boot_serial_start(&boot_funcs);
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
                 rsp.br_image_off);

    BOOT_LOG_INF("Jumping to the first image slot");
    do_boot(&rsp);

    BOOT_LOG_ERR("Never should get here");
    while (1)
        ;
}
