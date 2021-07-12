/* Manual version of auto-generated version. */

#ifndef SYSFLASH_H
#define SYSFLASH_H

#include <stdint.h>
#include "cy_syslib.h"

#define FLASH_AREA_BOOTLOADER               (0)
#define FLASH_AREA_IMAGE_0                  (1u)
#define FLASH_AREA_IMAGE_1                  (2u)
#define FLASH_AREA_IMAGE_SCRATCH            (3u)
#define FLASH_AREA_IMAGE_2                  (5u)
#define FLASH_AREA_IMAGE_3                  (6u)
#define FLASH_AREA_IMAGE_SWAP_STATUS        (7u)

/* it is related to multi-image case */
#define FLASH_AREA_IMAGE_IDX_1                  (0u)
#define FLASH_AREA_IMAGE_IDX_2                  (1u)

/* This defines if External Flash (SMIF) will be used for Upgrade Slots */
/* #define CY_BOOT_USE_EXTERNAL_FLASH */

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
#endif

#ifndef CY_BOOT_BOOTLOADER_SIZE
#define CY_BOOT_BOOTLOADER_SIZE             (0x18000)
#endif

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
#endif

#ifndef CY_BOOT_SECONDARY_1_SIZE
#define CY_BOOT_SECONDARY_1_SIZE            CY_BOOT_IMAGE_1_SIZE
#endif

#if (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
#ifndef CY_BOOT_PRIMARY_2_SIZE
#define CY_BOOT_PRIMARY_2_SIZE              CY_BOOT_IMAGE_2_SIZE
#endif

#ifndef CY_BOOT_SECONDARY_2_SIZE
#define CY_BOOT_SECONDARY_2_SIZE            CY_BOOT_IMAGE_2_SIZE
#endif
#endif

#ifndef CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET
#define CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET  (0x0u)
#endif

#ifndef CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET
#define CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET  (0x240000u)
#endif

#ifndef CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET
#define CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET      (0x440000u)
#endif

#ifndef CY_BOOT_SECONDARY_1_EXT_MEM_OFFSET
#define CY_BOOT_SECONDARY_1_EXT_MEM_OFFSET  (CY_SMIF_BASE_MEM_OFFSET + CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET)
#endif

#ifndef CY_BOOT_SECONDARY_2_EXT_MEM_OFFSET
#define CY_BOOT_SECONDARY_2_EXT_MEM_OFFSET  (CY_SMIF_BASE_MEM_OFFSET + CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET)
#endif

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

    if (0U == areaID)
    {
        result = FLASH_AREA_IMAGE_0;
    }
    else
    if (1U == areaID)
    {
        result = FLASH_AREA_IMAGE_2;
    }
    else
    {
        result = 0xFF;
    }

    return result;
}

__STATIC_INLINE uint8_t FLASH_AREA_IMAGE_SECONDARY(uint32_t areaID)
{
    uint8_t result;

    if (0U == areaID)
    {
        result = FLASH_AREA_IMAGE_1;
    }
    else
    if (1U == areaID)
    {
        result = FLASH_AREA_IMAGE_3;
    }
    else
    {
        result = 0xFF;
    }

    return result;
}
#endif
#endif

// #else
// #warning "Image slot and flash area mapping is not defined"
// #endif

#define CY_IMG_HDR_SIZE 0x400

#ifndef CY_FLASH_MAP_EXT_DESC
/* Uncomment in case you want to use separately defined table of flash area descriptors */
/* #define CY_FLASH_MAP_EXT_DESC */
#endif

#endif /* SYSFLASH_H */
