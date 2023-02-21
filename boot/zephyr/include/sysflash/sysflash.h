/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#include <mcuboot_config/mcuboot_config.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/flash_map.h>

#ifndef CONFIG_SINGLE_APPLICATION_SLOT

#if (MCUBOOT_IMAGE_NUMBER == 1)
/*
 * NOTE: the definition below returns the same values for true/false on
 * purpose, to avoid having to mark x as non-used by all callers when
 * running in single image mode.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?                \
                                         FIXED_PARTITION_ID(slot0_partition) : \
                                         FIXED_PARTITION_ID(slot0_partition))
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?                \
                                         FIXED_PARTITION_ID(slot1_partition) : \
                                         FIXED_PARTITION_ID(slot1_partition))
#elif (MCUBOOT_IMAGE_NUMBER == 2)
/* MCUBoot currently supports only up to 2 updateable firmware images.
 * If the number of the current image is greater than MCUBOOT_IMAGE_NUMBER - 1
 * then a dummy value will be assigned to the flash area macros.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?                \
                                         FIXED_PARTITION_ID(slot0_partition) : \
                                        ((x) == 1) ?                \
                                         FIXED_PARTITION_ID(slot2_partition) : \
                                         255)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?                \
                                         FIXED_PARTITION_ID(slot1_partition) : \
                                        ((x) == 1) ?                \
                                         FIXED_PARTITION_ID(slot3_partition) : \
                                         255)
#else
#error "Image slot and flash area mapping is not defined"
#endif

#if !defined(CONFIG_BOOT_SWAP_USING_MOVE)
#define FLASH_AREA_IMAGE_SCRATCH    FIXED_PARTITION_ID(scratch_partition)
#endif

#else /* CONFIG_SINGLE_APPLICATION_SLOT */

#define FLASH_AREA_IMAGE_PRIMARY(x)	FIXED_PARTITION_ID(slot0_partition)
#define FLASH_AREA_IMAGE_SECONDARY(x)	FIXED_PARTITION_ID(slot0_partition)

#endif /* CONFIG_SINGLE_APPLICATION_SLOT */

#endif /* __SYSFLASH_H__ */
