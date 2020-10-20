/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/flash.h>

#include "target.h"

#include <flash_map_backend/flash_map_backend.h>
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

#if (!defined(CONFIG_XTENSA) && defined(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL))
#define FLASH_DEVICE_ID SOC_FLASH_0_ID
#define FLASH_DEVICE_BASE CONFIG_FLASH_BASE_ADDRESS
#elif (defined(CONFIG_XTENSA) && defined(DT_JEDEC_SPI_NOR_0_LABEL))
#define FLASH_DEVICE_ID SPI_FLASH_0_ID
#define FLASH_DEVICE_BASE 0
#else
#error "FLASH_DEVICE_ID could not be determined"
#endif

static const struct device *flash_dev;

const struct device *flash_device_get_binding(char *dev_name)
{
    if (!flash_dev) {
        flash_dev = device_get_binding(dev_name);
    }
    return flash_dev;
}

int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    if (fd_id != FLASH_DEVICE_ID) {
        BOOT_LOG_ERR("invalid flash ID %d; expected %d",
                     fd_id, FLASH_DEVICE_ID);
        return -EINVAL;
    }
    *ret = FLASH_DEVICE_BASE;
    return 0;
}

/*
 * This depends on the mappings defined in sysflash.h.
 * MCUBoot uses continuous numbering for the primary slot, the secondary slot,
 * and the scratch while zephyr might number it differently.
 */
int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    switch (slot) {
    case 0: return FLASH_AREA_IMAGE_PRIMARY(image_index);
#if !defined(CONFIG_SINGLE_APPLICATION_SLOT)
    case 1: return FLASH_AREA_IMAGE_SECONDARY(image_index);
#if !defined(CONFIG_BOOT_SWAP_USING_MOVE)
    case 2: return FLASH_AREA_IMAGE_SCRATCH;
#endif
#endif
    }

    return -EINVAL; /* flash_area_open will fail on that */
}

int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if (area_id == FLASH_AREA_IMAGE_PRIMARY(image_index)) {
        return 0;
    }
#if !defined(CONFIG_SINGLE_APPLICATION_SLOT)
    if (area_id == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
        return 1;
    }
#endif

    BOOT_LOG_ERR("invalid flash area ID");
    return -1;
}

int flash_area_id_to_image_slot(int area_id)
{
    return flash_area_id_to_multi_image_slot(0, area_id);
}

int flash_area_sector_from_off(off_t off, struct flash_sector *sector)
{
    int rc;
    struct flash_pages_info page;

    rc = flash_get_page_info_by_offs(flash_dev, off, &page);
    if (rc) {
        return rc;
    }

    sector->fs_off = page.start_offset;
    sector->fs_size = page.size;

    return rc;
}

#define ERASED_VAL 0xff
uint8_t flash_area_erased_val(const struct flash_area *fap)
{
    (void)fap;
    return ERASED_VAL;
}
