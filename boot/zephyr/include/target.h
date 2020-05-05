/*
 *  Copyright (C) 2017, Linaro Ltd
 *  Copyright (c) 2019, Arm Limited
 *
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_TARGETS_TARGET_
#define H_TARGETS_TARGET_

#if defined(MCUBOOT_TARGET_CONFIG)
/*
 * Target-specific definitions are permitted in legacy cases that
 * don't provide the information via DTS, etc.
 */
#include MCUBOOT_TARGET_CONFIG
#else
/*
 * Otherwise, the Zephyr SoC header and the DTS provide most
 * everything we need.
 */
#include <soc.h>
#include <storage/flash_map.h>

#define FLASH_ALIGN FLASH_WRITE_BLOCK_SIZE

#endif /* !defined(MCUBOOT_TARGET_CONFIG) */

/*
 * Sanity check the target support.
 */
#if (!defined(CONFIG_XTENSA) && !defined(DT_FLASH_DEV_NAME)) || \
    (defined(CONFIG_XTENSA) && !defined(DT_JEDEC_SPI_NOR_0_LABEL)) || \
    !defined(FLASH_ALIGN) ||                  \
    !(FLASH_AREA_LABEL_EXISTS(image_0)) || \
    !(FLASH_AREA_LABEL_EXISTS(image_1)) || \
    (!defined(CONFIG_BOOT_SWAP_USING_MOVE) && !(FLASH_AREA_LABEL_EXISTS(image_scratch)))
#error "Target support is incomplete; cannot build mcuboot."
#endif

#if (MCUBOOT_IMAGE_NUMBER == 2) && (!(FLASH_AREA_LABEL_EXISTS(image_2)) || \
                                     !(FLASH_AREA_LABEL_EXISTS(image_3)))
#error "Target support is incomplete; cannot build mcuboot."
#endif

#endif /* H_TARGETS_TARGET_ */
