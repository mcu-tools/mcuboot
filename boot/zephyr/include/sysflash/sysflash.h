/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#include <mcuboot_config/mcuboot_config.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/mcuboot_partitions.h>

#ifndef CONFIG_SINGLE_APPLICATION_SLOT

#if !ZEPHYR_MCUBOOT_APP_0_SECONDARY_SLOT_EXISTS
#error "Not in single app slot mode but secondary slot is missing"
#endif

#if (MCUBOOT_IMAGE_NUMBER == 1)
/*
 * NOTE: the definition below returns the same values for true/false on
 * purpose, to avoid having to mark x as non-used by all callers when
 * running in single image mode.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)                            \
          (((x) == 0) ? ZEPHYR_MCUBOOT_APP_0_PRIMARY_SLOT_ID : \
                          ZEPHYR_MCUBOOT_APP_0_PRIMARY_SLOT_ID)

#define FLASH_AREA_IMAGE_SECONDARY(x)                            \
          (((x) == 0) ? ZEPHYR_MCUBOOT_APP_0_SECONDARY_SLOT_ID : \
                          ZEPHYR_MCUBOOT_APP_0_SECONDARY_SLOT_ID)

#elif (MCUBOOT_IMAGE_NUMBER == 2)
/* MCUBoot currently supports only up to 2 updateable firmware images.
 * If the number of the current image is greater than MCUBOOT_IMAGE_NUMBER - 1
 * then a dummy value will be assigned to the flash area macros.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)                                            \
          (((x) == 0) ? ZEPHYR_MCUBOOT_APP_0_PRIMARY_SLOT_ID :               \
                        (((x) == 1) ? ZEPHYR_MCUBOOT_APP_1_PRIMARY_SLOT_ID : \
                                      255)                                   \
          )

#define FLASH_AREA_IMAGE_SECONDARY(x)                                          \
          (((x) == 0) ? ZEPHYR_MCUBOOT_APP_0_SECONDARY_SLOT_ID :               \
                        (((x) == 1) ? ZEPHYR_MCUBOOT_APP_1_SECONDARY_SLOT_ID : \
                                      255)                                     \
          )
#else
#error "Image slot and flash area mapping is not defined"
#endif

#if !defined(CONFIG_BOOT_SWAP_USING_MOVE)
#define FLASH_AREA_IMAGE_SCRATCH    FIXED_PARTITION_ID(scratch_partition)
#endif

#else /* to ifndef CONFIG_SINGLE_APPLICATION_SLOT */
#define FLASH_AREA_IMAGE_PRIMARY(x)                             \
            (ZEPHYR_MCUBOOT_APP_0_PRIMARY_SLOT_ID + (x - x))
#define FLASH_AREA_IMAGE_SECONDARY(x)                           \
            (ZEPHYR_MCUBOOT_APP_0_PRIMARY_SLOT_ID + (x - x))
/* NOTE: Scratch parition is not used by single image DFU but some of
 * functions in common files reference it, so the definitions has been
 * provided to allow compilation of common units.
 */
#define FLASH_AREA_IMAGE_SCRATCH	0

#endif /* to else to ifndef CONFIG_SINGLE_APPLICATION_SLOT */

#endif /* __SYSFLASH_H__ */
