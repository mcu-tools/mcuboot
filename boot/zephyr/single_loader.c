/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Nordic Semiconductor ASA
 */

#include <assert.h>
#include "bootutil/image.h"
#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"

#include "mcuboot_config/mcuboot_config.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

/* Variables passed outside of unit via poiters. */
static const struct flash_area *_fa_p;
static struct image_header _hdr = { 0 };

#ifdef MCUBOOT_VALIDATE_PRIMARY_SLOT
/**
 * Validate hash of a primary boot image.
 *
 * @param[in]	fa_p	flash area pointer
 * @param[in]	hdr	boot image header pointer
 *
 * @return		0 on success, error code otherwise
 */
inline static int
boot_image_validate(const struct flash_area *fa_p,
                    struct image_header *hdr)
{
    static uint8_t tmpbuf[BOOT_TMPBUF_SZ];

    /* NOTE: The enc-state pointer may be NULL only because when there is
     * only one image (BOOT_IMAGE_NUMBER == 1), the code that uses the
     * pointer, within bootutil_img_validate and down the call path,
     * is excluded from compilation.
     */
    /* Validate hash */
    if (bootutil_img_validate(NULL, 0, hdr, fa_p, tmpbuf,
                              BOOT_TMPBUF_SZ, NULL, 0, NULL)) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}
#endif /* MCUBOOT_VALIDATE_PRIMARY_SLOT */


/**
 * Attempts to load image header from flash; verifies flash header fields.
 *
 * @param[in]	fa_p	flash area pointer
 * @param[out] 	hdr	buffer for image header
 *
 * @return		0 on success, error code otherwise
 */
static int
boot_image_load_header(const struct flash_area *fa_p,
                       struct image_header *hdr)
{
    uint32_t size;
    int rc = flash_area_read(fa_p, 0, hdr, sizeof *hdr);

    if (rc != 0) {
        rc = BOOT_EFLASH;
        BOOT_LOG_ERR("Failed reading image header");
	return BOOT_EFLASH;
    }

    if (hdr->ih_magic != IMAGE_MAGIC) {
        BOOT_LOG_ERR("Bad image magic 0x%lx", (unsigned long)hdr->ih_magic);

        return BOOT_EBADIMAGE;
    }

    if (hdr->ih_flags & IMAGE_F_NON_BOOTABLE) {
        BOOT_LOG_ERR("Image not bootable");

        return BOOT_EBADIMAGE;
    }

    if (!boot_u32_safe_add(&size, hdr->ih_img_size, hdr->ih_hdr_size) ||
        size >= fa_p->fa_size) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}


/**
 * Gather information on image and prepare for booting.
 *
 * @parami[out]	rsp	Parameters for booting image, on success
 *
 * @return		0 on success, error code otherwise.
 */
int
boot_go(struct boot_rsp *rsp)
{
    int rc = -1;

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &_fa_p);
    assert(rc == 0);

    rc = boot_image_load_header(_fa_p, &_hdr);
    if (rc != 0)
        goto out;

#ifdef MCUBOOT_VALIDATE_PRIMARY_SLOT
    rc = boot_image_validate(_fa_p, &_hdr);
    if (rc != 0) {
        goto out;
    }
#endif /* MCUBOOT_VALIDATE_PRIMARY_SLOT */

    rsp->br_flash_dev_id = _fa_p->fa_device_id;
    rsp->br_image_off = _fa_p->fa_off;
    rsp->br_hdr = &_hdr;

out:
    flash_area_close(_fa_p);
    return rc;
}
