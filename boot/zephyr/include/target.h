/*
 *  Copyright (C) 2017, Linaro Ltd
 *  Copyright (c) 2019, Arm Limited
 *
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_TARGETS_TARGET_
#define H_TARGETS_TARGET_

#ifndef USE_PARTITION_MANAGER

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

#define FLASH_ALIGN FLASH_WRITE_BLOCK_SIZE

#endif /* !defined(MCUBOOT_TARGET_CONFIG) */

#if DT_NODE_HAS_PROP(DT_INST(0, jedec_spi_nor), label)
#define JEDEC_SPI_NOR_0_LABEL DT_LABEL(DT_INST(0, jedec_spi_nor))
#endif

/*
 * Sanity check the target support.
 */
#if (!defined(CONFIG_XTENSA) && !defined(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL)) || \
    (defined(CONFIG_XTENSA) && !defined(JEDEC_SPI_NOR_0_LABEL)) || \
    !defined(FLASH_ALIGN) ||                  \
    !defined(DT_FLASH_AREA_IMAGE_0_OFFSET) || \
    !defined(DT_FLASH_AREA_IMAGE_0_SIZE) || \
    !defined(DT_FLASH_AREA_IMAGE_1_OFFSET) || \
    !defined(DT_FLASH_AREA_IMAGE_1_SIZE) || \
    (!defined(CONFIG_BOOT_SWAP_USING_MOVE) && !defined(DT_FLASH_AREA_IMAGE_SCRATCH_OFFSET)) || \
    (!defined(CONFIG_BOOT_SWAP_USING_MOVE) && !defined(DT_FLASH_AREA_IMAGE_SCRATCH_SIZE))
#error "Target support is incomplete; cannot build mcuboot."
#endif

#if (MCUBOOT_IMAGE_NUMBER == 2) && (!(FLASH_AREA_LABEL_EXISTS(image_2)) || \
                                     !(FLASH_AREA_LABEL_EXISTS(image_3)))
#error "Target support is incomplete; cannot build mcuboot."
#endif

#endif /* ifndef USE_PARTITION_MANAGER */

#endif /* H_TARGETS_TARGET_ */
