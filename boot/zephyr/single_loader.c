/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Nordic Semiconductor ASA
 * Copyright (c) 2020 Arm Limited
 */

#include <assert.h>
#include "bootutil/image.h"
#include "bootutil_priv.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/fault_injection_hardening.h"

#include "mcuboot_config/mcuboot_config.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

/* Variables passed outside of unit via poiters. */
static const struct flash_area *_fa_p;
static struct image_header _hdr = { 0 };

#if defined(MCUBOOT_VALIDATE_PRIMARY_SLOT) || defined(MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE)
/**
 * Validate hash of a primary boot image.
 *
 * @param[in]	fa_p	flash area pointer
 * @param[in]	hdr	boot image header pointer
 *
 * @return		FIH_SUCCESS on success, error code otherwise
 */
fih_ret
boot_image_validate(const struct flash_area *fa_p,
                    struct image_header *hdr)
{
    static uint8_t tmpbuf[BOOT_TMPBUF_SZ];
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    /* NOTE: The first argument to boot_image_validate, for enc_state pointer,
     * is allowed to be NULL only because the single image loader compiles
     * with BOOT_IMAGE_NUMBER == 1, which excludes the code that uses
     * the pointer from compilation.
     */
    /* Validate hash */
    if (IS_ENCRYPTED(hdr))
    {
        /* Clear the encrypted flag we didn't supply a key
         * This flag could be set if there was a decryption in place
         * was performed. We will try to validate the image, and if still
         * encrypted the validation will fail, and go in panic mode
         */
        hdr->ih_flags &= ~(ENCRYPTIONFLAGS);
    }
    FIH_CALL(bootutil_img_validate, fih_rc, NULL, 0, hdr, fa_p, tmpbuf,
             BOOT_TMPBUF_SZ, NULL, 0, NULL);

    FIH_RET(fih_rc);
}
#endif /* MCUBOOT_VALIDATE_PRIMARY_SLOT || MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE*/

#if defined(MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE)
inline static fih_ret
boot_image_validate_once(const struct flash_area *fa_p,
                    struct image_header *hdr)
{
    static struct boot_swap_state state;
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    memset(&state, 0, sizeof(struct boot_swap_state));
    rc = boot_read_swap_state(fa_p, &state);
    if (rc != 0)
        FIH_RET(FIH_FAILURE);
    if (state.magic != BOOT_MAGIC_GOOD
            || state.image_ok != BOOT_FLAG_SET) {
        /* At least validate the image once */
        FIH_CALL(boot_image_validate, fih_rc, fa_p, hdr);
        if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
            FIH_RET(FIH_FAILURE);
        }
        if (state.magic != BOOT_MAGIC_GOOD) {
            rc = boot_write_magic(fa_p);
            if (rc != 0)
                FIH_RET(FIH_FAILURE);
        }
        rc = boot_write_image_ok(fa_p);
        if (rc != 0)
            FIH_RET(FIH_FAILURE);
    }
    FIH_RET(FIH_SUCCESS);
}
#endif

/**
 * Gather information on image and prepare for booting.
 *
 * @parami[out]	rsp	Parameters for booting image, on success
 *
 * @return		FIH_SUCCESS on success; nonzero on failure.
 */
fih_ret
boot_go(struct boot_rsp *rsp)
{
    int rc = -1;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &_fa_p);
    assert(rc == 0);

    rc = boot_image_load_header(_fa_p, &_hdr);
    if (rc != 0)
        goto out;

#ifdef MCUBOOT_VALIDATE_PRIMARY_SLOT
    FIH_CALL(boot_image_validate, fih_rc, _fa_p, &_hdr);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        goto out;
    }
#elif defined(MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE)
    FIH_CALL(boot_image_validate_once, fih_rc, _fa_p, &_hdr);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        goto out;
    }
#else
    fih_rc = FIH_SUCCESS;
#endif /* MCUBOOT_VALIDATE_PRIMARY_SLOT */

    rsp->br_flash_dev_id = flash_area_get_device_id(_fa_p);
    rsp->br_image_off = flash_area_get_off(_fa_p);
    rsp->br_hdr = &_hdr;

out:
    flash_area_close(_fa_p);

    FIH_RET(fih_rc);
}
