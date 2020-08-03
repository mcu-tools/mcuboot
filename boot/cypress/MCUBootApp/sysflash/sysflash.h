/* Manual version of auto-generated version. */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#define FLASH_DEVICE_INTERNAL_FLASH        (0x7F)

#define FLASH_AREA_BOOTLOADER 0
#define FLASH_AREA_IMAGE_0 1
#define FLASH_AREA_IMAGE_1 2
#define FLASH_AREA_IMAGE_SCRATCH 3
#define FLASH_AREA_IMAGE_2 5
#define FLASH_AREA_IMAGE_3 6
#define FLASH_AREA_IMAGE_SWAP_STATUS 7

/* Uncomment if external flash is being used */
/* #define CY_BOOT_USE_EXTERNAL_FLASH */

/* use PDL-defined offset or one from SMFI config */
#define CY_SMIF_BASE_MEM_OFFSET             (0x18000000)

#define CY_FLASH_ALIGN                      (CY_FLASH_SIZEOF_ROW)
#define CY_FLASH_DEVICE_BASE                (CY_FLASH_BASE)

#ifndef CY_BOOT_SCRATCH_SIZE
#define CY_BOOT_SCRATCH_SIZE                (0x1000)
#endif

 /* TBD - needs to be calculated 
    Calculation considerations:

    sec0, state_x_0     \
    sec1, state_x_1      | this block consists of img sector number and its state (0..3)
    ...                  | crc and cnt fields are 4 byte each. Min size of this block is 4 bytes.
    secX, state_x_X      | In case of PSoC6: min_write_size (512 bytes) - crc (4 bytes) - cnt (4 bytes) =
    cnt                  | = 504 bytes. if secX = 2 byte and status_x_X = 2 byte, then 504/4 = 126 sectors info can stored.
    crc                 /
    ...
    secX+1, state_x_X+1 \
    secX+2, state_x_X+2  | MCUBootApp currently use MCUBOOT_MAX_IMG_SECTORS = 2560, one block can contain info about 126 sectors, so 21 blocks
    ...                  | needed to store info about all sectors.
    secX+n, state_x_X    |
    cnt                  |
    crc                 /

    enc_key_1           \ optionally there are encryption keys
    enc_key_2           / 16 bytes each

    swap_info          \
    copy_done           |
    cnt                 | this is status partition trailer area.
    crc                 | This area is considered to be 32 bytes size.
                        |
    image_ok            | For PSoC6 trailer size would be 512 bytes (min_wr_size)
    magic               |
    cnt                 |
    crc                /

    Finally ( (sec_info_block_size (512b) * sec_info_block_num (21) + 
            swap_status_trailer (512b) ) * BOOT_SWAP_STATUS_MULT ) * MCUBOOT_IMAGE_NUMBER = 22528 bytes (0x5800)
 */
#ifndef CY_BOOT_SWAP_STATUS_SIZE
#define CY_BOOT_SWAP_STATUS_SIZE            (0x5800)
#endif

#ifndef CY_BOOT_BOOTLOADER_SIZE
#define CY_BOOT_BOOTLOADER_SIZE             (0x18000)
#endif

#ifndef CY_BOOT_PRIMARY_1_SIZE
#define CY_BOOT_PRIMARY_1_SIZE              (0x10000)
#endif

#ifndef CY_BOOT_SECONDARY_1_SIZE
#define CY_BOOT_SECONDARY_1_SIZE            (0x10000)
#endif

#if (MCUBOOT_IMAGE_NUMBER == 2) /* if dual-image */
#ifndef CY_BOOT_PRIMARY_2_SIZE
#define CY_BOOT_PRIMARY_2_SIZE              (0x10000)
#endif

#ifndef CY_BOOT_SECONDARY_2_SIZE
#define CY_BOOT_SECONDARY_2_SIZE            (0x10000)
#endif
#endif

#if (MCUBOOT_IMAGE_NUMBER == 1)
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0 : \
                                         FLASH_AREA_IMAGE_0)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_1 : \
                                         FLASH_AREA_IMAGE_1)

#elif (MCUBOOT_IMAGE_NUMBER == 2)

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
#warning "Image slot and flash area mapping is not defined"
#endif
#define CY_IMG_HDR_SIZE 0x400

#ifndef CY_FLASH_MAP_EXT_DESC
/* Uncomment in case you want to use separately defined table of flash area descriptors */
/* #define CY_FLASH_MAP_EXT_DESC */
#endif

#endif /* __SYSFLASH_H__ */
