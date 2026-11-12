/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#ifndef H_MCUBOOT_CONFIG_H_
#define H_MCUBOOT_CONFIG_H_

/*
 * Cryptographic signature verification configuration:
 * Default to ECDSA P-256 with SHA-256 using mbed TLS.
 */
#if !defined(MCUBOOT_SIGN_RSA) && !defined(MCUBOOT_SIGN_EC256) && \
    !defined(MCUBOOT_SIGN_ED25519)
#define MCUBOOT_SIGN_EC256 1
#endif

#if !defined(MCUBOOT_USE_TINYCRYPT) && !defined(MCUBOOT_USE_MBED_TLS)
#define MCUBOOT_USE_MBED_TLS 1
#endif

/*
 * Upgrade strategy:
 * Default to swap using move algorithm, with rollback support.
 * (MCUBOOT_SWAP_USING_SCRATCH is deprecated).
 */
#if !defined(MCUBOOT_SWAP_USING_MOVE) && !defined(MCUBOOT_OVERWRITE_ONLY) && \
    !defined(MCUBOOT_DIRECT_XIP)
#define MCUBOOT_SWAP_USING_MOVE 1
#endif

/*
 * Maximum number of image sectors per slot.
 */
#ifndef MCUBOOT_MAX_IMG_SECTORS
#define MCUBOOT_MAX_IMG_SECTORS 128
#endif

/*
 * Alignment requirements:
 * 8-byte alignment covers flash architectures with up to 64-bit write granularity.
 */
#ifndef MCUBOOT_BOOT_MAX_ALIGN
#define MCUBOOT_BOOT_MAX_ALIGN 8
#endif

/*
 * Number of image pairs supported (primary + secondary = 1 image pair).
 */
#ifndef MCUBOOT_IMAGE_NUMBER
#define MCUBOOT_IMAGE_NUMBER 1
#endif

/*
 * Always validate primary slot before booting.
 */
#ifndef MCUBOOT_VALIDATE_PRIMARY_SLOT
#define MCUBOOT_VALIDATE_PRIMARY_SLOT 1
#endif

/*
 * Use modern flash_area_get_sectors API.
 */
#ifndef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS 1
#endif

/*
 * Watchdog feed hook.
 */
#ifndef MCUBOOT_WATCHDOG_FEED
#define MCUBOOT_WATCHDOG_FEED() do {} while (0)
#endif

/*
 * Enable logging and assertion integration.
 */
#ifndef MCUBOOT_HAVE_LOGGING
#define MCUBOOT_HAVE_LOGGING 1
#endif

#ifndef MCUBOOT_HAVE_ASSERT_H
#define MCUBOOT_HAVE_ASSERT_H 1
#endif

#endif /* H_MCUBOOT_CONFIG_H_ */
