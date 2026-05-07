/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG
 *
 * Simulator AES-CTR backend for MCUBOOT_USE_CUSTOM_CRYPTO.
 * Thin wrapper around the existing mbedTLS AES-CTR header.
 */

#ifndef SIM_CUSTOM_AES_CTR_H
#define SIM_CUSTOM_AES_CTR_H

/*
 * Reuse the production mbedTLS AES-CTR header directly.
 * It defines bootutil_aes_ctr_context, BOOT_ENC_BLOCK_SIZE, and all
 * bootutil_aes_ctr_* inline functions backed by mbedtls_aes_crypt_ctr.
 */
#include "bootutil/crypto/aes_ctr_mbedtls.h"

#endif /* SIM_CUSTOM_AES_CTR_H */
