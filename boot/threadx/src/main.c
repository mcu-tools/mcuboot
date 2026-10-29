/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"

BOOT_LOG_MODULE_REGISTER(mcuboot);

#define MCUBOOT_THREADX_VERSION "1.0.0"

static void do_boot_handoff(struct boot_rsp *rsp)
{
    uint32_t vector_table = rsp->br_image_off + rsp->br_hdr->ih_hdr_size;

    BOOT_LOG_INF("Handoff: transferring control to application at 0x%08x",
                 (unsigned int)vector_table);

#if defined(__arm__) || defined(__thumb__)
    /*
     * Cortex-M Vector Table Relocation and Entry:
     * - Vector table address must be aligned to table size (minimum 128/256 bytes).
     * - Word 0: Initial Main Stack Pointer (MSP).
     * - Word 1: Reset Handler address (entry point).
     */
    volatile uint32_t *vtor = (volatile uint32_t *)0xE000ED08;
    *vtor = vector_table;

    uint32_t initial_msp = *(volatile uint32_t *)vector_table;
    uint32_t reset_handler = *(volatile uint32_t *)(vector_table + 4);

    __asm volatile("msr msp, %0" : : "r"(initial_msp));
    void (*app_entry)(void) = (void (*)(void))reset_handler;
    app_entry();
#else
    BOOT_LOG_INF("Host environment: boot validation complete, target vector=0x%08x",
                 (unsigned int)vector_table);
#endif
}

int main(int argc, char **argv)
{
    struct boot_rsp rsp;
    int rc;

    (void)argc;
    (void)argv;

    BOOT_LOG_INF("*** Booting MCUboot %s for Eclipse ThreadX ***", MCUBOOT_THREADX_VERSION);

    rc = flash_map_backend_init();
    if (rc != 0) {
        BOOT_LOG_ERR("Failed to initialize flash map backend (error: %d)", rc);
        return rc;
    }

    rc = boot_go(&rsp);
    if (rc != 0) {
        BOOT_LOG_ERR("Unable to find or validate bootable image (error: %d)", rc);
        return rc;
    }

    BOOT_LOG_INF("Image verified: offset=0x%08x, hdr_size=0x%x, version=%u.%u.%u+%u",
                 (unsigned int)rsp.br_image_off,
                 (unsigned int)rsp.br_hdr->ih_hdr_size,
                 rsp.br_hdr->ih_ver.iv_major,
                 rsp.br_hdr->ih_ver.iv_minor,
                 rsp.br_hdr->ih_ver.iv_revision,
                 (unsigned int)rsp.br_hdr->ih_ver.iv_build_num);

    do_boot_handoff(&rsp);
    return 0;
}
