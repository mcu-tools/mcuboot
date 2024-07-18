/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020-2023 Nordic Semiconductor ASA
 * Copyright (c) 2020 Arm Limited
 */

#include <assert.h>
#include "bootutil/image.h"
#include <../src/bootutil_priv.h>
#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/enc_key.h"

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_ENC_IMAGES

BOOT_LOG_MODULE_DECLARE(serial_encryption);

fih_ret
boot_image_validate_encrypted(const struct flash_area *fa_p,
                              struct image_header *hdr, uint8_t *buf,
                              uint16_t buf_size)
{
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    struct boot_loader_state boot_data;
    struct boot_loader_state *state = &boot_data;
    struct boot_status _bs;
    struct boot_status *bs = &_bs;
    uint8_t image_index;
    int rc;

    memset(&boot_data, 0, sizeof(struct boot_loader_state));
    image_index = BOOT_CURR_IMG(state);
    if(IS_ENCRYPTED(hdr)) {
        rc = boot_enc_load(BOOT_CURR_ENC(state), 1, hdr, fa_p, bs);
        if (rc < 0) {
            FIH_RET(fih_rc);
        }
        rc = boot_enc_set_key(BOOT_CURR_ENC(state), 1, bs);
        if (rc < 0) {
            FIH_RET(fih_rc);
        }
    }
    FIH_CALL(bootutil_img_validate, fih_rc, BOOT_CURR_ENC(state), image_index,
             hdr, fa_p, buf, buf_size, NULL, 0, NULL);

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
static int
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
    int slot = flash_area_id_to_multi_image_slot(BOOT_CURR_IMG(state),
                                                 flash_area_get_id(fap));
    uint8_t buf[sz] __attribute__((aligned));
    assert(sz <= sizeof buf);
    assert(slot >= 0);

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
            boot_enc_decrypt(BOOT_CURR_ENC(state), slot,
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
    struct flash_sector sector;

    memset(&boot_data, 0, sizeof(struct boot_loader_state));
    memset(&_bs, 0, sizeof(struct boot_status));

    /* Get size from last sector to know page/sector erase size */
    rc = flash_area_get_sector(fa_p, boot_status_off(fa_p), &sector);


    if(IS_ENCRYPTED(hdr)) {
#if 0 //Skip this step?, the image will just not boot if it's not decrypted properly
        static uint8_t tmpbuf[BOOT_TMPBUF_SZ];
         /* First check if the encrypted image is a good image before decrypting */
        FIH_CALL(boot_image_validate_encrypted,fih_rc,fa_p,&_hdr,tmpbuf,BOOT_TMPBUF_SZ);
        if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
             FIH_RET(fih_rc);
        }
#endif
        memset(&boot_data, 0, sizeof(struct boot_loader_state));
        /* Load the encryption keys into cache */
        rc = boot_enc_load(BOOT_CURR_ENC(state), 0, hdr, fa_p, bs);
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

    /* TODO: This assumes every sector has an equal size, should instead use
     * flash_area_get_sectors() to get the size of each sector and iterate
     * over it.
     */
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
boot_handle_enc_fw(const struct flash_area *flash_area)
{
    int rc = -1;
    struct image_header _hdr = { 0 };
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    rc = boot_image_load_header(flash_area, &_hdr);
    if (rc != 0) {
        goto out;
    }

    if (IS_ENCRYPTED(&_hdr)) {
        //encrypted, we need to decrypt in place
        FIH_CALL(decrypt_image_inplace,fih_rc,flash_area,&_hdr);
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
    return rc;
}

#endif
