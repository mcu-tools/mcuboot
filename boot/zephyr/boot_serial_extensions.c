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

#include "bootutil/image.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

static int bs_custom_storage_erase(cbor_state_t *cs)
{
    int rc;

    const struct flash_area *fa;

    rc = flash_area_open(FLASH_AREA_ID(storage), &fa);

    if (rc < 0) {
        LOG_ERR("failed to open flash area");
    } else {
        rc = flash_area_erase(fa, 0, FLASH_AREA_SIZE(storage));
        if (rc < 0) {
            LOG_ERR("failed to erase flash area");
        }
        flash_area_close(fa);
    }
    if (rc == 0) {
        rc = MGMT_ERR_OK;
    } else {
        rc = MGMT_ERR_EUNKNOWN;
    }

    map_start_encode(cs, 10);
    tstrx_put(cs, "rc");
    uintx32_put(cs, rc);
    map_end_encode(cs, 10);

    return rc;
}

#ifdef MCUBOOT_MGMT_CUSTOM_IMG_LIST
static int custom_img_status(int image_index, uint32_t slot,char *buffer,
                        ssize_t len)
{
    uint32_t area_id;
    struct flash_area const *fap;
    struct image_header hdr;
    int rc;
    int img_install_stat = 0;

    area_id = flash_area_id_from_multi_image_slot(image_index, slot);

    rc = flash_area_open(area_id, &fap);
    if (rc) {
        return rc;
    }

    rc = flash_area_read(fap, 0, &hdr, sizeof(hdr));
    if (rc) {
        goto func_end;
    }

    if (hdr.ih_magic == IMAGE_MAGIC) {
        snprintf(buffer, len, "ver=%d.%d.%d.%d,install_stat=%d",
                 hdr.ih_ver.iv_major,
                 hdr.ih_ver.iv_minor,
                 hdr.ih_ver.iv_revision,
                 hdr.ih_ver.iv_build_num,
                 img_install_stat);
    } else {
        rc = 1;
    }

func_end:
    flash_area_close(fap);
    return rc;
}

static int bs_custom_img_list(cbor_state_t *cs)
{
    int rc = 0;
    char tmpbuf[64];	/* Buffer should fit version and flags */

    map_start_encode(cs, 10);

    for (int img = 0; img < MCUBOOT_IMAGE_NUMBER; img++) {
        for (int slot = 0; slot < 2; slot++) {
            rc = custom_img_status(img, slot, tmpbuf, sizeof(tmpbuf));

            intx32_put(cs, img * 2 + slot + 1);
            if (rc == 0) {
                tstrx_put_term(cs, tmpbuf);
            } else {
                tstrx_put_term(cs, "");
            }
        }
    }

    tstrx_put(cs, "rc");
    uintx32_put(cs, MGMT_ERR_OK);
    map_end_encode(cs, 10);

    return rc;
}

#ifndef ZEPHYR_MGMT_GRP_BASIC_CMD_IMAGE_LIST
    #define ZEPHYR_MGMT_GRP_BASIC_CMD_IMAGE_LIST 1
#endif
#endif /*MCUBOOT_MGMT_CUSTOM_IMG_LIST*/

int bs_peruser_system_specific(const struct nmgr_hdr *hdr, const char *buffer,
                               int len, cbor_state_t *cs)
{
    int mgmt_rc = MGMT_ERR_ENOTSUP;

    if (hdr->nh_group == ZEPHYR_MGMT_GRP_BASE) {
        if (hdr->nh_op == NMGR_OP_WRITE) {
            if (hdr->nh_id == ZEPHYR_MGMT_GRP_BASIC_CMD_ERASE_STORAGE) {
                mgmt_rc = bs_custom_storage_erase(cs);
            }
        } else if (hdr->nh_op == NMGR_OP_READ) {
#ifdef MCUBOOT_MGMT_CUSTOM_IMG_LIST
            if (hdr->nh_id == ZEPHYR_MGMT_GRP_BASIC_CMD_IMAGE_LIST) {
                mgmt_rc = bs_custom_img_list(cs);
            }
#endif
        }
    }

    if (mgmt_rc == MGMT_ERR_ENOTSUP) {
        map_start_encode(cs, 10);
        tstrx_put(cs, "rc");
        uintx32_put(cs, mgmt_rc);
        map_end_encode(cs, 10);
    }

    return MGMT_ERR_OK;
}
