/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/flash.h>
#include <mgmt/mcumgr/zephyr_groups.h>

#include <flash_map_backend/flash_map_backend.h>
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"
#include "../boot_serial/src/boot_serial_priv.h"
#include "../boot_serial/src/cbor_encode.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

int bs_peruser_system_specific(const struct nmgr_hdr *hdr, const char *buffer,
                               int len, cbor_state_t *cs)
{
    const struct flash_area *fa;
    int rc = flash_area_open(FLASH_AREA_ID(storage), &fa);

    if (rc < 0) {
        LOG_ERR("failed to open flash area");
    } else {
	rc = flash_area_erase(fa, 0, FLASH_AREA_SIZE(storage));
	if (rc < 0) {
		LOG_ERR("failed to erase flash area");
	}
	flash_area_close(fa);
    }

    map_start_encode(cs, 10);
    tstrx_put(cs, "rc");
    if (rc != 0) {
        uintx32_put(cs, MGMT_ERR_EUNKNOWN);
    } else {
        uintx32_put(cs, 0);
    }
    map_end_encode(cs, 10);

    return rc;
}
