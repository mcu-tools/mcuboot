/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include "board.h"
#include "hpm_debug_console.h"

#include "hpm_gpio_drv.h"
#include "hpm_romapi.h"
#include "hpm_l1c_drv.h"

#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/mcuboot_status.h"
#include "flash_map_backend/flash_map_backend.h"

#define irq_lock() disable_global_irq(CSR_MSTATUS_MIE_MASK);
#define irq_unlock() enable_global_irq(CSR_MSTATUS_MIE_MASK);

static inline bool boot_skip_serial_recovery(void)
{
    return false;
}

/* Default: Assume entry point is at the very beginning of the image. Simply
 * lock interrupts and jump there. This is the right thing to do for X86 and
 * possibly other platforms.
 */
static void do_boot(struct boot_rsp *rsp)
{
    void *start;

    l1c_fence_i();
#if defined(MCUBOOT_RAM_LOAD)
    start = (void *)(rsp->br_hdr->ih_load_addr + rsp->br_hdr->ih_hdr_size);
#else
    uintptr_t flash_base;
    int rc;

    rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
    assert(rc == 0);

    start = (void *)(flash_base + rsp->br_image_off +
                     rsp->br_hdr->ih_hdr_size);
#endif
    l1c_dc_flush_all();
    l1c_dc_disable();
    l1c_ic_disable();

    /* Lock interrupts and dive into the entry point */
    irq_lock();


    ((void (*)(void))start)();
}

#define LED_FLASH_PERIOD_IN_MS 300

int main(void)
{
    struct boot_rsp rsp;
    int rc;
    fih_int fih_rc = FIH_FAILURE;

    board_init();
    board_init_led_pins();

#if !defined(MCUBOOT_DIRECT_XIP)
    BOOT_LOG_INF("Starting bootloader");
#else
    BOOT_LOG_INF("Starting Direct-XIP bootloader");
#endif

#ifdef CONFIG_MCUBOOT_INDICATION_LED
    /* LED init */
    led_init();
#endif

    (void)rc;

    mcuboot_status_change(MCUBOOT_STATUS_STARTUP);

    FIH_CALL(boot_go, fih_rc, &rsp);

    if (fih_not_eq(fih_rc, FIH_SUCCESS)) {
        BOOT_LOG_ERR("Unable to find bootable image");

        mcuboot_status_change(MCUBOOT_STATUS_NO_BOOTABLE_IMAGE_FOUND);

        FIH_PANIC;
    }

    BOOT_LOG_INF("Bootloader chainload address offset: 0x%x",
                 rsp.br_image_off);

#if defined(MCUBOOT_DIRECT_XIP)
    BOOT_LOG_INF("Jumping to the image slot");
#else
    BOOT_LOG_INF("Jumping to the first image slot");
#endif

    mcuboot_status_change(MCUBOOT_STATUS_BOOTABLE_IMAGE_FOUND);

    do_boot(&rsp);

    mcuboot_status_change(MCUBOOT_STATUS_BOOT_FAILED);

    BOOT_LOG_ERR("Never should get here");
    while (1) {

    };
    return -1;
}
