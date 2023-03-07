/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mcuboot_config/mcuboot_logging.h>

/**
 * Override the bootloader's print banner function from IDF.
 */
void __wrap_bootloader_print_banner(void)
{
    MCUBOOT_LOG_INF("*** Booting MCUboot build %s ***", MCUBOOT_VER);
}
