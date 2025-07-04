/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */

#include <mcuboot_config/mcuboot_config.h>
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

__attribute__((__weak__)) void bootutil_crypto_key_housekeeping(void)
{
    BOOT_LOG_DBG("bootutil_crypto_key_housekeeping: NO BACKEND IMPLEMENTATION\n");
}
