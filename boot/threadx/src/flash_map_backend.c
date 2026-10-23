/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"
#include "mcuboot_config/mcuboot_config.h"
#include "mcuboot_config/mcuboot_logging.h"

/*
 * Reference Board Flash Geometry (STM32F429ZI / Cortex-M4):
 * Total Flash: 2048 KB (2 MB)
 *
 * Partition Layout:
 * - Bootloader:     128 KB [0x00000000 - 0x0001FFFF]
 * - Primary Slot:   384 KB [0x00020000 - 0x0007FFFF] (3 sectors x 128 KB)
 * - Secondary Slot: 384 KB [0x00080000 - 0x000DFFFF] (3 sectors x 128 KB)
 * - Scratch Slot:   128 KB [0x000E0000 - 0x000FFFFF] (1 sector  x 128 KB)
 * - Available Bank: 1024 KB [0x00100000 - 0x001FFFFF] (FileX / user data)
 */

#define THREADX_FLASH_TOTAL_SIZE      (2 * 1024 * 1024)
#define THREADX_BOOTLOADER_OFFSET     0x00000000
#define THREADX_BOOTLOADER_SIZE       (128 * 1024)

#define THREADX_PRIMARY_OFFSET        0x00020000
#define THREADX_PRIMARY_SIZE          (384 * 1024)

#define THREADX_SECONDARY_OFFSET      0x00080000
#define THREADX_SECONDARY_SIZE        (384 * 1024)

#define THREADX_SCRATCH_OFFSET        0x000E0000
#define THREADX_SCRATCH_SIZE          (128 * 1024)

#define THREADX_SLOT_SECTOR_SIZE      (128 * 1024)
#define THREADX_SLOT_SECTOR_COUNT     (THREADX_PRIMARY_SIZE / THREADX_SLOT_SECTOR_SIZE)

#define THREADX_FLASH_ALIGN           8
#define THREADX_FLASH_ERASED_VAL      0xFF

static const struct flash_area s_flash_areas[] = {
    {
        .fa_id = FLASH_AREA_BOOTLOADER,
        .fa_device_id = 0,
        .pad16 = 0,
        .fa_off = THREADX_BOOTLOADER_OFFSET,
        .fa_size = THREADX_BOOTLOADER_SIZE,
        .fa_driver_data = NULL
    },
    {
        .fa_id = FLASH_AREA_PRIMARY_ID,
        .fa_device_id = 0,
        .pad16 = 0,
        .fa_off = THREADX_PRIMARY_OFFSET,
        .fa_size = THREADX_PRIMARY_SIZE,
        .fa_driver_data = NULL
    },
    {
        .fa_id = FLASH_AREA_SECONDARY_ID,
        .fa_device_id = 0,
        .pad16 = 0,
        .fa_off = THREADX_SECONDARY_OFFSET,
        .fa_size = THREADX_SECONDARY_SIZE,
        .fa_driver_data = NULL
    },
    {
        .fa_id = FLASH_AREA_SCRATCH_ID,
        .fa_device_id = 0,
        .pad16 = 0,
        .fa_off = THREADX_SCRATCH_OFFSET,
        .fa_size = THREADX_SCRATCH_SIZE,
        .fa_driver_data = NULL
    }
};

#define FLASH_AREA_COUNT (sizeof(s_flash_areas) / sizeof(s_flash_areas[0]))

/* Flash memory buffer */
static uint8_t s_flash_memory[THREADX_FLASH_TOTAL_SIZE];

/* Fault injection simulation state */
static bool s_interruption_enabled = false;
static uint32_t s_ops_until_cut = 0;
static jmp_buf *s_reboot_env = NULL;

/*
 * TinyCrypt CSPRNG stub for signature verification.
 * Verification is deterministic and does not require active randomness.
 */
int default_CSPRNG(uint8_t *dest, unsigned int size)
{
    (void)dest;
    (void)size;
    return 1;
}

int flash_map_backend_init(void)
{
    static bool initialized = false;
    if (!initialized) {
        memset(s_flash_memory, THREADX_FLASH_ERASED_VAL, sizeof(s_flash_memory));
        s_interruption_enabled = false;
        s_ops_until_cut = 0;
        s_reboot_env = NULL;
        initialized = true;
    }
    return 0;
}

void flash_map_backend_reset(void)
{
    memset(s_flash_memory, THREADX_FLASH_ERASED_VAL, sizeof(s_flash_memory));
    s_interruption_enabled = false;
    s_ops_until_cut = 0;
    s_reboot_env = NULL;
}

void flash_map_set_interruption(bool enable, uint32_t ops_until_cut, jmp_buf *env)
{
    s_interruption_enabled = enable;
    s_ops_until_cut = ops_until_cut;
    s_reboot_env = env;
}

uint8_t *flash_map_get_memory_buffer(uint32_t offset)
{
    if (offset < THREADX_FLASH_TOTAL_SIZE) {
        return &s_flash_memory[offset];
    }
    return NULL;
}

int flash_area_open(uint8_t id, const struct flash_area **fa)
{
    if (fa == NULL) {
        return -EINVAL;
    }

    for (size_t i = 0; i < FLASH_AREA_COUNT; i++) {
        if (s_flash_areas[i].fa_id == id) {
            *fa = &s_flash_areas[i];
            return 0;
        }
    }

    *fa = NULL;
    return -ENODEV;
}

void flash_area_close(const struct flash_area *fa)
{
    (void)fa;
}

int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst, uint32_t len)
{
    if (fa == NULL || dst == NULL) {
        return -EINVAL;
    }

    if (off + len > fa->fa_size) {
        MCUBOOT_LOG_ERR("flash_area_read: out of bounds (off=0x%x len=0x%x size=0x%x)",
                        (unsigned int)off, (unsigned int)len, (unsigned int)fa->fa_size);
        return -EINVAL;
    }

    uint32_t abs_off = fa->fa_off + off;
    if (abs_off + len > THREADX_FLASH_TOTAL_SIZE) {
        return -EINVAL;
    }

    memcpy(dst, &s_flash_memory[abs_off], len);
    return 0;
}

int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src, uint32_t len)
{
    if (fa == NULL || src == NULL) {
        return -EINVAL;
    }

    if (off + len > fa->fa_size) {
        MCUBOOT_LOG_ERR("flash_area_write: out of bounds (off=0x%x len=0x%x size=0x%x)",
                        (unsigned int)off, (unsigned int)len, (unsigned int)fa->fa_size);
        return -EINVAL;
    }

    /* Enforce write alignment */
    if ((off % THREADX_FLASH_ALIGN) != 0 || (len % THREADX_FLASH_ALIGN) != 0) {
        MCUBOOT_LOG_ERR("flash_area_write: unaligned write (off=0x%x len=0x%x align=%u)",
                        (unsigned int)off, (unsigned int)len, THREADX_FLASH_ALIGN);
        return -EINVAL;
    }

    /* Interrupted swap / power-loss simulation */
    if (s_interruption_enabled && s_reboot_env != NULL) {
        if (s_ops_until_cut == 0) {
            MCUBOOT_LOG_WRN("flash_area_write: power cut triggered midway! Resetting MCU...");
            longjmp(*s_reboot_env, 1);
        }
        s_ops_until_cut--;
    }

    uint32_t abs_off = fa->fa_off + off;
    const uint8_t *src_bytes = (const uint8_t *)src;

    /* Simulate NOR flash bit clearing: 1 -> 0 allowed, 0 -> 1 requires erase */
    for (uint32_t i = 0; i < len; i++) {
        uint8_t current = s_flash_memory[abs_off + i];
        uint8_t write_val = src_bytes[i];
        s_flash_memory[abs_off + i] = current & write_val;
    }

    return 0;
}

int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    if (fa == NULL) {
        return -EINVAL;
    }

    if (off + len > fa->fa_size) {
        MCUBOOT_LOG_ERR("flash_area_erase: out of bounds (off=0x%x len=0x%x size=0x%x)",
                        (unsigned int)off, (unsigned int)len, (unsigned int)fa->fa_size);
        return -EINVAL;
    }

    if (s_interruption_enabled && s_reboot_env != NULL) {
        if (s_ops_until_cut == 0) {
            MCUBOOT_LOG_WRN("flash_area_erase: power cut triggered midway! Resetting MCU...");
            longjmp(*s_reboot_env, 1);
        }
        s_ops_until_cut--;
    }

    uint32_t abs_off = fa->fa_off + off;
    memset(&s_flash_memory[abs_off], THREADX_FLASH_ERASED_VAL, len);
    return 0;
}

uint32_t flash_area_align(const struct flash_area *fa)
{
    (void)fa;
    return THREADX_FLASH_ALIGN;
}

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
    (void)fa;
    return THREADX_FLASH_ERASED_VAL;
}

int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors)
{
    if (count == NULL) {
        return -EINVAL;
    }

    const struct flash_area *fa = NULL;
    int rc = flash_area_open((uint8_t)fa_id, &fa);
    if (rc != 0 || fa == NULL) {
        return -ENODEV;
    }

    uint32_t num_sectors = 0;
    if (fa_id == FLASH_AREA_PRIMARY_ID || fa_id == FLASH_AREA_SECONDARY_ID) {
        num_sectors = THREADX_SLOT_SECTOR_COUNT;
    } else if (fa_id == FLASH_AREA_SCRATCH_ID) {
        num_sectors = 1;
    } else if (fa_id == FLASH_AREA_BOOTLOADER) {
        num_sectors = 1;
    } else {
        return -EINVAL;
    }

    if (sectors != NULL) {
        if (*count < num_sectors) {
            return -ENOMEM;
        }
        for (uint32_t i = 0; i < num_sectors; i++) {
            sectors[i].fs_off = i * THREADX_SLOT_SECTOR_SIZE;
            sectors[i].fs_size = THREADX_SLOT_SECTOR_SIZE;
        }
    }

    *count = num_sectors;
    return 0;
}

int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector)
{
    if (fa == NULL || sector == NULL) {
        return -EINVAL;
    }

    if (off >= fa->fa_size) {
        return -EINVAL;
    }

    uint32_t sector_idx = off / THREADX_SLOT_SECTOR_SIZE;
    sector->fs_off = sector_idx * THREADX_SLOT_SECTOR_SIZE;
    sector->fs_size = THREADX_SLOT_SECTOR_SIZE;
    return 0;
}

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret)
{
    if (cnt == NULL) {
        return -EINVAL;
    }

    uint32_t count = (uint32_t)*cnt;
    struct flash_sector sectors[THREADX_SLOT_SECTOR_COUNT];
    int rc = flash_area_get_sectors(idx, &count, sectors);
    if (rc != 0) {
        return rc;
    }

    const struct flash_area *fa = NULL;
    rc = flash_area_open((uint8_t)idx, &fa);
    if (rc != 0 || fa == NULL) {
        return rc;
    }

    if (ret != NULL) {
        for (uint32_t i = 0; i < count; i++) {
            ret[i].fa_id = (uint8_t)idx;
            ret[i].fa_device_id = fa->fa_device_id;
            ret[i].pad16 = 0;
            ret[i].fa_off = fa->fa_off + sectors[i].fs_off;
            ret[i].fa_size = sectors[i].fs_size;
            ret[i].fa_driver_data = NULL;
        }
    }

    flash_area_close(fa);
    *cnt = (int)count;
    return 0;
}

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    if (image_index != 0) {
        return -EINVAL;
    }
    return flash_area_id_from_image_slot(slot);
}

int flash_area_id_from_image_slot(int slot)
{
    if (slot == 0) {
        return FLASH_AREA_PRIMARY_ID;
    } else if (slot == 1) {
        return FLASH_AREA_SECONDARY_ID;
    }
    return -EINVAL;
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if (image_index != 0) {
        return -EINVAL;
    }

    if (area_id == FLASH_AREA_PRIMARY_ID) {
        return 0;
    } else if (area_id == FLASH_AREA_SECONDARY_ID) {
        return 1;
    }

    return -EINVAL;
}

int flash_area_id_from_image_offset(uint32_t offset)
{
    if (offset >= THREADX_PRIMARY_OFFSET && offset < (THREADX_PRIMARY_OFFSET + THREADX_PRIMARY_SIZE)) {
        return FLASH_AREA_PRIMARY_ID;
    }
    if (offset >= THREADX_SECONDARY_OFFSET && offset < (THREADX_SECONDARY_OFFSET + THREADX_SECONDARY_SIZE)) {
        return FLASH_AREA_SECONDARY_ID;
    }
    if (offset >= THREADX_SCRATCH_OFFSET && offset < (THREADX_SCRATCH_OFFSET + THREADX_SCRATCH_SIZE)) {
        return FLASH_AREA_SCRATCH_ID;
    }
    return -EINVAL;
}
