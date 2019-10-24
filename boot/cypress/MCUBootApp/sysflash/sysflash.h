/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#define FLASH_DEVICE_INTERNAL_FLASH        (0x7F)

#define FLASH_AREA_BOOTLOADER 0
#define FLASH_AREA_IMAGE_0 1
#define FLASH_AREA_IMAGE_1 2
#define FLASH_AREA_IMAGE_SCRATCH 3
#define FLASH_AREA_IMAGE_2 4
#define FLASH_AREA_IMAGE_3 5

#define CY_FLASH_ALIGN                      (128) // TODO: will fix it to CY_FLASH_SIZEOF_ROW (512) in PSoC6
#define CY_FLASH_DEVICE_BASE                (CY_FLASH_BASE)

#define CY_IMG_HDR_SIZE 0x400

#endif /* __SYSFLASH_H__ */
