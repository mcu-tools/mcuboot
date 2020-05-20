/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#if USE_PARTITION_MANAGER
#include <pm_config.h>
#include <mcuboot_config/mcuboot_config.h>

#ifndef CONFIG_SINGLE_IMAGE_DFU

#if (MCUBOOT_IMAGE_NUMBER == 1)

#define FLASH_AREA_IMAGE_PRIMARY(x)    PM_MCUBOOT_PRIMARY_ID
#define FLASH_AREA_IMAGE_SECONDARY(x)  PM_MCUBOOT_SECONDARY_ID
/* NOTE: Scratch parition is not used by single image DFU but some of
 * functions in common files reference it, so the definitions has been
 * provided to allow compilation of common units.
 */
#define FLASH_AREA_IMAGE_SCRATCH       0

#elif (MCUBOOT_IMAGE_NUMBER == 2)

extern uint32_t _image_1_primary_slot_id[];

#define FLASH_AREA_IMAGE_PRIMARY(x)            \
        ((x == 0) ?                            \
           PM_MCUBOOT_PRIMARY_ID :             \
         (x == 1) ?                            \
          (uint32_t)_image_1_primary_slot_id : \
           255 )

#define FLASH_AREA_IMAGE_SECONDARY(x) \
        ((x == 0) ?                   \
            PM_MCUBOOT_SECONDARY_ID:  \
        (x == 1) ?                    \
           PM_MCUBOOT_SECONDARY_ID:   \
           255 )
#endif
#define FLASH_AREA_IMAGE_SCRATCH    PM_MCUBOOT_SCRATCH_ID

#else /* CONFIG_SINGLE_IMAGE_DFU */

#define FLASH_AREA_IMAGE_PRIMARY(x)	PM_MCUBOOT_PRIMARY_ID
#define FLASH_AREA_IMAGE_SECONDARY(x)	PM_MCUBOOT_PRIMARY_ID

#endif /* CONFIG_SINGLE_IMAGE_DFU */

#else

#include <devicetree.h>
#include <mcuboot_config/mcuboot_config.h>

#ifndef CONFIG_SINGLE_IMAGE_DFU

#if (MCUBOOT_IMAGE_NUMBER == 1)
/*
 * NOTE: the definition below returns the same values for true/false on
 * purpose, to avoid having to mark x as non-used by all callers when
 * running in single image mode.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?                \
                                         FLASH_AREA_ID(image_0) : \
                                         FLASH_AREA_ID(image_0))
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?                \
                                         FLASH_AREA_ID(image_1) : \
                                         FLASH_AREA_ID(image_1))
#elif (MCUBOOT_IMAGE_NUMBER == 2)
/* MCUBoot currently supports only up to 2 updateable firmware images.
 * If the number of the current image is greater than MCUBOOT_IMAGE_NUMBER - 1
 * then a dummy value will be assigned to the flash area macros.
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?                \
                                         FLASH_AREA_ID(image_0) : \
                                        ((x) == 1) ?                \
                                         FLASH_AREA_ID(image_2) : \
                                         255)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?                \
                                         FLASH_AREA_ID(image_1) : \
                                        ((x) == 1) ?                \
                                         FLASH_AREA_ID(image_3) : \
                                         255)
#else
#error "Image slot and flash area mapping is not defined"
#endif

#if !defined(CONFIG_BOOT_SWAP_USING_MOVE)
#define FLASH_AREA_IMAGE_SCRATCH    FLASH_AREA_ID(image_scratch)
#endif

#else /* CONFIG_SINGLE_IMAGE_DFU */

#define FLASH_AREA_IMAGE_PRIMARY(x)	FLASH_AREA_ID(image_0)
#define FLASH_AREA_IMAGE_SECONDARY(x)	FLASH_AREA_ID(image_0)
/* NOTE: Scratch parition is not used by single image DFU but some of
 * functions in common files reference it, so the definitions has been
 * provided to allow compilation of common units.
 */
#define FLASH_AREA_IMAGE_SCRATCH	0

#endif /* CONFIG_SINGLE_IMAGE_DFU */

#endif /* USE_PARTITION_MANAGER */

#endif /* __SYSFLASH_H__ */
