/*
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt_defines.h>
#include <zephyr/mgmt/mcumgr/grp/zephyr/zephyr_basic.h>
#include <../subsys/mgmt/mcumgr/transport/include/mgmt/mcumgr/transport/smp_internal.h>

#include <flash_map_backend/flash_map_backend.h>
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"
#include "../boot_serial/src/boot_serial_priv.h"
#include <zcbor_encode.h>

#include "bootutil/image.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/boot_hooks.h"

#include <boot_serial/boot_serial_extensions.h>

BOOT_LOG_MODULE_DECLARE(mcuboot);

#ifdef CONFIG_BOOT_MGMT_CUSTOM_STORAGE_ERASE
static int bs_custom_storage_erase(const struct nmgr_hdr *hdr,
                                   const char *buffer, int len,
                                   zcbor_state_t *cs)
{
    int rc;
    const struct flash_area *fa;

    (void)buffer;
    (void)len;

    if (hdr->nh_group != ZEPHYR_MGMT_GRP_BASIC || hdr->nh_op != NMGR_OP_WRITE ||
        hdr->nh_id != ZEPHYR_MGMT_GRP_BASIC_CMD_ERASE_STORAGE) {
        return MGMT_ERR_ENOTSUP;
    }

    rc = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);

    if (rc < 0) {
        BOOT_LOG_ERR("failed to open flash area");
    } else {
        rc = flash_area_erase(fa, 0, flash_area_get_size(fa));
        if (rc < 0) {
            BOOT_LOG_ERR("failed to erase flash area");
        }
        flash_area_close(fa);
    }
    if (rc == 0) {
        rc = MGMT_ERR_OK;
    } else {
        rc = MGMT_ERR_EUNKNOWN;
    }

    zcbor_map_start_encode(cs, 10);
    zcbor_tstr_put_lit(cs, "rc");
    zcbor_uint32_put(cs, rc);
    zcbor_map_end_encode(cs, 10);

    return rc;
}

MCUMGR_HANDLER_DEFINE(storage_erase, bs_custom_storage_erase);
#endif
