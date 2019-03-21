/*
 * Copyright (c) 2018 Open Source Foundries Limited
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
#elif defined(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256)
#define MCUBOOT_SIGN_EC256
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

#ifdef CONFIG_BOOT_HAVE_LOGGING
#define MCUBOOT_HAVE_LOGGING 1
#endif

#ifdef CONFIG_BOOT_ENCRYPT_RSA
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_RSA
#endif

#ifdef CONFIG_BOOT_BOOTSTRAP
#define MCUBOOT_BOOTSTRAP 1
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

#endif /* __MCUBOOT_CONFIG_H__ */
