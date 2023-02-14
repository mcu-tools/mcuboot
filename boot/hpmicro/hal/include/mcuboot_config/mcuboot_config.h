/*
 * Copyright (c) 2018 Open Source Foundries Limited
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2019-2020 Linaro Limited
 * Copyright (c) 2023 HPMicro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

#include "config.h"
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
#if defined(CONFIG_BOOT_USE_TINYCRYPT)
#  if defined(CONFIG_MBEDTLS) || defined(CONFIG_BOOT_USE_CC310)
#     error "One crypto library implementation allowed at a time."
#  endif
#elif defined(CONFIG_MBEDTLS) && defined(CONFIG_BOOT_USE_CC310)
#     error "One crypto library implementation allowed at a time."
#endif

#ifdef CONFIG_BOOT_USE_MBEDTLS
#define MCUBOOT_USE_MBED_TLS
#elif defined(CONFIG_BOOT_USE_TINYCRYPT)
#elif defined(CONFIG_BOOT_USE_CC310)
#define MCUBOOT_USE_CC310
#ifdef CONFIG_BOOT_USE_NRF_CC310_BL
#define MCUBOOT_USE_NRF_CC310_BL
#endif
#endif

/* Zephyr, regardless of C library used, provides snprintf */
#define MCUBOOT_USE_SNPRINTF 1

#ifdef CONFIG_BOOT_HW_KEY
#define MCUBOOT_HW_KEY
#endif

#ifdef CONFIG_BOOT_VALIDATE_SLOT0
#define MCUBOOT_VALIDATE_PRIMARY_SLOT
#endif

#ifdef CONFIG_BOOT_VALIDATE_SLOT0_ONCE
#define MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE
#endif

#ifdef CONFIG_BOOT_UPGRADE_ONLY
#define MCUBOOT_OVERWRITE_ONLY
#define MCUBOOT_OVERWRITE_ONLY_FAST
#endif

#ifdef CONFIG_SINGLE_APPLICATION_SLOT
#define MCUBOOT_SINGLE_APPLICATION_SLOT 1
#define MCUBOOT_IMAGE_NUMBER    1
#else

#ifdef CONFIG_BOOT_SWAP_USING_MOVE
#define MCUBOOT_SWAP_USING_MOVE 1
#endif

#ifdef CONFIG_BOOT_DIRECT_XIP
#define MCUBOOT_DIRECT_XIP
#endif

#ifdef CONFIG_BOOT_DIRECT_XIP_REVERT
#define MCUBOOT_DIRECT_XIP_REVERT
#endif

#ifdef CONFIG_BOOT_RAM_LOAD
#define MCUBOOT_RAM_LOAD 1
#define IMAGE_EXECUTABLE_RAM_START CONFIG_BOOT_IMAGE_EXECUTABLE_RAM_START
#define IMAGE_EXECUTABLE_RAM_SIZE CONFIG_BOOT_IMAGE_EXECUTABLE_RAM_SIZE
#endif

#ifdef CONFIG_UPDATEABLE_IMAGE_NUMBER
#define MCUBOOT_IMAGE_NUMBER    CONFIG_UPDATEABLE_IMAGE_NUMBER
#else
#define MCUBOOT_IMAGE_NUMBER    1
#endif

#ifdef CONFIG_BOOT_SWAP_SAVE_ENCTLV
#define MCUBOOT_SWAP_SAVE_ENCTLV 1
#endif

#endif /* CONFIG_SINGLE_APPLICATION_SLOT */
#ifdef CONFIG_LOG
#define MCUBOOT_HAVE_LOGGING 1
#endif

#ifdef CONFIG_BOOT_ENCRYPT_RSA
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_RSA
#endif

#ifdef CONFIG_BOOT_ENCRYPT_EC256
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_EC256
#endif

#ifdef CONFIG_BOOT_SERIAL_ENCRYPT_EC256
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_EC256
#endif

#ifdef CONFIG_BOOT_ENCRYPT_X25519
#define MCUBOOT_ENC_IMAGES
#define MCUBOOT_ENCRYPT_X25519
#endif
#define CONFIG_BOOT_BOOTSTRAP
#ifdef CONFIG_BOOT_BOOTSTRAP
#define MCUBOOT_BOOTSTRAP 1
#endif

#ifdef CONFIG_BOOT_USE_BENCH
#define MCUBOOT_USE_BENCH 1
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

#ifdef CONFIG_BOOT_FIH_PROFILE_OFF
#define MCUBOOT_FIH_PROFILE_OFF
#endif

#ifdef CONFIG_BOOT_FIH_PROFILE_LOW
#define MCUBOOT_FIH_PROFILE_LOW
#endif

#ifdef CONFIG_BOOT_FIH_PROFILE_MEDIUM
#define MCUBOOT_FIH_PROFILE_MEDIUM
#endif

#ifdef CONFIG_BOOT_FIH_PROFILE_HIGH
#define MCUBOOT_FIH_PROFILE_HIGH
#endif

#ifdef CONFIG_ENABLE_MGMT_PERUSER
#define MCUBOOT_PERUSER_MGMT_GROUP_ENABLED 1
#else
#define MCUBOOT_PERUSER_MGMT_GROUP_ENABLED 0
#endif

#ifdef CONFIG_BOOT_MGMT_CUSTOM_IMG_LIST
#define MCUBOOT_MGMT_CUSTOM_IMG_LIST
#endif

#ifdef CONFIG_BOOT_MGMT_ECHO
#define MCUBOOT_BOOT_MGMT_ECHO
#endif

#ifdef CONFIG_BOOT_IMAGE_ACCESS_HOOKS
#define MCUBOOT_IMAGE_ACCESS_HOOKS
#endif

#ifdef CONFIG_MCUBOOT_VERIFY_IMG_ADDRESS
#define MCUBOOT_VERIFY_IMG_ADDRESS
#endif

/*
 * The configuration option enables direct image upload with the
 * serial recovery.
 */
#ifdef CONFIG_MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD
#define MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD
#endif

#ifdef CONFIG_BOOT_SERIAL_WAIT_FOR_DFU
#define MCUBOOT_SERIAL_WAIT_FOR_DFU
#endif

/*
 * The option enables code, currently in boot_serial, that attempts
 * to erase flash progressively, as update fragments are received,
 * instead of erasing whole image size of flash area after receiving
 * first frame.
 * Enabling this options prevents stalling the beginning of transfer
 * for the time needed to erase large chunk of flash.
 */
#ifdef CONFIG_BOOT_ERASE_PROGRESSIVELY
#define MCUBOOT_ERASE_PROGRESSIVELY
#endif

/*
 * Enabling this option uses newer flash map APIs. This saves RAM and
 * avoids deprecated API usage.
 *
 * (This can be deleted when flash_area_to_sectors() is removed instead
 * of simply deprecated.)
 */
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS

#if (defined(CONFIG_BOOT_USB_DFU_WAIT) || \
     defined(CONFIG_BOOT_USB_DFU_GPIO))
#  ifndef CONFIG_MULTITHREADING
#    error "USB DFU Requires MULTITHREADING"
#  endif
#endif

#ifdef CONFIG_BOOT_MAX_IMG_SECTORS

#define MCUBOOT_MAX_IMG_SECTORS       CONFIG_BOOT_MAX_IMG_SECTORS

#else
#define MCUBOOT_MAX_IMG_SECTORS       128
#endif

#if CONFIG_BOOT_WATCHDOG_FEED
#include <zephyr/devicetree.h>
#if CONFIG_NRFX_WDT
#include <nrfx_wdt.h>

#define FEED_WDT_INST(id)                                    \
    do {                                                     \
        nrfx_wdt_t wdt_inst_##id = NRFX_WDT_INSTANCE(id);    \
        for (uint8_t i = 0; i < NRF_WDT_CHANNEL_NUMBER; i++) {\
            nrf_wdt_reload_request_set(wdt_inst_##id.p_reg,  \
                (nrf_wdt_rr_register_t)(NRF_WDT_RR0 + i));   \
        }                                                    \
    } while (0)
#if defined(CONFIG_NRFX_WDT0) && defined(CONFIG_NRFX_WDT1)
#define MCUBOOT_WATCHDOG_FEED() \
    do {                        \
        FEED_WDT_INST(0);       \
        FEED_WDT_INST(1);       \
    } while (0)
#elif defined(CONFIG_NRFX_WDT0)
#define MCUBOOT_WATCHDOG_FEED() \
    FEED_WDT_INST(0);
#else /* defined(CONFIG_NRFX_WDT0) && defined(CONFIG_NRFX_WDT1) */
#error "No NRFX WDT instances enabled"
#endif /* defined(CONFIG_NRFX_WDT0) && defined(CONFIG_NRFX_WDT1) */

#elif CONFIG_IWDG_STM32 /* CONFIG_NRFX_WDT */
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

#define MCUBOOT_WATCHDOG_FEED() \
    do {                        \
        const struct device *wdt =                            \
            DEVICE_DT_GET(DT_INST(0, st_stm32_watchdog));     \
        if (device_is_ready(wdt)) {                           \
                wdt_feed(wtd, 0);                             \
        }                                                     \
    } while (0)

#elif DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay) /* CONFIG_IWDG_STM32 */
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

#define MCUBOOT_WATCHDOG_FEED()                               \
    do {                                                      \
        const struct device *wdt =                            \
            DEVICE_DT_GET(DT_ALIAS(watchdog0));               \
        if (device_is_ready(wdt)) {                           \
            wdt_feed(wtd, 0);                             \
        }                                                     \
    } while (0)
#else /* DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay) */
#warning "MCUBOOT_WATCHDOG_FEED() is no-op"
/* No vendor implementation, no-op for historical reasons */
#define MCUBOOT_WATCHDOG_FEED()         \
    do {                                \
    } while (0)
#endif
#else  /* CONFIG_BOOT_WATCHDOG_FEED */
/* Not enabled, no feed activity */
#define MCUBOOT_WATCHDOG_FEED()         \
    do {                                \
    } while (0)

#endif /* CONFIG_BOOT_WATCHDOG_FEED */

#define MCUBOOT_CPU_IDLE() \
  if (!IS_ENABLED(CONFIG_MULTITHREADING)) { \
    k_cpu_idle(); \
  }

#endif /* __MCUBOOT_CONFIG_H__ */
