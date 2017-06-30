/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <zephyr.h>
#include <flash.h>

#include "target.h"

#include <flash_map/flash_map.h>
#include <hal/hal_flash.h>
#include <sysflash/sysflash.h>

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_INFO
#include "bootutil/bootutil_log.h"

extern struct device *boot_flash_device;

/*
 * For now, we only support one flash device.
 *
 * Pick a random device ID for it that's unlikely to collide with
 * anything "real".
 */
#define FLASH_DEVICE_ID 100
#define FLASH_DEVICE_BASE CONFIG_FLASH_BASE_ADDRESS

#define FLASH_MAP_ENTRY_MAGIC 0xd00dbeef

struct flash_map_entry {
    const uint32_t magic;
    const struct flash_area area;
    unsigned int ref_count;
};

/*
 * The flash area describes essentially the partition table of the
 * flash.  In this case, it starts with FLASH_AREA_IMAGE_0.
 */
static struct flash_map_entry part_map[] = {
    {
        .magic = FLASH_MAP_ENTRY_MAGIC,
        .area = {
            .fa_id = FLASH_AREA_IMAGE_0,
            .fa_device_id = FLASH_DEVICE_ID,
            .fa_off = FLASH_AREA_IMAGE_0_OFFSET,
            .fa_size = FLASH_AREA_IMAGE_0_SIZE,
        },
    },
    {
        .magic = FLASH_MAP_ENTRY_MAGIC,
        .area = {
            .fa_id = FLASH_AREA_IMAGE_1,
            .fa_device_id = FLASH_DEVICE_ID,
            .fa_off = FLASH_AREA_IMAGE_1_OFFSET,
            .fa_size = FLASH_AREA_IMAGE_1_SIZE,
        },
    },
    {
        .magic = FLASH_MAP_ENTRY_MAGIC,
        .area = {
            .fa_id = FLASH_AREA_IMAGE_SCRATCH,
            .fa_device_id = FLASH_DEVICE_ID,
            .fa_off = FLASH_AREA_IMAGE_SCRATCH_OFFSET,
            .fa_size = FLASH_AREA_IMAGE_SCRATCH_SIZE,
        },
    }
};

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
 * `open` a flash area.  The `area` in this case is not the individual
 * sectors, but describes the particular flash area in question.
 */
int flash_area_open(uint8_t id, const struct flash_area **area)
{
    int i;

    BOOT_LOG_DBG("area %d", id);

    for (i = 0; i < ARRAY_SIZE(part_map); i++) {
        if (id == part_map[i].area.fa_id) {
            break;
        }
    }
    if (i == ARRAY_SIZE(part_map)) {
        return -1;
    }

    *area = &part_map[i].area;
    part_map[i].ref_count++;
    return 0;
}

/*
 * Nothing to do on close.
 */
void flash_area_close(const struct flash_area *area)
{
    struct flash_map_entry *entry;

    if (!area) {
        return;
    }

    entry = CONTAINER_OF(area, struct flash_map_entry, area);
    if (entry->magic != FLASH_MAP_ENTRY_MAGIC) {
        BOOT_LOG_ERR("invalid area %p (id %u)", area, area->fa_id);
        return;
    }
    if (entry->ref_count == 0) {
        BOOT_LOG_ERR("area %u use count underflow", area->fa_id);
        return;
    }
    entry->ref_count--;
}

void zephyr_flash_area_warn_on_open(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(part_map); i++) {
        struct flash_map_entry *entry = &part_map[i];
        if (entry->ref_count) {
            BOOT_LOG_WRN("area %u has %u users",
                         entry->area.fa_id, entry->ref_count);
        }
    }
}

int flash_area_read(const struct flash_area *area, uint32_t off, void *dst,
            uint32_t len)
{
    BOOT_LOG_DBG("area=%d, off=%x, len=%x", area->fa_id, off, len);
    return flash_read(boot_flash_device, area->fa_off + off, dst, len);
}

int flash_area_write(const struct flash_area *area, uint32_t off, const void *src,
             uint32_t len)
{
    int rc = 0;

    BOOT_LOG_DBG("area=%d, off=%x, len=%x", area->fa_id, off, len);
    flash_write_protection_set(boot_flash_device, false);
    rc = flash_write(boot_flash_device, area->fa_off + off, src, len);
    flash_write_protection_set(boot_flash_device, true);
    return rc;
}

int flash_area_erase(const struct flash_area *area, uint32_t off, uint32_t len)
{
    int rc;

    BOOT_LOG_DBG("area=%d, off=%x, len=%x", area->fa_id, off, len);
    flash_write_protection_set(boot_flash_device, false);
    rc = flash_erase(boot_flash_device, area->fa_off + off, len);
    flash_write_protection_set(boot_flash_device, true);
    return rc;
}

uint8_t flash_area_align(const struct flash_area *area)
{
    return hal_flash_align(area->fa_id);
}

/*
 * This depends on the mappings defined in sysflash.h, and assumes
 * that slot 0, slot 1, and the scratch area area contiguous.
 */
int flash_area_id_from_image_slot(int slot)
{
    return slot + FLASH_AREA_IMAGE_0;
}

#ifndef FLASH_AREA_IMAGE_SECTOR_SIZE
#warning "Missing FLASH_AREA_IMAGE_SECTOR_SIZE; assuming scratch size instead"
#define FLASH_AREA_IMAGE_SECTOR_SIZE FLASH_AREA_IMAGE_SCRATCH_SIZE
#endif

static int validate_idx(int idx, uint32_t *off, uint32_t *len)
{
    /*
     * This simple layout has uniform slots, so just fill in the
     * right one.
     */
    if (idx < FLASH_AREA_IMAGE_0 || idx > FLASH_AREA_IMAGE_SCRATCH) {
        return -1;
    }

    switch (idx) {
    case FLASH_AREA_IMAGE_0:
        *off = FLASH_AREA_IMAGE_0_OFFSET;
        *len = FLASH_AREA_IMAGE_0_SIZE;
        break;
    case FLASH_AREA_IMAGE_1:
        *off = FLASH_AREA_IMAGE_1_OFFSET;
        *len = FLASH_AREA_IMAGE_1_SIZE;
        break;
    case FLASH_AREA_IMAGE_SCRATCH:
        *off = FLASH_AREA_IMAGE_SCRATCH_OFFSET;
        *len = FLASH_AREA_IMAGE_SCRATCH_SIZE;
        break;
    default:
        BOOT_LOG_ERR("unknown flash area %d", idx);
        return -1;
    }

    BOOT_LOG_DBG("area %d: offset=0x%x, length=0x%x, sector size=0x%x",
                 idx, *off, *len, FLASH_AREA_IMAGE_SECTOR_SIZE);
    return 0;
}

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret)
{
    uint32_t off;
    uint32_t len;
    uint32_t max_cnt = *cnt;
    uint32_t rem_len;

    if (validate_idx(idx, &off, &len)) {
        return -1;
    }

    if (*cnt < 1) {
        return -1;
    }

    rem_len = len;
    *cnt = 0;
    while (rem_len > 0 && *cnt < max_cnt) {
        if (rem_len < FLASH_AREA_IMAGE_SECTOR_SIZE) {
            BOOT_LOG_ERR("area %d size 0x%x not divisible by sector size 0x%x",
                     idx, len, FLASH_AREA_IMAGE_SECTOR_SIZE);
            return -1;
        }

        ret[*cnt].fa_id = idx;
        ret[*cnt].fa_device_id = 0;
        ret[*cnt].pad16 = 0;
        ret[*cnt].fa_off = off + (FLASH_AREA_IMAGE_SECTOR_SIZE * (*cnt));
        ret[*cnt].fa_size = FLASH_AREA_IMAGE_SECTOR_SIZE;
        *cnt = *cnt + 1;
        rem_len -= FLASH_AREA_IMAGE_SECTOR_SIZE;
    }

    if (*cnt >= max_cnt) {
        BOOT_LOG_ERR("flash area %d sector count overflow", idx);
        return -1;
    }

    return 0;
}

/*
 * Lookup the sector map for a given flash area.  This should fill in
 * `ret` with all of the sectors in the area.  `*cnt` will be set to
 * the storage at `ret` and should be set to the final number of
 * sectors in this area.
 */
int flash_area_get_sectors(int idx, uint32_t *cnt, struct flash_sector *ret)
{
    uint32_t off;
    uint32_t len;
    uint32_t max_cnt = *cnt;
    uint32_t rem_len;

    if (validate_idx(idx, &off, &len)) {
        return -1;
    }

    if (*cnt < 1) {
        return -1;
    }

    rem_len = len;
    *cnt = 0;
    while (rem_len > 0 && *cnt < max_cnt) {
        if (rem_len < FLASH_AREA_IMAGE_SECTOR_SIZE) {
            BOOT_LOG_ERR("area %d size 0x%x not divisible by sector size 0x%x",
                         idx, len, FLASH_AREA_IMAGE_SECTOR_SIZE);
            return -1;
        }

        ret[*cnt].fs_off = FLASH_AREA_IMAGE_SECTOR_SIZE * (*cnt);
        ret[*cnt].fs_size = FLASH_AREA_IMAGE_SECTOR_SIZE;
        *cnt = *cnt + 1;
        rem_len -= FLASH_AREA_IMAGE_SECTOR_SIZE;
    }

    if (*cnt >= max_cnt) {
        BOOT_LOG_ERR("flash area %d sector count overflow", idx);
        return -1;
    }

    return 0;
}
