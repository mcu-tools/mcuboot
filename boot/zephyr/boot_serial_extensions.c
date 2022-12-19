/*
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>

#include <flash_map_backend/flash_map_backend.h>
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"
#include "../boot_serial/src/boot_serial_priv.h"
#include "../boot_serial/src/zcbor_encode.h"

#include "bootutil/image.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/boot_hooks.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#define ZEPHYR_MGMT_GRP_BASIC (MGMT_GROUP_ID_PERUSER - 1)
#define ZEPHYR_MGMT_GRP_BASIC_CMD_ERASE_STORAGE 0

#ifdef CONFIG_BOOT_MGMT_CUSTOM_STORAGE_ERASE
static int bs_custom_storage_erase(zcbor_state_t *cs)
{
    int rc;

    const struct flash_area *fa;

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
#endif

#ifdef MCUBOOT_MGMT_CUSTOM_IMG_LIST
static int custom_img_status(int image_index, uint32_t slot,char *buffer,
                        ssize_t len)
{
    uint32_t area_id;
    struct flash_area const *fap;
    struct image_header hdr;
    int rc;
    int img_install_stat;

    rc = BOOT_HOOK_CALL(boot_img_install_stat_hook, BOOT_HOOK_REGULAR,
                        image_index, slot, &img_install_stat);
    if (rc == BOOT_HOOK_REGULAR)
    {
        img_install_stat = 0;
    }

    rc = BOOT_HOOK_CALL(boot_read_image_header_hook, BOOT_HOOK_REGULAR,
                        image_index, slot, &hdr);
    if (rc == BOOT_HOOK_REGULAR)
    {
        area_id = flash_area_id_from_multi_image_slot(image_index, slot);

        rc = flash_area_open(area_id, &fap);
        if (rc) {
            return rc;
        }

        rc = flash_area_read(fap, 0, &hdr, sizeof(hdr));

        flash_area_close(fap);
    }

    if (rc == 0) {
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
    }

    return rc;
}

static int bs_custom_img_list(zcbor_state_t *cs)
{
    int rc = 0;
    char tmpbuf[64];	/* Buffer should fit version and flags */

    zcbor_map_start_encode(cs, 10);

    for (int img = 0; img < MCUBOOT_IMAGE_NUMBER; img++) {
        for (int slot = 0; slot < 2; slot++) {
            rc = custom_img_status(img, slot, tmpbuf, sizeof(tmpbuf));

            zcbor_int32_put(cs, img * 2 + slot + 1);
            if (rc == 0) {
                zcbor_tstr_put_term(cs, tmpbuf);
            } else {
                zcbor_tstr_put_lit(cs, "");
            }
        }
    }

    zcbor_tstr_put_lit(cs, "rc");
    zcbor_uint32_put(cs, MGMT_ERR_OK);
    zcbor_map_end_encode(cs, 10);

    return rc;
}

#ifndef ZEPHYR_MGMT_GRP_BASIC_CMD_IMAGE_LIST
    #define ZEPHYR_MGMT_GRP_BASIC_CMD_IMAGE_LIST 1
#endif
#endif /*MCUBOOT_MGMT_CUSTOM_IMG_LIST*/

int bs_peruser_system_specific(const struct nmgr_hdr *hdr, const char *buffer,
                               int len, zcbor_state_t *cs)
{
    int mgmt_rc = MGMT_ERR_ENOTSUP;

    if (hdr->nh_group == ZEPHYR_MGMT_GRP_BASIC) {
        if (hdr->nh_op == NMGR_OP_WRITE) {
#ifdef CONFIG_BOOT_MGMT_CUSTOM_STORAGE_ERASE
            if (hdr->nh_id == ZEPHYR_MGMT_GRP_BASIC_CMD_ERASE_STORAGE) {
                mgmt_rc = bs_custom_storage_erase(cs);
            }
#endif
        } else if (hdr->nh_op == NMGR_OP_READ) {
#ifdef MCUBOOT_MGMT_CUSTOM_IMG_LIST
            if (hdr->nh_id == ZEPHYR_MGMT_GRP_BASIC_CMD_IMAGE_LIST) {
                mgmt_rc = bs_custom_img_list(cs);
            }
#endif
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
