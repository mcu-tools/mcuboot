/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#include <mcuboot_config/mcuboot_config.h>
#include "flash_map.h"
#ifndef CONFIG_SINGLE_APPLICATION_SLOT

#define FLASH_AREA_BOOTLOADER 0

#define FLASH_AREA_IMAGE_0_PRIMARY 1
#define FLASH_AREA_IMAGE_0_SECONDARY 2
#define FLASH_AREA_IMAGE_SCRATCH 3
#define FLASH_AREA_IMAGE_1_PRIMARY 4
#define FLASH_AREA_IMAGE_1_SECONDARY 5
#define FLASH_SLOT_DOES_NOT_EXIST 255
#if (MCUBOOT_IMAGE_NUMBER == 1)
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_PRIMARY : \
                                         FLASH_SLOT_DOES_NOT_EXIST)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_SECONDARY : \
                                         FLASH_SLOT_DOES_NOT_EXIST)

#elif (MCUBOOT_IMAGE_NUMBER == 2)
/* MCUBoot currently supports only up to 2 updateable firmware images.
 * If the number of the current image is greater than MCUBOOT_IMAGE_NUMBER - 1
 * then a dummy value will be assigned to the flash area macros.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_PRIMARY : \
                                         FLASH_AREA_IMAGE_1_PRIMARY)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_SECONDARY : \
                                         FLASH_AREA_IMAGE_1_SECONDARY)
#else
#error "Image slot and flash area mapping is not defined"
#endif

#else /* CONFIG_SINGLE_APPLICATION_SLOT */

#define FLASH_AREA_IMAGE_PRIMARY(x)	FLASH_AREA_IMAGE_0_PRIMARY
#define FLASH_AREA_IMAGE_SECONDARY(x)	FLASH_AREA_IMAGE_0_PRIMARY
/* NOTE: Scratch partition is not used by single image DFU but some of
 * functions in common files reference it, so the definitions has been
 * provided to allow compilation of common units.
 */
#define FLASH_AREA_IMAGE_SCRATCH	0

#endif /* CONFIG_SINGLE_APPLICATION_SLOT */

#endif /* __SYSFLASH_H__ */
