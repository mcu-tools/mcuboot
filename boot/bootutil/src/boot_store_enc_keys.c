/*
 * Copyright (c) 2026 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_GEN_ENC_KEY)
#include <bootutil/boot_store_enc_keys.h>
#include "bootutil_priv.h"
#include <bootutil/bootutil_macros.h>

#define PRIV_ENC_KEY_ALIGN_SIZE ALIGN_UP(MCUBOOT_PRIV_ENC_KEY_LEN, MCUBOOT_BOOT_MAX_ALIGN)

int
store_priv_enc_key(const unsigned char *key, size_t key_len)
{
    if((key == NULL) || key_len != MCUBOOT_PRIV_ENC_KEY_LEN)
    {
        return -1;
    }

    const struct flash_area *fa;
    int rc = flash_area_open(KEY_STORAGE_ID, &fa);
    if (rc)
    {
        return rc;
    }

    uint32_t pad_off = ALIGN_UP(KEY_STORAGE_BASE, MCUBOOT_BOOT_MAX_ALIGN) - KEY_STORAGE_BASE;
    uint8_t erased_val = flash_area_erased_val(fa);

    rc = boot_erase_region(fa, pad_off, PRIV_ENC_KEY_ALIGN_SIZE, false);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    uint8_t priv_enc_key[PRIV_ENC_KEY_ALIGN_SIZE];
    memset(&priv_enc_key[0], erased_val, PRIV_ENC_KEY_ALIGN_SIZE);
    memcpy(&priv_enc_key[0], key, MCUBOOT_PRIV_ENC_KEY_LEN);

    rc = flash_area_write(fa, pad_off, &priv_enc_key[0], PRIV_ENC_KEY_ALIGN_SIZE);
    flash_area_close(fa);
    if (rc != 0) {
        return BOOT_EFLASH;
    }
    return 0;
}

bool is_key_storage_erased(){
    const struct flash_area *fa;
    if (flash_area_open(KEY_STORAGE_ID, &fa) < 0)
    {
        return false;
    }
    bool ret = bootutil_buffer_is_erased(fa, (const void*)ALIGN_UP(KEY_STORAGE_BASE, MCUBOOT_BOOT_MAX_ALIGN), MCUBOOT_PRIV_ENC_KEY_LEN);
    flash_area_close(fa);
    return ret;
}

#endif /* MCUBOOT_GEN_ENC_KEY */