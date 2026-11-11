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
#include <zephyr/devicetree.h>
#include <soc.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/devicetree/partitions.h>

#define FLASH_ALIGN FLASH_WRITE_BLOCK_SIZE

#endif /* !defined(MCUBOOT_TARGET_CONFIG) */

/*
 * Sanity check the target support.
 */
#if !defined(FLASH_ALIGN) || \
    !(PARTITION_EXISTS(slot0_partition)) || \
    !(PARTITION_EXISTS(slot1_partition) || defined(CONFIG_SINGLE_APPLICATION_SLOT) || \
      defined(CONFIG_SINGLE_APPLICATION_SLOT_RAM_LOAD)) || \
    (defined(CONFIG_BOOT_SWAP_USING_SCRATCH) && !PARTITION_EXISTS(scratch_partition))
#error "Target support is incomplete; cannot build mcuboot."
#endif

#if (MCUBOOT_IMAGE_NUMBER > 1) && (!(PARTITION_EXISTS(slot2_partition)) || \
                                     !(PARTITION_EXISTS(slot3_partition)))
#error "Target support is incomplete; cannot build mcuboot."
#if (MCUBOOT_IMAGE_NUMBER > 2) && (!(PARTITION_EXISTS(slot4_partition)) || \
                                     !(PARTITION_EXISTS(slot5_partition)))
#error "Target support is incomplete; cannot build mcuboot."
#if (MCUBOOT_IMAGE_NUMBER > 3) && (!(PARTITION_EXISTS(slot6_partition)) || \
                                     !(PARTITION_EXISTS(slot7_partition)))
#error "Target support is incomplete; cannot build mcuboot."
#if (MCUBOOT_IMAGE_NUMBER > 4) && (!(PARTITION_EXISTS(slot8_partition)) || \
                                     !(PARTITION_EXISTS(slot9_partition)))
#error "Target support is incomplete; cannot build mcuboot."
#if (MCUBOOT_IMAGE_NUMBER > 5) && (!(PARTITION_EXISTS(slot10_partition)) || \
                                     !(PARTITION_EXISTS(slot11_partition)))
#error "Target support is incomplete; cannot build mcuboot."
#if (MCUBOOT_IMAGE_NUMBER > 6) && (!(PARTITION_EXISTS(slot12_partition)) || \
                                     !(PARTITION_EXISTS(slot13_partition)))
#error "Target support is incomplete; cannot build mcuboot."
#if (MCUBOOT_IMAGE_NUMBER > 7) && (!(PARTITION_EXISTS(slot14_partition)) || \
                                     !(PARTITION_EXISTS(slot15_partition)))
#error "Target support is incomplete; cannot build mcuboot."
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#endif /* H_TARGETS_TARGET_ */
