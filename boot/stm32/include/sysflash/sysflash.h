#ifndef H_SYSFLASH_H__
#define H_SYSFLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "mcuboot_config.h"
//#define FLASH_AREA_BOOTLOADER_ID         0
#define FLASH_AREA_IMAGE_PRIMARY_ID      0
#define FLASH_AREA_IMAGE_SECONDARY_ID    1
#define FLASH_AREA_SCRATCH_ID            2


#if (MCUBOOT_IMAGE_NUMBER == 1)

#define FLASH_AREA_IMAGE_PRIMARY(x)      FLASH_AREA_IMAGE_PRIMARY_ID
#define FLASH_AREA_IMAGE_SECONDARY(x)    FLASH_AREA_IMAGE_SECONDARY_ID

#else
#error "Image slot and flash area mapping is not defined"
#endif

#define FLASH_AREA_IMAGE_SCRATCH         FLASH_AREA_SCRATCH_ID


#define FLASH_AREA_REBOOT_LOG            255

#ifdef __cplusplus
}
#endif

#endif /* H_SYSFLASH_H__ */
