/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>

#include "target.h"

#include <flash_map_backend/flash_map_backend.h>
#include <zephyr/devicetree/partitions.h>
#include <sysflash/sysflash.h>

#include "bootutil/boot_hooks.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil_public.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#define FLASH_PARTITION_ADDRESS(partition) (PARTITION_ADDRESS(partition) - PARTITION_OFFSET(partition))

#define FLASH_PARTITION_DEV_UNIQUE(n, m) (PARTITION_EXISTS(n) && \
                                          !(DT_SAME_NODE(PARTITION_MTD(n), PARTITION_MTD(m))))

int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    switch (fd_id)
    {
    case 0U:
        *ret = FLASH_PARTITION_ADDRESS(slot0_partition);
        break;
#if FLASH_PARTITION_DEV_UNIQUE(slot1_partition, slot0_partition)
    case 1U:
        *ret = FLASH_PARTITION_ADDRESS(slot1_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot2_partition, slot1_partition)
    case 2U:
        *ret = FLASH_PARTITION_ADDRESS(slot2_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot3_partition, slot2_partition)
    case 3U:
        *ret = FLASH_PARTITION_ADDRESS(slot3_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot4_partition, slot3_partition)
    case 4U:
        *ret = FLASH_PARTITION_ADDRESS(slot4_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot5_partition, slot4_partition)
    case 5U:
        *ret = FLASH_PARTITION_ADDRESS(slot5_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot6_partition, slot5_partition)
    case 6U:
        *ret = FLASH_PARTITION_ADDRESS(slot6_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot7_partition, slot6_partition)
    case 7U:
        *ret = FLASH_PARTITION_ADDRESS(slot7_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot8_partition, slot7_partition)
    case 8U:
        *ret = FLASH_PARTITION_ADDRESS(slot8_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot9_partition, slot8_partition)
    case 9U:
        *ret = FLASH_PARTITION_ADDRESS(slot9_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot10_partition, slot9_partition)
    case 10U:
        *ret = FLASH_PARTITION_ADDRESS(slot10_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot11_partition, slot10_partition)
    case 11U:
        *ret = FLASH_PARTITION_ADDRESS(slot11_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot12_partition, slot11_partition)
    case 12U:
        *ret = FLASH_PARTITION_ADDRESS(slot12_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot13_partition, slot12_partition)
    case 13U:
        *ret = FLASH_PARTITION_ADDRESS(slot13_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot14_partition, slot13_partition)
    case 14U:
        *ret = FLASH_PARTITION_ADDRESS(slot14_partition);
        break;
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot15_partition, slot14_partition)
    case 15U:
        *ret = FLASH_PARTITION_ADDRESS(slot15_partition);
        break;
#endif
    default:
        BOOT_LOG_ERR("invalid flash ID %d", fd_id);
        return -EINVAL;
    }
    return 0;
}

/*
 * This depends on the mappings defined in sysflash.h.
 * MCUBoot uses continuous numbering for the primary slot, the secondary slot,
 * and the scratch while zephyr might number it differently.
 */
int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    int rc;
    int id = -1;

    rc = BOOT_HOOK_FLASH_AREA_CALL(flash_area_id_from_multi_image_slot_hook,
                                   BOOT_HOOK_REGULAR, image_index, slot, &id);
    if (rc != BOOT_HOOK_REGULAR) {
        return id;
    }

    switch (slot) {
    case 0: return FLASH_AREA_IMAGE_PRIMARY(image_index);
#if !defined(CONFIG_SINGLE_APPLICATION_SLOT)
    case 1: return FLASH_AREA_IMAGE_SECONDARY(image_index);
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

#if defined(CONFIG_MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD)
int flash_area_id_from_direct_image(int image_id)
{
    switch (image_id) {
    case 0:
    case 1:
        return PARTITION_ID(slot0_partition);
#if PARTITION_EXISTS(slot1_partition)
    case 2:
        return PARTITION_ID(slot1_partition);
#endif
#if PARTITION_EXISTS(slot2_partition)
    case 3:
        return PARTITION_ID(slot2_partition);
#endif
#if PARTITION_EXISTS(slot3_partition)
    case 4:
        return PARTITION_ID(slot3_partition);
#endif
#if PARTITION_EXISTS(slot4_partition)
    case 5:
        return PARTITION_ID(slot4_partition);
#endif
#if PARTITION_EXISTS(slot5_partition)
    case 6:
        return PARTITION_ID(slot5_partition);
#endif
    }
    return -EINVAL;
}
#endif

uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
    const struct device *dev = flash_area_get_device(fa);

    if (dev == PARTITION_DEVICE(slot0_partition)) {
        return 0U;
    }
#if FLASH_PARTITION_DEV_UNIQUE(slot1_partition, slot0_partition)
    if (dev == PARTITION_DEVICE(slot1_partition)) {
        return 1U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot2_partition, slot1_partition)
    if (dev == PARTITION_DEVICE(slot2_partition)) {
        return 2U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot3_partition, slot2_partition)
    if (dev == PARTITION_DEVICE(slot3_partition)) {
        return 3U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot4_partition, slot3_partition)
    if (dev == PARTITION_DEVICE(slot4_partition)) {
        return 4U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot5_partition, slot4_partition)
    if (dev == PARTITION_DEVICE(slot5_partition)) {
        return 5U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot6_partition, slot5_partition)
    if (dev == PARTITION_DEVICE(slot6_partition)) {
        return 6U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot7_partition, slot6_partition)
    if (dev == PARTITION_DEVICE(slot7_partition)) {
        return 7U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot8_partition, slot7_partition)
    if (dev == PARTITION_DEVICE(slot8_partition)) {
        return 8U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot9_partition, slot8_partition)
    if (dev == PARTITION_DEVICE(slot9_partition)) {
        return 9U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot10_partition, slot9_partition)
    if (dev == PARTITION_DEVICE(slot10_partition)) {
        return 10U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot11_partition, slot10_partition)
    if (dev == PARTITION_DEVICE(slot11_partition)) {
        return 11U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot12_partition, slot11_partition)
    if (dev == PARTITION_DEVICE(slot12_partition)) {
        return 12U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot13_partition, slot12_partition)
    if (dev == PARTITION_DEVICE(slot13_partition)) {
        return 13U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot14_partition, slot13_partition)
    if (dev == PARTITION_DEVICE(slot14_partition)) {
        return 14U;
    }
#endif
#if FLASH_PARTITION_DEV_UNIQUE(slot15_partition, slot14_partition)
    if (dev == PARTITION_DEVICE(slot15_partition)) {
        return 15U;
    }
#endif

    __ASSERT(0, "invalid flash area device");

    return 0U;
}

#define ERASED_VAL 0xff
__weak uint8_t flash_area_erased_val(const struct flash_area *fap)
{
    (void)fap;
    return ERASED_VAL;
}

int flash_area_get_sector(const struct flash_area *fap, off_t off,
                          struct flash_sector *fsp)
{
    struct flash_pages_info fpi;
    int rc;

    if (off < 0 || (size_t) off >= fap->fa_size) {
        return -ERANGE;
    }

    rc = flash_get_page_info_by_offs(fap->fa_dev, fap->fa_off + off,
            &fpi);

    if (rc == 0) {
        fsp->fs_off = fpi.start_offset - fap->fa_off;
        fsp->fs_size = fpi.size;
    }

    return rc;
}
