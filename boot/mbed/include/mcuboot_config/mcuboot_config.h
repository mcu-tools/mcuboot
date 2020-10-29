/*
 * Copyright (c) 2018 Open Source Foundries Limited
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2019-2020 Linaro Limited
 * Copyright (c) 2020 Embedded Planet
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

/*
 * For available configurations and their explanations,
 * see mbed_lib.json.
 */

#define SIGNATURE_TYPE_RSA      0
#define SIGNATURE_TYPE_EC256    1
#define SIGNATURE_TYPE_ED25519  2

/*
 * Signature algorithm
 */
#if (MCUBOOT_SIGNATURE_ALGORITHM == SIGNATURE_TYPE_RSA)
#define MCUBOOT_SIGN_RSA
#  if (MCUBOOT_RSA_SIGNATURE_LENGTH != 2048 && \
       MCUBOOT_RSA_SIGNATURE_LENGTH != 3072)
#    error "Invalid RSA key size (must be 2048 or 3072)"
#  else
#    define MCUBOOT_SIGN_RSA_LEN MCUBOOT_RSA_SIGNATURE_LENGTH
#  endif
#elif (MCUBOOT_SIGNATURE_ALGORITHM == SIGNATURE_TYPE_EC256)
#define MCUBOOT_SIGN_EC256
#elif (MCUBOOT_SIGNATURE_ALGORITHM == SIGNATURE_TYPE_ED25519)
#define MCUBOOT_SIGN_ED25519
#endif

/*
 * Crypto backend
 */
#define MBEDTLS     0
#define TINYCRYPT   1

#if (MCUBOOT_CRYPTO_BACKEND == MBEDTLS)
#define MCUBOOT_USE_MBED_TLS
#elif (MCUBOOT_CRYPTO_BACKEND == TINYCRYPT)
/**
 * XXX TinyCrypt is currently only supported in GCC builds
 * See https://github.com/mcu-tools/mcuboot/pull/791#discussion_r515050672 for more information.
 */
#if !defined(__GNUC__)
#error TinyCrypt is currently only supported in GCC builds for Mbed-OS.
#endif
#define MCUBOOT_USE_TINYCRYPT
#endif

/*
 * Only one image (two slots) supported for now
 */
#define MCUBOOT_IMAGE_NUMBER 1

/*
 * Encrypted Images
 */
#if defined(MCUBOOT_ENCRYPT_RSA) || defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
#define MCUBOOT_ENC_IMAGES
#endif

/*
 * Enabling this option uses newer flash map APIs. This saves RAM and
 * avoids deprecated API usage.
 */
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS

/*
 * No watchdog integration for now
 */
#define MCUBOOT_WATCHDOG_FEED()                 \
    do {                                        \
    } while (0)


#endif /* __MCUBOOT_CONFIG_H__ */
