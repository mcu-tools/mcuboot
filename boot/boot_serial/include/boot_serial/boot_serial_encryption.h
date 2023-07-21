/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Nordic Semiconductor ASA
 */

#ifndef H_BOOT_SERIAL_ENCRYPTION_
#define H_BOOT_SERIAL_ENCRYPTION_
#include "bootutil/fault_injection_hardening.h"

/**
 * Validate hash of a primary boot image doing on the fly decryption as well
 *
 * @param[in]   fa_p      flash area pointer
 * @param[in]   hdr       boot image header pointer
 * @param[in]   buf       buffer which is used for validating data
 * @param[in]   buf_size  size of input buffer
 *
 * @return                FIH_SUCCESS on success, error code otherwise
 */
fih_ret
boot_image_validate_encrypted(const struct flash_area *fa_p,
                              struct image_header *hdr, uint8_t *buf,
                              uint16_t buf_size);

/**
 * Handle an encrypted firmware in the main flash.
 * This will decrypt the image inplace
 */
int boot_handle_enc_fw(const struct flash_area *flash_area);

#endif
