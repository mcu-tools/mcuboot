/*
 * Copyright (c) 2018 Open Source Foundries Limited
 * Copyright (c) 2019-2024 Arm Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

/*
 * Template configuration file for MCUboot.
 *
 * When porting MCUboot to a new target, copy it somewhere that your
 * include path can find it as mcuboot_config/mcuboot_config.h, and
 * make adjustments to suit your platform.
 *
 * For examples, see:
 *
 * boot/zephyr/include/mcuboot_config/mcuboot_config.h
 * boot/mynewt/mcuboot_config/include/mcuboot_config/mcuboot_config.h
 */

/*
 * Hash types
 *
 * You must choose exactly one hash type.
 */

/* Uncomment to use SHA256 hashes */
/* #define MCUBOOT_SHA256 */

/* Uncomment to use SHA512 hashes */
/* #define MCUBOOT_SHA512 */

/*
 * Signature types
 *
 * You must choose exactly one signature type.
 */

/* Uncomment for RSA signature support */
/* #define MCUBOOT_SIGN_RSA */

/* Uncomment for ECDSA signatures using curve P-256. */
/* #define MCUBOOT_SIGN_EC256 */

/* Uncomment for Ed25519 signature support. */
/* #define MCUBOOT_SIGN_ED25519 */

/*
 * Public key handling
 *
 * Choose one or none from the different public key handling options.
 */

/* Uncomment to use key hash(es) instead of incorporating
 * the public key into the code. */
/* #define MCUBOOT_HW_KEY */
/* Uncomment to use builtin key(s) instead of incorporating
 * the public key into the code. */
/* #define MCUBOOT_BUILTIN_KEY */
/* Uncomment to bypass ASN encoding when importing keys, this requires that keys are stored
 * without this encoding, otherwise key loading will fail */
/* #define MCUBOOT_KEY_IMPORT_BYPASS_ASN */

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

/*
 * Uncomment to use swap-using-offset mode, update images are placed starting in the 2nd sector in
 * the update slot, then swapped during a test cycle. This supports reverting if the image is bad
 * but does not support keeping the old image post-confirming an image, as the image data then
 * starts from the first sector, it is faster then swap-using-move. This is the default swapping
 * algorithm for MCUboot. By default, sectors on primary and secondary devices must be the same,
 * but the logical sectors configuration can be used to allow different sector sizes by using a
 * common "software" sector size.
 */
/* #define MCUBOOT_SWAP_USING_OFFSET */

/*
 * Uncomment to use swap-using-move mode, update images are placed starting in the 1st sector in
 * the update slot, then swapped during a test cycle. This supports reverting to the older image
 * even after confirming a new image, it is slower then swap-using-offset and uses ore flash
 * endurance cycles. By default, sectors on primary and secondary devices must be the same, but
 * the logical sectors configuration can be used to allow different sector sizes by using a
 * common "software" sector size.
*/
/* #define MCUBOOT_SWAP_USING_MOVE */

/* Uncomment to enable the overwrite-only code path. */
/* #define MCUBOOT_OVERWRITE_ONLY */

#ifdef MCUBOOT_OVERWRITE_ONLY
/* Uncomment to only erase and overwrite those primary slot sectors needed
 * to install the new image, rather than the entire image slot. */
/* #define MCUBOOT_OVERWRITE_ONLY_FAST */
#endif

/* Uncomment to enable the direct-xip code path. */
/* #define MCUBOOT_DIRECT_XIP */
#ifdef MCUBOOT_DIRECT_XIP
/*
 * Uncomment to enable the revert mechanism in direct-xip mode. With this enabled, images can be
 * tested, if they are marked as confirmed then they will remain (the older image/non-confirmed
 * image will be erased upon device reboot)
 */
/* #define MCUBOOT_DIRECT_XIP_REVERT */
#endif

/* Uncomment to enable the RAM-load code path. */
/* #define MCUBOOT_RAM_LOAD */
#ifdef MCUBOOT_RAM_LOAD
/*
 * Uncomment to enable the revert mechanism in RAM load mode. With this enabled, images can be
 * tested, if they are marked as confirmed then they will remain (the older image/non-confirmed
 * image will be erased upon device reboot)
 */
/* #define MCUBOOT_RAM_LOAD_REVERT */
#endif

/*
 * Cryptographic settings
 *
 * You must choose between mbedTLS and Tinycrypt as source of
 * cryptographic primitives. Other cryptographic settings are also
 * available.
 */

/* Uncomment to use ARM's mbedTLS cryptographic primitives */
/* #define MCUBOOT_USE_MBED_TLS */
/* Uncomment to use Tinycrypt's. */
/* #define MCUBOOT_USE_TINYCRYPT */

/*
 * Encrypted images
 *
 * Uncomment one of the below options (MCUBOOT_ENCRYPT_x) to enable
 * encrypted image upgrades.
 */

/* Uncomment to use RSA-OAEP for key encryption */
/* #define MCUBOOT_ENCRYPT_RSA */
/* Uncomment to use AES-KW for key encryption */
/* #define MCUBOOT_ENCRYPT_KW */
/* Uncomment to use ECIES-P256 for key encryption */
/* #define MCUBOOT_ENCRYPT_EC256 */
/* Uncomment to use ECIES-X25519 for key encryption */
/* #define MCUBOOT_ENCRYPT_X25519 */

/* Uncomment to use a builtin key-encryption key (retrieved from a trusted
 * source - if implemented) instead of a key embedded in the bootloader. */
/* #define MCUBOOT_ENC_BUILTIN_KEY */

#if defined(MCUBOOT_ENCRYPT_RSA)    || \
    defined(MCUBOOT_ENCRYPT_KW)     || \
    defined(MCUBOOT_ENCRYPT_EC256)  || \
    defined(MCUBOOT_ENCRYPT_X25519)
#define MCUBOOT_ENC_IMAGES
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
/* #define MCUBOOT_USE_FLASH_AREA_GET_SECTORS */

/* Default maximum number of flash sectors per image slot; change
 * as desirable. */
#define MCUBOOT_MAX_IMG_SECTORS 128

/* Default number of separately updateable images; change in case of
 * multiple images. */
#define MCUBOOT_IMAGE_NUMBER 1

/* Uncomment and set to override the size of the buffer used to read the
 * image in chunks while its hash is computed (default 256).  Raising it
 * reduces the number of flash reads over an image, which matters on flash
 * that is not memory mapped; lowering it saves RAM.  The buffer is
 * statically allocated. */
/* #define MCUBOOT_BOOT_TMPBUF_SZ 256 */

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

/*
 * Assertions
 */

/* Uncomment if your platform has its own mcuboot_config/mcuboot_assert.h.
 * If so, it must provide an ASSERT macro for use by bootutil. Otherwise,
 * "assert" is used. */
/* #define MCUBOOT_HAVE_ASSERT_H */

/*
 * Watchdog feeding
 */

/* #define MCUBOOT_USE_TLV_ALLOW_LIST */
/* #define MCUBOOT_AES_128 */
/* #define MCUBOOT_AES_256 */
/* #define MCUBOOT_HMAC_SHA512 */
/* #define MCUBOOT_BYPASS_KEY_MATCH */
/* #define MCUBOOT_DECOMPRESS_IMAGES */
/* #define MCUBOOT_HASH_STORAGE_DIRECTLY */
/* #define MCUBOOT_SIGN_PURE */
/* #define MCUBOOT_BOOTSTRAP */
/* #define MCUBOOT_DOWNGRADE_PREVENTION */
/* #define MCUBOOT_HW_ROLLBACK_PROT */
/* #define MCUBOOT_HW_ROLLBACK_PROT_COUNTER_LIMITED */
/* #define MCUBOOT_HW_ROLLBACK_PROT_LOCK */
/* #define MCUBOOT_MEASURED_BOOT */
/* #define MCUBOOT_DATA_SHARING */
/* #define MCUBOOT_CUSTOM_DATA_SHARING_FUNCTION */
/* #define MCUBOOT_DATA_SHARING_BOOTINFO */
/* #define MAX_BOOT_RECORD_SZ <x> */
/* #define MCUBOOT_FIH_PROFILE_OFF */
/* #define MCUBOOT_FIH_PROFILE_LOW */
/* #define MCUBOOT_FIH_PROFILE_MEDIUM */
/* #define MCUBOOT_FIH_PROFILE_HIGH */
/* #define MCUBOOT_PERUSER_MGMT_GROUP_ENABLED 1 */
/* #define MCUBOOT_BOOT_MGMT_ECHO */
/* #define MCUBOOT_BOOT_MGMT_MCUMGR_PARAMS */
/* #define MCUBOOT_IMAGE_ACCESS_HOOKS */
/* #define MCUBOOT_BOOT_GO_HOOKS */
/* #define MCUBOOT_BOOT_IMAGE_JUMP_HOOKS */
/* #define MCUBOOT_FLASH_AREA_HOOKS */
/* #define MCUBOOT_FIND_NEXT_SLOT_HOOKS */
/* #define MCUBOOT_CHECK_HEADER_LOAD_ADDRESS */

/* #define MCUBOOT_SERIAL */
/* #define MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD */
/* #define MCUBOOT_SERIAL_INACTIVITY_TIMEOUT <x> */
/* #define MCUBOOT_SERIAL_WAIT_FOR_DFU */
/* #define MCUBOOT_SERIAL_IMG_GRP_HASH */
/* #define MCUBOOT_SERIAL_IMG_GRP_IMAGE_STATE */
/* #define MCUBOOT_SERIAL_IMG_GRP_SLOT_INFO */
/* #define MCUBOOT_SERIAL_RAW_PROTOCOL */
/* #define MCUBOOT_SERIAL_RAW_PROTOCOL_INPUT_TIMEOUT */
/* #define MCUBOOT_SERIAL_RAW_PROTOCOL_INPUT_TIMEOUT_MS <x> */
/* #define MCUBOOT_SERIAL_RECOVERY */

/* #define MCUBOOT_USB_DFU */
/* #define MCUBOOT_ERASE_PROGRESSIVELY */
/* #define MCUBOOT_SUPPORT_DEV_WITHOUT_ERASE */
/* #define MCUBOOT_SUPPORT_DEV_WITH_ERASE */
/* #define MCUBOOT_MINIMAL_SCRAMBLE */
/* #define MCUBOOT_LOGICAL_SECTOR_SIZE <x> */
/* #define MCUBOOT_VERIFY_LOGICAL_SECTORS */
/* #define MCUBOOT_MAX_IMG_SECTORS       MIN_SECTOR_COUNT */
/* #define MCUBOOT_SERIAL_MAX_RECEIVE_SIZE CONFIG_BOOT_SERIAL_MAX_RECEIVE_SIZE */
/* #define MCUBOOT_SERIAL_UNALIGNED_BUFFER_SIZE CONFIG_BOOT_SERIAL_UNALIGNED_BUFFER_SIZE */
/* #define MCUBOOT_BOOT_MAX_ALIGN CONFIG_MCUBOOT_BOOT_MAX_ALIGN */

/* This macro might be implemented if the OS / HW watchdog is enabled while
 * doing a swap upgrade and the time it takes for a swapping is long enough
 * to cause an unwanted reset. If implementing this, the OS main.c must also
 * enable the watchdog (if required)!
 *
 * #define MCUBOOT_WATCHDOG_FEED()
 *    do { do watchdog feeding here! } while (0)
 */

/* If a OS ports support single thread mode or is bare-metal then:
 * This macro implements call that switches CPU to an idle state, from which
 * the CPU may be woken up by, for example, UART transmission event.
 *
 * Otherwise this macro should be no-op.
 */
#define MCUBOOT_CPU_IDLE() \
    do {                   \
    } while (0)

#endif /* __MCUBOOT_CONFIG_H__ */
