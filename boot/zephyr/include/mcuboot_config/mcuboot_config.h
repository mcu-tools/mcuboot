/*
 * Copyright (c) 2018 Open Source Foundries Limited
 * Copyright (c) 2019-2020 Arm Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

/*
 * This file is also included by the simulator, but we don't want to
 * define anything here in simulator builds.
 *
 * Instead of using mcuboot_config.h, the simulator adds MCUBOOT_xxx
 * configuration flags to the compiler command lines based on the
 * values of environment variables. However, the file still must
 * exist, or bootutil won't build.
 */
#ifndef __BOOTSIM__

#ifdef CONFIG_BOOT_SIGNATURE_TYPE_RSA
#define MCUBOOT_SIGN_RSA
#  if (CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN != 2048 && \
       CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN != 3072)
#    error "Invalid RSA key size (must be 2048 or 3072)"
#  else
#    define MCUBOOT_SIGN_RSA_LEN CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN
#  endif
#elif defined(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256)
#define MCUBOOT_SIGN_EC256
#elif defined(CONFIG_BOOT_SIGNATURE_TYPE_ED25519)
#define MCUBOOT_SIGN_ED25519
#endif

#ifdef CONFIG_BOOT_USE_MBEDTLS
#define MCUBOOT_USE_MBED_TLS
#elif defined(CONFIG_BOOT_USE_TINYCRYPT)
#define MCUBOOT_USE_TINYCRYPT
#elif defined(CONFIG_BOOT_USE_CC310)
#define MCUBOOT_USE_CC310
#ifdef CONFIG_BOOT_USE_NRF_CC310_BL
#define MCUBOOT_USE_NRF_CC310_BL
#endif
#endif

#ifdef CONFIG_BOOT_VALIDATE_SLOT0
#define MCUBOOT_VALIDATE_PRIMARY_SLOT
#endif

#ifdef CONFIG_BOOT_UPGRADE_ONLY
#define MCUBOOT_OVERWRITE_ONLY
#define MCUBOOT_OVERWRITE_ONLY_FAST
#endif

#ifdef CONFIG_BOOT_SWAP_USING_MOVE
#define MCUBOOT_SWAP_USING_MOVE 1
#endif

#ifdef CONFIG_LOG
#define MCUBOOT_HAVE_LOGGING 1
#endif

#ifdef CONFIG_BOOT_SWAP_SAVE_ENCTLV
#define MCUBOOT_SWAP_SAVE_ENCTLV 1
#endif

#ifdef CONFIG_BOOT_ENCRYPT_RSA
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_RSA
#endif

#ifdef CONFIG_BOOT_ENCRYPT_EC256
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_EC256
#endif

#ifdef CONFIG_BOOT_ENCRYPT_X25519
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_X25519
#endif

#ifdef CONFIG_BOOT_BOOTSTRAP
#define MCUBOOT_BOOTSTRAP 1
#endif

#ifdef CONFIG_BOOT_USE_BENCH
#define MCUBOOT_USE_BENCH 1
#endif

#ifdef CONFIG_UPDATEABLE_IMAGE_NUMBER
#define MCUBOOT_IMAGE_NUMBER    CONFIG_UPDATEABLE_IMAGE_NUMBER
#else
#define MCUBOOT_IMAGE_NUMBER    1
#endif

#ifdef CONFIG_MCUBOOT_DOWNGRADE_PREVENTION
#define MCUBOOT_DOWNGRADE_PREVENTION 1
#endif

#ifdef CONFIG_MCUBOOT_HW_DOWNGRADE_PREVENTION
#define MCUBOOT_HW_ROLLBACK_PROT
#endif

#ifdef CONFIG_MEASURED_BOOT
#define MCUBOOT_MEASURED_BOOT
#endif

#ifdef CONFIG_BOOT_SHARE_DATA
#define MCUBOOT_DATA_SHARING
#endif

/*
 * Enabling this option uses newer flash map APIs. This saves RAM and
 * avoids deprecated API usage.
 *
 * (This can be deleted when flash_area_to_sectors() is removed instead
 * of simply deprecated.)
 */
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS

#define MCUBOOT_MAX_IMG_SECTORS       CONFIG_BOOT_MAX_IMG_SECTORS

#endif /* !__BOOTSIM__ */

#define MCUBOOT_WATCHDOG_FEED()         \
    do {                                \
        /* TODO: to be implemented */   \
    } while (0)

#endif /* __MCUBOOT_CONFIG_H__ */
