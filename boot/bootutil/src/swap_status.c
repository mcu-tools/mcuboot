/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2020 Cypress Semiconductors
 *
 * Original license:
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "swap_priv.h"
#include "swap_status.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/fault_injection_hardening.h"

#include "mcuboot_config/mcuboot_config.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

#ifdef MCUBOOT_SWAP_USING_STATUS

#if !defined(MCUBOOT_DIRECT_XIP) && !defined(MCUBOOT_RAM_LOAD)
int
swap_read_status_bytes(const struct flash_area *fap,
        struct boot_loader_state *state, struct boot_status *bs)
{
    const struct flash_area *fap_stat = NULL;
    uint32_t off;
    uint8_t status = 0;
    uint8_t erased_val;
    uint8_t last_status;
    uint32_t max_entries;
    int32_t found_idx;
    bool found;
    bool invalid;
    int rc;
    uint32_t i;
    (void)state;

    BOOT_LOG_DBG("> STATUS: swap_read_status_bytes: fa_id = %u", (unsigned)fap->fa_id);

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return -1;
    }

    if (fap->fa_id == FLASH_AREA_IMAGE_SCRATCH) {
        max_entries = 1;
    } else {
        max_entries = BOOT_STATUS_MAX_ENTRIES;
    }

    erased_val = flash_area_erased_val(fap_stat);

    off = boot_status_off(fap);

    found = false;
    found_idx = -1;
    invalid = false;
    last_status = erased_val;

    for (i = 0; i < max_entries; i++) {
        rc = swap_status_retrieve(fap->fa_id, off + i, &status, 1);
        if (rc < 0) {
            flash_area_close(fap_stat);
            return -1;
        }

        if (status == erased_val) {
            if (found && (found_idx == -1)) {
                found_idx = (int)i;
            }
        } else {
            last_status = status;

            if (!found) {
                found = true;
            } else if (found_idx > 0) {
                invalid = true;
                break;
            } else {
                /* No action required */
            }
        }
    }

    if (invalid) {
        /* This means there was an error writing status on the last
         * swap. Tell user and move on to validation!
         */
#if !defined(__BOOTSIM__)
        BOOT_LOG_ERR("Detected inconsistent status!");
#endif

#if !defined(MCUBOOT_VALIDATE_PRIMARY_SLOT)
        /* With validation of the primary slot disabled, there is no way
         * to be sure the swapped primary slot is OK, so abort!
         */
        FIH_PANIC;
#endif
    }

    if (found_idx == -1) {
        /* no swap status found; nothing to do */
    }
    else {
        uint8_t image_index = BOOT_CURR_IMG(state);
        rc = boot_read_swap_size((int32_t)image_index, &bs->swap_size);
        if (rc < 0) {
            flash_area_close(fap_stat);
            return -1;
        }

#ifdef MCUBOOT_SWAP_USING_MOVE
        /* get image size in blocks */
        uint32_t move_entries = bs->swap_size / state->write_sz + (uint32_t)(bs->swap_size % state->write_sz != 0u);

        if (found_idx < (int32_t)move_entries) {
            /* continue move sector up operation */
            bs->op = (uint8_t)BOOT_STATUS_OP_MOVE;
            bs->idx = (uint32_t)found_idx;
            bs->state = (uint8_t)last_status;
        } else
#endif /* MCUBOOT_SWAP_USING_MOVE */
        {
            /* resume swap sectors operation */
            last_status++;
            if (last_status > (uint8_t)BOOT_STATUS_STATE_COUNT) {
                last_status = BOOT_STATUS_STATE_0;
                found_idx++;
            }

            bs->op = (uint8_t)BOOT_STATUS_OP_SWAP;
            bs->idx = (uint32_t)found_idx;
            bs->state = (uint8_t)last_status;
        }
    }

    flash_area_close(fap_stat);

    return 0;
}

/* this is internal offset in swap status area */
uint32_t
boot_status_internal_off(const struct boot_status *bs, uint32_t elem_sz)
{
    uint32_t off = (bs->idx - BOOT_STATUS_IDX_0) * elem_sz;

    return off;
}
#endif /* !MCUBOOT_DIRECT_XIP && !MCUBOOT_RAM_LOAD */

#endif /* MCUBOOT_SWAP_USING_STATUS */
