/* Manual version of auto-generated version. */

#ifndef SYSFLASH_H
#define SYSFLASH_H

#include <stdint.h>
#include "cy_syslib.h"

#ifndef MCUBOOT_IMAGE_NUMBER
#ifdef MCUBootApp
#warning Undefined MCUBOOT_IMAGE_NUMBER. Assuming 1 (single-image).
#endif /* MCUBootApp */
#define MCUBOOT_IMAGE_NUMBER 1
#endif /* MCUBOOT_IMAGE_NUMBER */

#if (MCUBOOT_IMAGE_NUMBER < 1 || MCUBOOT_IMAGE_NUMBER > 2)
#error Unsupported MCUBOOT_IMAGE_NUMBER. Set it to either 1 or 2.
#endif /* (MCUBOOT_IMAGE_NUMBER < 1 || MCUBOOT_IMAGE_NUMBER > 2) */

#define FLASH_AREA_BOOTLOADER               (0)
#define FLASH_AREA_IMAGE_0                  (1u)
#define FLASH_AREA_IMAGE_1                  (2u)
#define FLASH_AREA_IMAGE_SCRATCH            (3u)
#define FLASH_AREA_IMAGE_2                  (4u)
#define FLASH_AREA_IMAGE_3                  (5u)
#define FLASH_AREA_IMAGE_SWAP_STATUS        (7u)


/* it is related to multi-image case */
#define FLASH_AREA_IMAGE_IDX_1              (0u)
#define FLASH_AREA_IMAGE_IDX_2              (1u)

#ifndef CY_BOOT_USE_EXTERNAL_FLASH
/* Uncomment in case you want to use an external flash */
/* #define CY_BOOT_USE_EXTERNAL_FLASH */
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */

#ifndef CY_FLASH_MAP_EXT_DESC
/* Uncomment in case you want to use separately defined table of flash area descriptors */
/* #define CY_FLASH_MAP_EXT_DESC */
#endif /* CY_FLASH_MAP_EXT_DESC */

/* use PDL-defined offset or one from SMFI config */
#define CY_SMIF_BASE_MEM_OFFSET             (0x18000000)

#define CY_FLASH_ALIGN                      (CY_FLASH_SIZEOF_ROW)
#define CY_FLASH_DEVICE_BASE                (CY_FLASH_BASE)

#ifndef CY_BOOT_SCRATCH_SIZE
#ifndef CY_BOOT_USE_EXTERNAL_FLASH
#define CY_BOOT_SCRATCH_SIZE                (0x1000u)
#else /* CY_BOOT_USE_EXTERNAL_FLASH */
#define CY_BOOT_SCRATCH_SIZE                (0x80000)
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
#endif /* CY_BOOT_SCRATCH_SIZE */

#ifndef CY_BOOT_SWAP_STATUS_SIZE
#define CY_BOOT_SWAP_STATUS_SIZE            (BOOT_SWAP_STATUS_SZ_PRIM + BOOT_SWAP_STATUS_SZ_SEC)
#endif /* CY_BOOT_SWAP_STATUS_SIZE */

#ifndef CY_BOOT_BOOTLOADER_SIZE
#define CY_BOOT_BOOTLOADER_SIZE             (0x18000)
#endif /* CY_BOOT_BOOTLOADER_SIZE */

/* Sizes of CY_BOOT_IMAGE_1_SIZE and CY_BOOT_IMAGE_2_SIZE
 * can be defined from build system. Use default values otherwise
*/
#ifndef CY_BOOT_USE_EXTERNAL_FLASH

#ifndef CY_BOOT_IMAGE_1_SIZE
#define CY_BOOT_IMAGE_1_SIZE                (0x10000)
#endif /* CY_BOOT_IMAGE_1_SIZE */
#if (MCUBOOT_IMAGE_NUMBER == 2)
#ifndef CY_BOOT_IMAGE_2_SIZE
#define CY_BOOT_IMAGE_2_SIZE                (0x20000)
#endif /* CY_BOOT_IMAGE_2_SIZE */
#endif /* (MCUBOOT_IMAGE_NUMBER == 2) */

#else /* CY_BOOT_USE_EXTERNAL_FLASH */

#ifndef CY_BOOT_IMAGE_1_SIZE
#define CY_BOOT_IMAGE_1_SIZE                (0xC0000)
#endif /* CY_BOOT_IMAGE_1_SIZE */
#if (MCUBOOT_IMAGE_NUMBER == 2)
#ifndef CY_BOOT_IMAGE_2_SIZE
#define CY_BOOT_IMAGE_2_SIZE                (0xC0000)
#endif /* CY_BOOT_IMAGE_2_SIZE */
#endif /* (MCUBOOT_IMAGE_NUMBER == 2) */

#endif /* CY_BOOT_USE_EXTERNAL_FLASH */

#ifndef CY_BOOT_PRIMARY_1_SIZE
#define CY_BOOT_PRIMARY_1_SIZE              CY_BOOT_IMAGE_1_SIZE
#endif /* CY_BOOT_PRIMARY_1_SIZE */

#ifndef CY_BOOT_SECONDARY_1_SIZE
#define CY_BOOT_SECONDARY_1_SIZE            CY_BOOT_IMAGE_1_SIZE
#endif /* CY_BOOT_SECONDARY_1_SIZE */

#if (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
#ifndef CY_BOOT_PRIMARY_2_SIZE
#define CY_BOOT_PRIMARY_2_SIZE              CY_BOOT_IMAGE_2_SIZE
#endif /* CY_BOOT_PRIMARY_2_SIZE */

#ifndef CY_BOOT_SECONDARY_2_SIZE
#define CY_BOOT_SECONDARY_2_SIZE            CY_BOOT_IMAGE_2_SIZE
#endif /* CY_BOOT_SECONDARY_2_SIZE */
#endif /* (MCUBOOT_IMAGE_NUMBER == 2) */

#ifndef CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET
#define CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET  (0x0u)
#endif /* CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET */

#ifndef CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET
#define CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET  (0x240000u)
#endif /* CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET */

#ifndef CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET
#define CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET      (0x440000u)
#endif /* CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET */

#ifndef CY_BOOT_SECONDARY_1_EXT_MEM_OFFSET
#define CY_BOOT_SECONDARY_1_EXT_MEM_OFFSET  (CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET)
#endif /* CY_BOOT_SECONDARY_1_EXT_MEM_OFFSET */

#ifndef CY_BOOT_SECONDARY_2_EXT_MEM_OFFSET
#define CY_BOOT_SECONDARY_2_EXT_MEM_OFFSET  (CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET)
#endif /* CY_BOOT_SECONDARY_2_EXT_MEM_OFFSET */

#define BOOT_MAX_SWAP_STATUS_SECTORS        (64)

#if (MCUBOOT_IMAGE_NUMBER == 1)

#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0 : \
                                         FLASH_AREA_IMAGE_0)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_1 : \
                                         FLASH_AREA_IMAGE_1)

#elif (MCUBOOT_IMAGE_NUMBER == 2)

#ifndef CY_FLASH_MAP_EXT_DESC
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0 : \
                                        ((x) == 1) ?          \
                                         FLASH_AREA_IMAGE_2 : \
                                         255)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_1 : \
                                        ((x) == 1) ?          \
                                         FLASH_AREA_IMAGE_3 : \
                                         255)
#else
__STATIC_INLINE uint8_t FLASH_AREA_IMAGE_PRIMARY(uint32_t areaID)
{
    uint8_t result;

    if (0U == areaID) {
        result = FLASH_AREA_IMAGE_0;
    }
    else if (1U == areaID) {
        result = FLASH_AREA_IMAGE_2;
    }
    else {
        result = 0xFF;
    }

    return result;
}

__STATIC_INLINE uint8_t FLASH_AREA_IMAGE_SECONDARY(uint32_t areaID)
{
    uint8_t result;

    if (0U == areaID) {
        result = FLASH_AREA_IMAGE_1;
    }
    else if (1U == areaID) {
        result = FLASH_AREA_IMAGE_3;
    }
    else {
        result = 0xFF;
    }

    return result;
}
#endif /* CY_FLASH_MAP_EXT_DESC */
#endif /* (MCUBOOT_IMAGE_NUMBER == ?) */

#define CY_IMG_HDR_SIZE 0x400

#endif /* SYSFLASH_H */
