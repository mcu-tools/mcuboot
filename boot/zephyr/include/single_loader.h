/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2021-2021 Crodeon Technologies
 *
 */

#ifndef H_SINGLE_LOADER_
#define H_SINGLE_LOADER_
#include "bootutil/fault_injection_hardening.h"

/**
 * Handle an encrypted firmware in the main flash.
 * This will decrypt the image inplace
 */
int boot_handle_enc_fw();

fih_ret boot_image_validate(const struct flash_area *fa_p,
                    struct image_header *hdr);
#endif
