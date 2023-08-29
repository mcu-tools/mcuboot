/*
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include "bootutil/bootutil_log.h"
#include "../boot_serial/src/boot_serial_priv.h"
#include <zcbor_encode.h>
#include <boot_serial/boot_serial_extensions.h>

BOOT_LOG_MODULE_DECLARE(mcuboot);

int bs_peruser_system_specific(const struct nmgr_hdr *hdr, const char *buffer,
                               int len, zcbor_state_t *cs)
{
    int mgmt_rc = MGMT_ERR_ENOTSUP;

    STRUCT_SECTION_FOREACH(mcuboot_bs_custom_handlers, function) {
        if (function->handler) {
            mgmt_rc = function->handler(hdr, buffer, len, cs);

            if (mgmt_rc != MGMT_ERR_ENOTSUP) {
                break;
            }
        }
    }

    if (mgmt_rc == MGMT_ERR_ENOTSUP) {
        zcbor_map_start_encode(cs, 10);
        zcbor_tstr_put_lit(cs, "rc");
        zcbor_uint32_put(cs, mgmt_rc);
        zcbor_map_end_encode(cs, 10);
    }

    return MGMT_ERR_OK;
}
