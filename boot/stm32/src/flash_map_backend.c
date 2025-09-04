#include "flash_map_backend.h"
#include "stm32wlxx_hal.h"
#include <string.h>
#include <stdio.h>
#include "bootutil_log.h"

extern UART_HandleTypeDef huart1;

#define SLOT0_BASE     0x08010000
//#define SLOT0_BASE     0x0800C000
//#define SLOT1_BASE     0x08025000
#define SLOT1_BASE     0x08020000
#define SCRATCH_BASE   0x0803F000


#define SLOT_SIZE      0x19000
#define SCRATCH_SIZE   0x1000
#define SECTOR_SIZE    0x4000
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
            HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
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

//int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors) {
//
//	BOOT_LOG_INF("bg");
//	if (fa_id == 0 || fa_id == 1) {
//        *count =7;
//        for (int i = 0; i < *count; i++) {
//            sectors[i].fs_off = i * SECTOR_SIZE;
//            sectors[i].fs_size = SECTOR_SIZE;
//        }
//        return 0;
//    }
//    return -1;
//}

int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors)
{
    const struct flash_area *fa;
    if (flash_area_open(fa_id, &fa) != 0) {
        BOOT_LOG_ERR("Can not open fa_id=%d", fa_id);
        return -1;
    }

    uint32_t size = fa->fa_size;
    uint32_t offset = 0;
    uint32_t idx = 0;
    *count = 0;

    while (offset < size && idx < MCUBOOT_MAX_IMG_SECTORS) {
        uint32_t remain = size - offset;
        uint32_t chunk  = (remain >= SECTOR_SIZE) ? SECTOR_SIZE : remain;

        sectors[idx].fs_off  = offset;
        sectors[idx].fs_size = chunk;

        offset += chunk;
        (idx)++;
    }
    uint32_t expected = (size + SECTOR_SIZE -1)/ SECTOR_SIZE;
    if (idx != expected){
    	BOOT_LOG_WRN("get_sectors: MCUBOOT count %d sectors, correction in %d (size=0x%X)", idx, expected, size);
    }

    *count = idx;
    BOOT_LOG_INF("get_sectors: fa_id=%d => %d sectors dedected (taille=0x%X)",
                 fa_id, idx, size);
    return 0;
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

//int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector)
//{
//	BOOT_LOG_INF("hello");
//	  if (off >= fa->fa_size) {
//		  BOOT_LOG_INF("error");
//			return -1;
//		}
//    sector->fs_off = (off / SECTOR_SIZE) * SECTOR_SIZE;
//    sector->fs_size = SECTOR_SIZE;
//
//    return 0;
//}
int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector)
{
    const struct flash_area *fa_ptr = fa;


    if (fa_ptr == NULL) {
        uint8_t default_id = 0; // par défaut slot0
        if (flash_area_open(default_id, &fa_ptr) != 0) {
            BOOT_LOG_ERR("get_sector: impossible d'ouvrir slot par défaut");
            return -1;
        }
    }


    if (off >= fa_ptr->fa_size) {
        BOOT_LOG_ERR("get_sector: offset 0x%X hors du slot (taille=0x%X)", off, fa_ptr->fa_size);
        return -1;
    }

    uint32_t index  = off / SECTOR_SIZE;
    uint32_t remain = fa_ptr->fa_size - (index * SECTOR_SIZE);

    sector->fs_off  = index * SECTOR_SIZE;
    sector->fs_size = (remain < SECTOR_SIZE) ? remain : SECTOR_SIZE;

    BOOT_LOG_INF("get_sector: index=%d fs_off=0x%X fs_size=0x%X",
                 index, sector->fs_off, sector->fs_size);
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

