/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

/*
 * Signature types
 *
 * You must choose exactly one signature type - check bootloader.conf
 * configuration file
 */

/* Uncomment for RSA signature support */
#if defined(CONFIG_ESP_SIGN_RSA)
#define MCUBOOT_SIGN_RSA
#  if (CONFIG_ESP_SIGN_RSA_LEN != 2048 && \
       CONFIG_ESP_SIGN_RSA_LEN != 3072)
#    error "Invalid RSA key size (must be 2048 or 3072)"
#  else
#    define MCUBOOT_SIGN_RSA_LEN CONFIG_ESP_SIGN_RSA_LEN
#  endif
#elif defined(CONFIG_ESP_SIGN_EC256)
#define MCUBOOT_SIGN_EC256
#elif defined(CONFIG_ESP_SIGN_ED25519)
#define MCUBOOT_SIGN_ED25519
#endif

#if defined(CONFIG_SECURE_FLASH_ENC_ENABLED)
#define MCUBOOT_BOOT_MAX_ALIGN 32
#endif

/*
 * Upgrade mode
 *
 * The default is to support A/B image swapping with rollback.  Other modes
 * with simpler code path, which only supports overwriting the existing image
 * with the update image or running the newest image directly from its flash
 * partition, are also available.
 *
 * You can enable only one mode at a time from the list below to override
 * the default upgrade mode.
 */

/* Uncomment to enable the overwrite-only code path. */
/* #define MCUBOOT_OVERWRITE_ONLY */

#ifdef MCUBOOT_OVERWRITE_ONLY
/* Uncomment to only erase and overwrite those primary slot sectors needed
 * to install the new image, rather than the entire image slot. */
/* #define MCUBOOT_OVERWRITE_ONLY_FAST */
#endif

/* Uncomment to enable the direct-xip code path. */
/* #define MCUBOOT_DIRECT_XIP */

/* Uncomment to enable the ram-load code path. */
/* #define MCUBOOT_RAM_LOAD */

/*
 * Cryptographic settings
 *
 * You must choose between Mbed TLS and Tinycrypt as source of
 * cryptographic primitives. Other cryptographic settings are also
 * available.
 */

/* Uncomment to use Mbed TLS cryptographic primitives */
#if defined(CONFIG_ESP_USE_MBEDTLS)
#define MCUBOOT_USE_MBED_TLS
#else
/* MCUboot requires the definition of a crypto lib,
 * using Tinycrypt as default */
#define MCUBOOT_USE_TINYCRYPT
#endif

/*
 * Always check the signature of the image in the primary slot before booting,
 * even if no upgrade was performed. This is recommended if the boot
 * time penalty is acceptable.
 */
#define MCUBOOT_VALIDATE_PRIMARY_SLOT

/*
 * Flash abstraction
 */

/* Uncomment if your flash map API supports flash_area_get_sectors().
 * See the flash APIs for more details. */
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS

/* Default maximum number of flash sectors per image slot; change
 * as desirable. */
#define MCUBOOT_MAX_IMG_SECTORS 512

/* Default number of separately updateable images; change in case of
 * multiple images. */
#if defined(CONFIG_ESP_IMAGE_NUMBER)
#define MCUBOOT_IMAGE_NUMBER CONFIG_ESP_IMAGE_NUMBER
#else
#define MCUBOOT_IMAGE_NUMBER 1
#endif

/*
 * Logging
 */

/*
 * If logging is enabled the following functions must be defined by the
 * platform:
 *
 *    MCUBOOT_LOG_MODULE_REGISTER(domain)
 *      Register a new log module and add the current C file to it.
 *
 *    MCUBOOT_LOG_MODULE_DECLARE(domain)
 *      Add the current C file to an existing log module.
 *
 *    MCUBOOT_LOG_ERR(...)
 *    MCUBOOT_LOG_WRN(...)
 *    MCUBOOT_LOG_INF(...)
 *    MCUBOOT_LOG_DBG(...)
 *
 * The function priority is:
 *
 *    MCUBOOT_LOG_ERR > MCUBOOT_LOG_WRN > MCUBOOT_LOG_INF > MCUBOOT_LOG_DBG
 */
#define MCUBOOT_HAVE_LOGGING 1
/* #define MCUBOOT_LOG_LEVEL MCUBOOT_LOG_LEVEL_INFO */

/*
 * Assertions
 */

/* Uncomment if your platform has its own mcuboot_config/mcuboot_assert.h.
 * If so, it must provide an ASSERT macro for use by bootutil. Otherwise,
 * "assert" is used. */
#define MCUBOOT_HAVE_ASSERT_H 1

/*
 * Watchdog feeding
 */

/* This macro might be implemented if the OS / HW watchdog is enabled while
 * doing a swap upgrade and the time it takes for a swapping is long enough
 * to cause an unwanted reset. If implementing this, the OS main.c must also
 * enable the watchdog (if required)!
 */
#include <bootloader_wdt.h>
  #define MCUBOOT_WATCHDOG_FEED() \
      do { \
          bootloader_wdt_feed(); \
      } while (0)

#endif /* __MCUBOOT_CONFIG_H__ */
