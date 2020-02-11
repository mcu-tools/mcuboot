/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#include <generated_dts_board.h>
#include <mcuboot_config/mcuboot_config.h>

#if (MCUBOOT_IMAGE_NUMBER == 1)
/*
 * NOTE: the definition below returns the same values for true/false on
 * purpose, to avoid having to mark x as non-used by all callers when
 * running in single image mode.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?                \
                                         DT_FLASH_AREA_IMAGE_0_ID : \
                                         DT_FLASH_AREA_IMAGE_0_ID)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?                \
                                         DT_FLASH_AREA_IMAGE_1_ID : \
                                         DT_FLASH_AREA_IMAGE_1_ID)
#elif (MCUBOOT_IMAGE_NUMBER == 2)
/* MCUBoot currently supports only up to 2 updateable firmware images.
 * If the number of the current image is greater than MCUBOOT_IMAGE_NUMBER - 1
 * then a dummy value will be assigned to the flash area macros.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?                \
                                         DT_FLASH_AREA_IMAGE_0_ID : \
                                        ((x) == 1) ?                \
                                         DT_FLASH_AREA_IMAGE_2_ID : \
                                         255)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?                \
                                         DT_FLASH_AREA_IMAGE_1_ID : \
                                        ((x) == 1) ?                \
                                         DT_FLASH_AREA_IMAGE_3_ID : \
                                         255)
#else
#error "Image slot and flash area mapping is not defined"
#endif

#if !defined(CONFIG_BOOT_SWAP_USING_MOVE)
#define FLASH_AREA_IMAGE_SCRATCH    DT_FLASH_AREA_IMAGE_SCRATCH_ID
#endif

#endif /* __SYSFLASH_H__ */
