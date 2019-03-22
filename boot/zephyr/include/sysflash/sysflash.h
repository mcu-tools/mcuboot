/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#include <generated_dts_board.h>
#include <mcuboot_config/mcuboot_config.h>

extern uint8_t current_image;

#if (MCUBOOT_IMAGE_NUMBER == 1)
#define FLASH_AREA_IMAGE_PRIMARY    DT_FLASH_AREA_IMAGE_0_ID
#define FLASH_AREA_IMAGE_SECONDARY  DT_FLASH_AREA_IMAGE_1_ID
#elif (MCUBOOT_IMAGE_NUMBER == 2)
/* MCUBoot currently supports only up to 2 updateable firmware images.
 * If the number of the current image is greater than MCUBOOT_IMAGE_NUMBER - 1
 * then a dummy value will be assigned to the flash area macros.
 */
#define FLASH_AREA_IMAGE_PRIMARY    ((current_image == 0) ?         \
                                         DT_FLASH_AREA_IMAGE_0_ID : \
                                     (current_image == 1) ?         \
                                         DT_FLASH_AREA_IMAGE_2_ID : \
                                         255 )
#define FLASH_AREA_IMAGE_SECONDARY  ((current_image == 0) ?         \
                                         DT_FLASH_AREA_IMAGE_1_ID : \
                                     (current_image == 1) ?         \
                                         DT_FLASH_AREA_IMAGE_3_ID : \
                                         255 )
#else
#error "Image slot and flash area mapping is not defined"
#endif

#define FLASH_AREA_IMAGE_SCRATCH    DT_FLASH_AREA_IMAGE_SCRATCH_ID

#endif /* __SYSFLASH_H__ */
