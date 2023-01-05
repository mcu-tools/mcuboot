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
        size >= flash_area_get_size(fa_p)) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

#ifdef MCUBOOT_ENC_IMAGES

/**
 * Validate hash of a primary boot image doing on the fly decryption as well
 *
 * @param[in]	fa_p	flash area pointer
 * @param[in]	hdr	boot image header pointer
 *
 * @return		FIH_SUCCESS on success, error code otherwise
 */
inline static fih_ret
boot_image_validate_encrypted(const struct flash_area *fa_p,
                    struct image_header *hdr)
{
    static uint8_t tmpbuf[BOOT_TMPBUF_SZ];
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    struct boot_loader_state boot_data;
    struct boot_loader_state *state = &boot_data;
    struct boot_status _bs;
    struct boot_status *bs = &_bs;
    uint8_t image_index;
    int rc;

    memset(&boot_data, 0, sizeof(struct boot_loader_state));
    image_index = BOOT_CURR_IMG(state);
    if (MUST_DECRYPT(fa_p, image_index, hdr)) {
        rc = boot_enc_load(BOOT_CURR_ENC(state), image_index, hdr, fa_p, bs);
        if (rc < 0) {
            FIH_RET(fih_rc);
        }
        if (rc == 0 && boot_enc_set_key(BOOT_CURR_ENC(state), 0, bs)) {
            FIH_RET(fih_rc);
        }
    }
    FIH_CALL(bootutil_img_validate, fih_rc, BOOT_CURR_ENC(state), image_index,
             hdr, fa_p, tmpbuf, BOOT_TMPBUF_SZ, NULL, 0, NULL);

    FIH_RET(fih_rc);
}

/*
 * Compute the total size of the given image.  Includes the size of
 * the TLVs.
 */
static int
read_image_size(const struct flash_area *fa_p,
        struct image_header *hdr,
        uint32_t *size)
{
    struct image_tlv_info info;
    uint32_t off;
    uint32_t protect_tlv_size;
    int rc;

    off = BOOT_TLV_OFF(hdr);

    if (flash_area_read(fa_p, off, &info, sizeof(info))) {
        rc = BOOT_EFLASH;
        goto done;
    }

    protect_tlv_size = hdr->ih_protect_tlv_size;
    if (info.it_magic == IMAGE_TLV_PROT_INFO_MAGIC) {
        if (protect_tlv_size != info.it_tlv_tot) {
            rc = BOOT_EBADIMAGE;
            goto done;
        }

        if (flash_area_read(fa_p, off + info.it_tlv_tot, &info, sizeof(info))) {
            rc = BOOT_EFLASH;
            goto done;
        }
    } else if (protect_tlv_size != 0) {
        rc = BOOT_EBADIMAGE;
        goto done;
    }

    if (info.it_magic != IMAGE_TLV_INFO_MAGIC) {
        rc = BOOT_EBADIMAGE;
        goto done;
    }

    *size = off + protect_tlv_size + info.it_tlv_tot;
    rc = 0;

done:
    return rc;
}


/* Get the SOC's flash erase block size from the DTS, fallback to 1024. */
#define SOC_FLASH_ERASE_BLK_SZ \
         DT_PROP_OR(DT_CHOSEN(zephyr_flash), erase_block_size,1024)

/**
 * reads, decrypts in RAM & write back the decrypted image in the same region
 * This function is NOT power failsafe since the image is decrypted in the RAM
 * buffer.
 *
 * @param flash_area            The ID of the source flash area.
 * @param off_src               The offset within the flash area to
 *                                  copy from.
 * @param sz                    The number of bytes to copy. should match erase sector
 *
 * @return                      0 on success; nonzero on failure.
 */
int
decrypt_region_inplace(struct boot_loader_state *state,
                 const struct flash_area *fap,
                 struct image_header *hdr,
                 uint32_t off, uint32_t sz)
{
    uint32_t bytes_copied;
    int chunk_sz;
    int rc;
    uint32_t tlv_off;
    size_t blk_off;
    uint16_t idx;
    uint32_t blk_sz;
    uint8_t image_index;

    static uint8_t buf[SOC_FLASH_ERASE_BLK_SZ] __attribute__((aligned));
    assert(sz <= sizeof buf);

    bytes_copied = 0;
    while (bytes_copied < sz) {
        if (sz - bytes_copied > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = sz - bytes_copied;
        }

        rc = flash_area_read(fap, off + bytes_copied, buf, chunk_sz);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        image_index = BOOT_CURR_IMG(state);
        if (IS_ENCRYPTED(hdr)) {
            blk_sz = chunk_sz;
            idx = 0;
            if (off + bytes_copied < hdr->ih_hdr_size) {
                /* do not decrypt header */
                if (hdr->ih_hdr_size > (off + bytes_copied + chunk_sz)) {
                    /* all bytes in header, skip decryption */
                    blk_sz = 0;
                }
                else {
                    blk_sz = off + bytes_copied + chunk_sz - hdr->ih_hdr_size;
                }

                blk_off = 0;
                idx = hdr->ih_hdr_size;
            } else {
                blk_off = ((off + bytes_copied) - hdr->ih_hdr_size) & 0xf;
            }
            tlv_off = BOOT_TLV_OFF(hdr);
            if (off + bytes_copied + chunk_sz > tlv_off) {
                /* do not decrypt TLVs */
                if (off + bytes_copied >= tlv_off) {
                    blk_sz = 0;
                } else {
                    blk_sz = tlv_off - (off + bytes_copied);
                }
            }
            boot_encrypt(BOOT_CURR_ENC(state), image_index, fap,
                    (off + bytes_copied + idx) - hdr->ih_hdr_size, blk_sz,
                    blk_off, &buf[idx]);
        }
        rc = flash_area_erase(fap, off + bytes_copied, chunk_sz);
        if (rc != 0) {
            return BOOT_EFLASH;
        }
        rc = flash_area_write(fap, off + bytes_copied, buf, chunk_sz);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        bytes_copied += chunk_sz;

        MCUBOOT_WATCHDOG_FEED();
    }

    return 0;
}

/**
 * Check if a image was encrypted into the first slot, and decrypt it
 * in place. this operation is not power failsafe.
 *
 * The operation is done by checking the last flash sector, and using it as a
 * temporarely scratch partition. The
 *
 * @param[in]	fa_p	flash area pointer
 * @param[in]	hdr	boot image header pointer
 *
 * @return		FIH_SUCCESS on success, error code otherwise
 */
inline static fih_ret
decrypt_image_inplace(const struct flash_area *fa_p,
                     struct image_header *hdr)
{
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    int rc;
    struct boot_loader_state boot_data;
    struct boot_loader_state *state = &boot_data;
    struct boot_status _bs;
    struct boot_status *bs = &_bs;
    size_t size;
    size_t sect_size;
    size_t sect_count;
    size_t sect;
    uint8_t image_index;
    struct flash_sector sector;

    memset(&boot_data, 0, sizeof(struct boot_loader_state));
    memset(&_bs, 0, sizeof(struct boot_status));

    /* Get size from last sector to know page/sector erase size */
    rc = flash_area_get_sector(fap, boot_status_off(fa_p), &sector);


    image_index = BOOT_CURR_IMG(state);

    if (MUST_DECRYPT(fa_p, image_index, hdr)) {
#if 0 //Skip this step?, the image will just not boot if it's not decrypted properly
         /* First check if the encrypted image is a good image before decrypting */
        FIH_CALL(boot_image_validate_encrypted,fih_rc,_fa_p,&_hdr);
        if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
             FIH_RET(fih_rc);
        }
#endif
        memset(&boot_data, 0, sizeof(struct boot_loader_state));
        /* Load the encryption keys into cache */
        rc = boot_enc_load(BOOT_CURR_ENC(state), image_index, hdr, fa_p, bs);
        if (rc < 0) {
            FIH_RET(fih_rc);
        }
        if (rc == 0 && boot_enc_set_key(BOOT_CURR_ENC(state), 0, bs)) {
            FIH_RET(fih_rc);
        }
    }
    else
    {
        /* Expected encrypted image! */
        FIH_RET(fih_rc);
    }

    uint32_t src_size = 0;
    rc = read_image_size(fa_p,hdr, &src_size);
    if (rc != 0) {
        FIH_RET(fih_rc);
    }

    sect_size = sector.fs_size;
    sect_count = fa_p->fa_size / sect_size;
    for (sect = 0, size = 0; size < src_size && sect < sect_count; sect++) {
        rc = decrypt_region_inplace(state, fa_p,hdr, size, sect_size);
        if (rc != 0) {
            FIH_RET(fih_rc);
        }
        size += sect_size;
    }

    fih_rc = FIH_SUCCESS;
    FIH_RET(fih_rc);
}

int
boot_handle_enc_fw()
{
    int rc = -1;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    rc = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &_fa_p);
    assert(rc == 0);

    rc = boot_image_load_header(_fa_p, &_hdr);
    if (rc != 0) {
        goto out;
    }

    if (IS_ENCRYPTED(&_hdr)) {
        //encrypted, we need to decrypt in place
        FIH_CALL(decrypt_image_inplace,fih_rc,_fa_p,&_hdr);
        if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
            rc = -1;
            goto out;
        }
    }
    else
    {
        rc = 0;
    }

out:
    flash_area_close(_fa_p);
    return rc;
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
