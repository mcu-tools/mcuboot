/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2023 Arm Limited
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES)

#include "encrypted_priv.h"

int
boot_enc_init(struct enc_key_data *enc_state, uint8_t slot)
{
    bootutil_aes_ctr_init(&enc_state[slot].aes_ctr);
    return 0;
}

int
boot_enc_drop(struct enc_key_data *enc_state, uint8_t slot)
{
    bootutil_aes_ctr_drop(&enc_state[slot].aes_ctr);
    return 0;
}

int
boot_enc_set_key(struct enc_key_data *enc_state, uint8_t slot,
        const struct boot_status *bs)
{
    int rc;

    rc = bootutil_aes_ctr_set_key(&enc_state[slot].aes_ctr, bs->enckey[slot]);
    if (rc != 0) {
        boot_enc_drop(enc_state, slot);
        enc_state[slot].valid = 0;
        return -1;
    }

    enc_state[slot].valid = 1;

    return 0;
}

/*
 * Load encryption key.
 */
int
boot_enc_load(struct enc_key_data *enc_state, int image_index,
        const struct image_header *hdr, const struct flash_area *fap,
        struct boot_status *bs)
{
    uint32_t off;
    uint16_t len;
    struct image_tlv_iter it;
#if MCUBOOT_SWAP_SAVE_ENCTLV
    uint8_t *buf;
#else
    uint8_t buf[EXPECTED_ENC_LEN];
#endif
    uint8_t slot;
    int rc;

    rc = flash_area_id_to_multi_image_slot(image_index, flash_area_get_id(fap));
    if (rc < 0) {
        return rc;
    }
    slot = rc;

    /* Already loaded... */
    if (enc_state[slot].valid) {
        return 1;
    }

    /* Initialize the AES context */
    boot_enc_init(enc_state, slot);

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, EXPECTED_ENC_TLV, false);
    if (rc) {
        return -1;
    }

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        return rc;
    }

    if (len != EXPECTED_ENC_LEN) {
        return -1;
    }

#if MCUBOOT_SWAP_SAVE_ENCTLV
    buf = bs->enctlv[slot];
    memset(buf, 0xff, BOOT_ENC_TLV_ALIGN_SIZE);
#endif

    rc = flash_area_read(fap, off, buf, EXPECTED_ENC_LEN);
    if (rc) {
        return -1;
    }

    return boot_enc_decrypt(buf, bs->enckey[slot]);
}

bool
boot_enc_valid(struct enc_key_data *enc_state, int image_index,
        const struct flash_area *fap)
{
    int rc;

    rc = flash_area_id_to_multi_image_slot(image_index, flash_area_get_id(fap));
    if (rc < 0) {
        /* can't get proper slot number - skip encryption, */
        /* postpone the error for a upper layer */
        return false;
    }

    return enc_state[rc].valid;
}

void
boot_encrypt(struct enc_key_data *enc_state, int image_index,
        const struct flash_area *fap, uint32_t off, uint32_t sz,
        uint32_t blk_off, uint8_t *buf)
{
    struct enc_key_data *enc;
    uint8_t nonce[16];
    int rc;

    /* boot_copy_region will call boot_encrypt with sz = 0 when skipping over
       the TLVs. */
    if (sz == 0) {
       return;
    }

    memset(nonce, 0, 12);
    off >>= 4;
    nonce[12] = (uint8_t)(off >> 24);
    nonce[13] = (uint8_t)(off >> 16);
    nonce[14] = (uint8_t)(off >> 8);
    nonce[15] = (uint8_t)off;

    rc = flash_area_id_to_multi_image_slot(image_index, flash_area_get_id(fap));
    if (rc < 0) {
        assert(0);
        return;
    }

    enc = &enc_state[rc];
    assert(enc->valid == 1);
    bootutil_aes_ctr_encrypt(&enc->aes_ctr, nonce, buf, sz, blk_off, buf);
}

/**
 * Clears encrypted state after use.
 */
void
boot_enc_zeroize(struct enc_key_data *enc_state)
{
    uint8_t slot;
    for (slot = 0; slot < BOOT_NUM_SLOTS; slot++) {
        (void)boot_enc_drop(enc_state, slot);
    }
    memset(enc_state, 0, sizeof(struct enc_key_data) * BOOT_NUM_SLOTS);
}

#endif /* MCUBOOT_ENC_IMAGES */
