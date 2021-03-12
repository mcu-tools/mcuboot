/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bootloader_flash.h"
#include "soc/dport_reg.h"
#include "esp32/rom/spi_flash.h"
#include "esp32/rom/cache.h"
#include "esp_err.h"

#include "mcuboot_config/mcuboot_logging.h"

/* Use first 50 blocks in MMU for bootloader_mmap,
   50th block for bootloader_flash_read
*/
#define MMU_BLOCK0_VADDR  SOC_DROM_LOW
#define MMU_SIZE          (0x320000)
#define MMU_BLOCK50_VADDR (MMU_BLOCK0_VADDR + MMU_SIZE)
#define FLASH_READ_VADDR MMU_BLOCK50_VADDR

#define MMU_FREE_PAGES    (MMU_SIZE / FLASH_BLOCK_SIZE)

static bool mapped;

/* Current bootloader mapping (ab)used for bootloader_read() */
static uint32_t current_read_mapping = UINT32_MAX;

uint32_t bootloader_mmap_get_free_pages(void)
{
    /**
     * Allow mapping up to 50 of the 51 available MMU blocks (last one used for reads)
     * Since, bootloader_mmap function below assumes it to be 0x320000 (50 pages), we can safely do this.
     */
    return MMU_FREE_PAGES;
}

const void *bootloader_mmap(uint32_t src_addr, uint32_t size)
{
    if (mapped) {
        MCUBOOT_LOG_ERR("tried to bootloader_mmap twice");
        return NULL; /* can't map twice */
    }
    if (size > MMU_SIZE) {
        MCUBOOT_LOG_ERR("bootloader_mmap excess size %x", size);
        return NULL;
    }

    uint32_t src_addr_aligned = src_addr & MMU_FLASH_MASK;
    uint32_t count = bootloader_cache_pages_to_map(size, src_addr);
    Cache_Read_Disable(0);
    Cache_Flush(0);
    MCUBOOT_LOG_DBG("mmu set paddr=%08x count=%d size=%x src_addr=%x src_addr_aligned=%x",
             src_addr & MMU_FLASH_MASK, count, size, src_addr, src_addr_aligned );
    int e = cache_flash_mmu_set(0, 0, MMU_BLOCK0_VADDR, src_addr_aligned, 64, count);
    if (e != 0) {
        MCUBOOT_LOG_ERR("cache_flash_mmu_set failed: %d\n", e);
        Cache_Read_Enable(0);
        return NULL;
    }
    Cache_Read_Enable(0);

    mapped = true;

    return (void *)(MMU_BLOCK0_VADDR + (src_addr - src_addr_aligned));
}

void bootloader_munmap(const void *mapping)
{
    if (mapped)  {
        /* Full MMU reset */
        Cache_Read_Disable(0);
        Cache_Flush(0);
        mmu_init(0);
        mapped = false;
        current_read_mapping = UINT32_MAX;
    }
}

static esp_err_t spi_to_esp_err(esp_rom_spiflash_result_t r)
{
    switch (r) {
    case ESP_ROM_SPIFLASH_RESULT_OK:
        return ESP_OK;
    case ESP_ROM_SPIFLASH_RESULT_ERR:
        return ESP_ERR_FLASH_OP_FAIL;
    case ESP_ROM_SPIFLASH_RESULT_TIMEOUT:
        return ESP_ERR_FLASH_OP_TIMEOUT;
    default:
        return ESP_FAIL;
    }
}

static esp_err_t bootloader_flash_read_no_decrypt(size_t src_addr, void *dest, size_t size)
{
    Cache_Read_Disable(0);
    Cache_Flush(0);
    esp_rom_spiflash_result_t r = esp_rom_spiflash_read(src_addr, dest, size);
    return spi_to_esp_err(r);
}

static esp_err_t bootloader_flash_read_allow_decrypt(size_t src_addr, void *dest, size_t size)
{
    uint32_t *dest_words = (uint32_t *)dest;

    for (size_t word = 0; word < size / 4; word++) {
        uint32_t word_src = src_addr + word * 4;  /* Read this offset from flash */
        uint32_t map_at = word_src & MMU_FLASH_MASK; /* Map this 64KB block from flash */
        uint32_t *map_ptr;
        if (map_at != current_read_mapping) {
            /* Move the 64KB mmu mapping window to fit map_at */
            Cache_Read_Disable(0);
            Cache_Flush(0);
            MCUBOOT_LOG_DBG("mmu set block paddr=0x%08x (was 0x%08x)", map_at, current_read_mapping);
            int e = cache_flash_mmu_set(0, 0, FLASH_READ_VADDR, map_at, 64, 1);
            if (e != 0) {
                MCUBOOT_LOG_ERR("cache_flash_mmu_set failed: %d\n", e);
                Cache_Read_Enable(0);
                return ESP_FAIL;
            }
            current_read_mapping = map_at;
            Cache_Read_Enable(0);
        }
        map_ptr = (uint32_t *)(FLASH_READ_VADDR + (word_src - map_at));
        dest_words[word] = *map_ptr;
    }
    return ESP_OK;
}

esp_err_t bootloader_flash_read(size_t src_addr, void *dest, size_t size, bool allow_decrypt)
{
    if (src_addr & 3) {
        MCUBOOT_LOG_ERR("bootloader_flash_read src_addr 0x%x not 4-byte aligned", src_addr);
        return ESP_FAIL;
    }
    if (size & 3) {
        MCUBOOT_LOG_ERR("bootloader_flash_read size 0x%x not 4-byte aligned", size);
        return ESP_FAIL;
    }
    if ((intptr_t)dest & 3) {
        MCUBOOT_LOG_ERR("bootloader_flash_read dest 0x%x not 4-byte aligned", (intptr_t)dest);
        return ESP_FAIL;
    }

    if (allow_decrypt) {
        return bootloader_flash_read_allow_decrypt(src_addr, dest, size);
    } else {
        return bootloader_flash_read_no_decrypt(src_addr, dest, size);
    }
}

esp_err_t bootloader_flash_write(size_t dest_addr, void *src, size_t size, bool write_encrypted)
{
    esp_err_t err;
    size_t alignment = write_encrypted ? 32 : 4;
    if ((dest_addr % alignment) != 0) {
        MCUBOOT_LOG_ERR("bootloader_flash_write dest_addr 0x%x not %d-byte aligned", dest_addr, alignment);
        return ESP_FAIL;
    }
    if ((size % alignment) != 0) {
        MCUBOOT_LOG_ERR("bootloader_flash_write size 0x%x not %d-byte aligned", size, alignment);
        return ESP_FAIL;
    }
    if (((intptr_t)src % 4) != 0) {
        MCUBOOT_LOG_ERR("bootloader_flash_write src 0x%x not 4 byte aligned", (intptr_t)src);
        return ESP_FAIL;
    }

    err = spi_to_esp_err(esp_rom_spiflash_unlock());
    if (err != ESP_OK) {
        return err;
    }

    if (write_encrypted) {
        return spi_to_esp_err(esp_rom_spiflash_write_encrypted(dest_addr, src, size));
    } else {
        return spi_to_esp_err(esp_rom_spiflash_write(dest_addr, src, size));
    }
}

esp_err_t bootloader_flash_erase_sector(size_t sector)
{
    return spi_to_esp_err(esp_rom_spiflash_erase_sector(sector));
}

esp_err_t bootloader_flash_erase_range(uint32_t start_addr, uint32_t size)
{
    if (start_addr % FLASH_SECTOR_SIZE != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (size % FLASH_SECTOR_SIZE != 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t start = start_addr / FLASH_SECTOR_SIZE;
    size_t end = start + size / FLASH_SECTOR_SIZE;
    const size_t sectors_per_block = FLASH_BLOCK_SIZE / FLASH_SECTOR_SIZE;

    esp_rom_spiflash_result_t rc = ESP_ROM_SPIFLASH_RESULT_OK;
    for (size_t sector = start; sector != end && rc == ESP_ROM_SPIFLASH_RESULT_OK; ) {
        if (sector % sectors_per_block == 0 && end - sector >= sectors_per_block) {
            rc = esp_rom_spiflash_erase_block(sector / sectors_per_block);
            sector += sectors_per_block;
        } else {
            rc = esp_rom_spiflash_erase_sector(sector);
            ++sector;
        }
    }
    return spi_to_esp_err(rc);
}
