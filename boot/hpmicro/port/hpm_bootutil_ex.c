/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "bootutil/bootutil_public.h"
#include "flash_map.h"
#include "sysflash/sysflash.h"

int boot_read_image_state_by_id(int flash_area_id, uint8_t *image_ok)
{
    const struct flash_area *fap;
    int rc;
    uint8_t flag;

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(flash_area_id), &fap);
    if (rc != 0) {
        return -1;
    }

    rc = boot_read_image_ok(fap, &flag);
    flash_area_close(fap);
    *image_ok = flag;
    return rc;
}
