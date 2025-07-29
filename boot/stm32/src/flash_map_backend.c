#include "flash_map_backend.h"
#include "stm32wlxx_hal.h"
#include <string.h>
#include <stdio.h>
#include "bootutil_log.h"

extern UART_HandleTypeDef hlpuart1;

#define SLOT0_BASE     0x08010000
#define SLOT1_BASE     0x08020000
#define SCRATCH_BASE   0x08030000


#define SLOT_SIZE      0x10000  // 128 Ko
#define SCRATCH_SIZE   0x10000  // 64 Ko
#define SECTOR_SIZE    0x4000   // 32 Ko
#define SECTOR_COUNT   (SLOT_SIZE / SECTOR_SIZE)

static struct flash_area slot0 = {
    .fa_id = 0,
    .fa_device_id = 0,
    .fa_off = SLOT0_BASE,
    .fa_size = SLOT_SIZE,
};

static struct flash_area slot1 = {
    .fa_id = 1,
    .fa_device_id = 0,
    .fa_off = SLOT1_BASE,
    .fa_size = SLOT_SIZE,
};

static struct flash_area slot2 = {
    .fa_id = 2,
    .fa_device_id = 0,
    .fa_off = SCRATCH_BASE,
    .fa_size = SCRATCH_SIZE,
};

int flash_area_open(uint8_t id, const struct flash_area **fa) {
    switch (id) {
        case 0: *fa = &slot0; return 0;
        case 1: *fa = &slot1; return 0;
        case 2: *fa = &slot2; return 0;
        default:
            char msg[64];
            snprintf(msg, sizeof(msg), "Unknown slot id (%d)\r\n", id);
            HAL_UART_Transmit(&hlpuart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
            return -1;
    }
}

void flash_area_close(const struct flash_area *fa) {
    (void)fa;
}

int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst, uint32_t len) {
    if ((off + len) > fa->fa_size) {
        BOOT_LOG_ERR("Read out of bounds!, offset=%lu, len=%lu,  fa_size=%lu\r\n", off, len, fa->fa_size);
        return -1;
    }
    memcpy(dst, (const void *)(fa->fa_off + off), len);
    return 0;
}

int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src, uint32_t len) {
    HAL_FLASH_Unlock();
    const uint8_t *src_bytes = (const uint8_t *)src;
    uint32_t addr = fa->fa_off + off;
    for (uint32_t i = 0; i < len; i += 8) {
        uint64_t data = 0xFFFFFFFFFFFFFFFF;
        memcpy(&data, &src_bytes[i], (len - i >= 8) ? 8 : (len - i));

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, data) != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}


int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len) {
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page = (fa->fa_off + off - FLASH_BASE) / FLASH_PAGE_SIZE,
        .NbPages = (len + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE,
    };
    uint32_t page_error;
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }
    HAL_FLASH_Lock();
    return 0;
}

int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors) {
    if (fa_id == 0 || fa_id == 1) {
        *count = 4;
        for (int i = 0; i < *count; i++) {
            sectors[i].fs_off = i * SECTOR_SIZE;
            sectors[i].fs_size = SECTOR_SIZE;
        }
        return 0;
    }
    return -1;
}

int flash_area_align(const struct flash_area *fa) {
    (void)fa;
    return 8;
}

int flash_area_erased_val(const struct flash_area *fa) {
    (void)fa;
    return 0xFF;
}

int flash_area_id_from_multi_image_slot(int image_index, int slot) {
    (void)image_index;
    return slot;
}

int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector)
{

    sector->fs_off = (off / SECTOR_SIZE) * SECTOR_SIZE;
    sector->fs_size = SECTOR_SIZE;

    return 0;
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    switch (area_id) {
        case 0:
            return 0;
        case 1:
            return 1;
        default:
            return -1;
    }
}

