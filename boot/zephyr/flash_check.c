/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>

#include <../../bootutil/src/bootutil_priv.h>
#include "bootutil/bootutil_log.h"

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_SLOT0_EXPECTED_WRITE_SIZE) || defined(MCUBOOT_SLOT1_EXPECTED_WRITE_SIZE)
BOOT_LOG_MODULE_DECLARE(mcuboot);

bool swap_write_block_size_check(struct boot_loader_state *state)
{
#ifdef MCUBOOT_SLOT0_EXPECTED_WRITE_SIZE
    size_t flash_write_block_size_pri;
#endif
#ifdef MCUBOOT_SLOT1_EXPECTED_WRITE_SIZE
    size_t flash_write_block_size_sec;
#endif

#ifdef MCUBOOT_SLOT0_EXPECTED_WRITE_SIZE
    flash_write_block_size_pri = flash_get_write_block_size(
                                              state->imgs[0][BOOT_PRIMARY_SLOT].area->fa_dev);

    if (flash_write_block_size_pri != MCUBOOT_SLOT0_EXPECTED_WRITE_SIZE) {
        BOOT_LOG_DBG("Discrepancy, slot0 expected write block size: %d, actual: %d",
                     MCUBOOT_SLOT0_EXPECTED_WRITE_SIZE, flash_write_block_size_pri);
    }
#endif

#ifdef MCUBOOT_SLOT1_EXPECTED_WRITE_SIZE
    flash_write_block_size_sec = flash_get_write_block_size(
                                              state->imgs[0][BOOT_SECONDARY_SLOT].area->fa_dev);

    if (flash_write_block_size_sec != MCUBOOT_SLOT1_EXPECTED_WRITE_SIZE) {
        BOOT_LOG_DBG("Discrepancy, slot1 expected write block size: %d, actual: %d",
                     MCUBOOT_SLOT1_EXPECTED_WRITE_SIZE, flash_write_block_size_sec);
    }
#endif

    return true;
}
#endif
