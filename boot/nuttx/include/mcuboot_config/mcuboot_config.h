/****************************************************************************
 * boot/nuttx/include/mcuboot_config/mcuboot_config.h
 *
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

#ifndef __BOOT_NUTTX_INCLUDE_MCUBOOT_CONFIG_MCUBOOT_CONFIG_H
#define __BOOT_NUTTX_INCLUDE_MCUBOOT_CONFIG_MCUBOOT_CONFIG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_MCUBOOT_WATCHDOG
#  include "watchdog/watchdog.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Signature types
 *
 * You must choose exactly one signature type.
 */

/* Uncomment for RSA signature support */

/* #define MCUBOOT_SIGN_RSA */

/* Uncomment for ECDSA signatures using curve P-256. */

/* #define MCUBOOT_SIGN_EC256 */

/* Upgrade mode
 *
 * The default is to support A/B image swapping with rollback.  Other modes
 * with simpler code path, which only supports overwriting the existing image
 * with the update image or running the newest image directly from its flash
 * partition, are also available.
 *
 * You can enable only one mode at a time from the list below to override
 * the default upgrade mode.
 */

/* Use image swap without using scratch area.*/

#ifdef CONFIG_MCUBOOT_SWAP_USING_MOVE
#  define MCUBOOT_SWAP_USING_MOVE 1
#endif

/* Enable the overwrite-only code path. */

#ifdef CONFIG_MCUBOOT_OVERWRITE_ONLY
#  define MCUBOOT_OVERWRITE_ONLY
#endif

/* Only erase and overwrite those primary slot sectors needed
 * to install the new image, rather than the entire image slot.
 */

#ifdef CONFIG_MCUBOOT_OVERWRITE_ONLY_FAST
#  define MCUBOOT_OVERWRITE_ONLY_FAST
#endif

/* Enable the direct-xip code path. */

#ifdef CONFIG_MCUBOOT_DIRECT_XIP
#  define MCUBOOT_DIRECT_XIP
#endif

/* Enable the revert mechanism in direct-xip mode. */

#ifdef CONFIG_MCUBOOT_DIRECT_XIP_REVERT
#  define MCUBOOT_DIRECT_XIP_REVERT
#endif

/* Enable the ram-load code path. */

#ifdef CONFIG_MCUBOOT_RAM_LOAD
#  define MCUBOOT_RAM_LOAD
#endif

/* Enable bootstrapping the erased primary slot from the secondary slot */

#ifdef CONFIG_MCUBOOT_BOOTSTRAP
#  define MCUBOOT_BOOTSTRAP
#endif

/* Cryptographic settings
 *
 * You must choose between mbedTLS and Tinycrypt as source of
 * cryptographic primitives. Other cryptographic settings are also
 * available.
 */

#ifdef CONFIG_MCUBOOT_USE_MBED_TLS
#  define MCUBOOT_USE_MBED_TLS
#endif

#ifdef CONFIG_MCUBOOT_USE_TINYCRYPT
#  define MCUBOOT_USE_TINYCRYPT
#endif

/* Always check the signature of the image in the primary slot before
 * booting, even if no upgrade was performed. This is recommended if the boot
 * time penalty is acceptable.
 */

#define MCUBOOT_VALIDATE_PRIMARY_SLOT

/* Flash abstraction */

/* Uncomment if your flash map API supports flash_area_get_sectors().
 * See the flash APIs for more details.
 */

#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS

/* Default maximum number of flash sectors per image slot; change
 * as desirable.
 */

#define MCUBOOT_MAX_IMG_SECTORS 512

/* Default number of separately updateable images; change in case of
 * multiple images.
 */

#define MCUBOOT_IMAGE_NUMBER 1

/* Logging */

/* If logging is enabled the following functions must be defined by the
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

#ifdef CONFIG_MCUBOOT_ENABLE_LOGGING
#  define MCUBOOT_HAVE_LOGGING
#endif

/* Assertions */

/* Uncomment if your platform has its own mcuboot_config/mcuboot_assert.h.
 * If so, it must provide an ASSERT macro for use by bootutil. Otherwise,
 * "assert" is used.
 */

/* #define MCUBOOT_HAVE_ASSERT_H 1 */

/* Watchdog feeding */

/* This macro might be implemented if the OS / HW watchdog is enabled while
 * doing a swap upgrade and the time it takes for a swapping is long enough
 * to cause an unwanted reset. If implementing this, the OS main.c must also
 * enable the watchdog (if required)!
 */

#ifdef CONFIG_MCUBOOT_WATCHDOG

#ifndef CONFIG_MCUBOOT_WATCHDOG_DEVPATH
#  define CONFIG_MCUBOOT_WATCHDOG_DEVPATH "/dev/watchdog0"
#endif

#ifndef CONFIG_MCUBOOT_WATCHDOG_TIMEOUT
#  define CONFIG_MCUBOOT_WATCHDOG_TIMEOUT 10000      /* Watchdog timeout in ms */
#endif

#  define MCUBOOT_WATCHDOG_FEED()       do                           \
                                          {                          \
                                            mcuboot_watchdog_feed(); \
                                          }                          \
                                        while (0)

#else
#  define MCUBOOT_WATCHDOG_FEED()       do                           \
                                          {                          \
                                          }                          \
                                        while (0)
#endif

#endif /* __BOOT_NUTTX_INCLUDE_MCUBOOT_CONFIG_MCUBOOT_CONFIG_H */
