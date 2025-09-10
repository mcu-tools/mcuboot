/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <bootutil/bootutil.h>
#include <bootutil/bootutil_log.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "rom/cache.h"
#include "hal/cache_hal.h"
#include "hal/mmu_hal.h"
#include "bootloader_flash_priv.h"
#include "esp_flash_encrypt.h"
#include "mcuboot_config/mcuboot_config.h"

#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(arr)           (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef MIN
#  define MIN(a, b)                 (((a) < (b)) ? (a) : (b))
#endif

#ifndef ALIGN_UP
#  define ALIGN_UP(num, align)      (((num) + ((align) - 1)) & ~((align) - 1))
#endif

#ifndef ALIGN_DOWN
#  define ALIGN_DOWN(num, align)    ((num) & ~((align) - 1))
#endif

#ifndef ALIGN_OFFSET
#  define ALIGN_OFFSET(num, align)  ((num) & ((align) - 1))
#endif

#ifndef IS_ALIGNED
#  define IS_ALIGNED(num, align)    (ALIGN_OFFSET((num), (align)) == 0)
#endif

#define FLASH_BUFFER_SIZE           256 /* SPI Flash block size */

_Static_assert(IS_ALIGNED(FLASH_BUFFER_SIZE, 4), "Buffer size for SPI Flash operations must be 4-byte aligned.");

#define BOOTLOADER_START_ADDRESS CONFIG_BOOTLOADER_OFFSET_IN_FLASH
#define BOOTLOADER_SIZE CONFIG_ESP_BOOTLOADER_SIZE

#define IMAGE0_PRIMARY_START_ADDRESS CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS
#define IMAGE0_SECONDARY_START_ADDRESS CONFIG_ESP_IMAGE0_SECONDARY_START_ADDRESS
#if (MCUBOOT_IMAGE_NUMBER == 2)
#define IMAGE1_PRIMARY_START_ADDRESS CONFIG_ESP_IMAGE1_PRIMARY_START_ADDRESS
#define IMAGE1_SECONDARY_START_ADDRESS CONFIG_ESP_IMAGE1_SECONDARY_START_ADDRESS
#endif
#define APPLICATION_SIZE CONFIG_ESP_APPLICATION_SIZE

#ifdef CONFIG_ESP_BOOT_SWAP_USING_SCRATCH
#define SCRATCH_OFFSET CONFIG_ESP_SCRATCH_OFFSET
#define SCRATCH_SIZE CONFIG_ESP_SCRATCH_SIZE
#endif

extern int ets_printf(const char *fmt, ...);

static const struct flash_area bootloader = {
    .fa_id = FLASH_AREA_BOOTLOADER,
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = BOOTLOADER_START_ADDRESS,
    .fa_size = BOOTLOADER_SIZE,
};

static const struct flash_area primary_img0 = {
    .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = IMAGE0_PRIMARY_START_ADDRESS,
    .fa_size = APPLICATION_SIZE,
};

static const struct flash_area secondary_img0 = {
    .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = IMAGE0_SECONDARY_START_ADDRESS,
    .fa_size = APPLICATION_SIZE,
};

#if (MCUBOOT_IMAGE_NUMBER == 2)
static const struct flash_area primary_img1 = {
    .fa_id = FLASH_AREA_IMAGE_PRIMARY(1),
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = IMAGE1_PRIMARY_START_ADDRESS,
    .fa_size = APPLICATION_SIZE,
};

static const struct flash_area secondary_img1 = {
    .fa_id = FLASH_AREA_IMAGE_SECONDARY(1),
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = IMAGE1_SECONDARY_START_ADDRESS,
    .fa_size = APPLICATION_SIZE,
};
#endif

#ifdef CONFIG_ESP_BOOT_SWAP_USING_SCRATCH
static const struct flash_area scratch_img0 = {
    .fa_id = FLASH_AREA_IMAGE_SCRATCH,
    .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
    .fa_off = SCRATCH_OFFSET,
    .fa_size = SCRATCH_SIZE,
};
#endif

static const struct flash_area *s_flash_areas[] = {
    &bootloader,
    &primary_img0,
    &secondary_img0,
#if (MCUBOOT_IMAGE_NUMBER == 2)
    &primary_img1,
    &secondary_img1,
#endif
#ifdef CONFIG_ESP_BOOT_SWAP_USING_SCRATCH
    &scratch_img0,
#endif
};

static const struct flash_area *prv_lookup_flash_area(uint8_t id) {
    for (size_t i = 0; i < ARRAY_SIZE(s_flash_areas); i++) {
        const struct flash_area *area = s_flash_areas[i];
        if (id == area->fa_id) {
            return area;
        }
    }
    return NULL;
}

int flash_area_open(uint8_t id, const struct flash_area **area_outp)
{
    BOOT_LOG_DBG("%s: ID=%d", __func__, (int)id);
    const struct flash_area *area = prv_lookup_flash_area(id);
    *area_outp = area;
    return area != NULL ? 0 : -1;
}

void flash_area_close(const struct flash_area *area)
{

}

static void flush_cache(size_t start_addr, size_t length)
{
#if CONFIG_IDF_TARGET_ESP32
    Cache_Read_Disable(0);
    Cache_Flush(0);
    Cache_Read_Enable(0);
#else
    uint32_t vaddr = 0;

    mmu_hal_paddr_to_vaddr(0, start_addr, MMU_TARGET_FLASH0, MMU_VADDR_DATA, &vaddr);
    if ((void *)vaddr != NULL) {
        cache_hal_invalidate_addr(vaddr, length);
    }
#endif
}

static bool aligned_flash_read(uintptr_t addr, void *dest, size_t size)
{
    if (IS_ALIGNED(addr, 4) && IS_ALIGNED((uintptr_t)dest, 4) && IS_ALIGNED(size, 4)) {
        /* A single read operation is enough when when all parameters are aligned */

        return bootloader_flash_read(addr, dest, size, true) == ESP_OK;
    }

    const uint32_t aligned_addr = ALIGN_DOWN(addr, 4);
    const uint32_t addr_offset = ALIGN_OFFSET(addr, 4);
    uint32_t bytes_remaining = size;
    uint8_t read_data[FLASH_BUFFER_SIZE] = {0};

    /* Align the read address to 4-byte boundary and ensure read size is a multiple of 4 bytes */

    uint32_t bytes = MIN(bytes_remaining + addr_offset, sizeof(read_data));
    if (bootloader_flash_read(aligned_addr, read_data, ALIGN_UP(bytes, 4), true) != ESP_OK) {
        return false;
    }

    /* Skip non-useful data which may have been read for adjusting the alignment */

    uint32_t bytes_read = bytes - addr_offset;
    memcpy(dest, &read_data[addr_offset], bytes_read);

    bytes_remaining -= bytes_read;

    /* Read remaining data from Flash in case requested size is greater than buffer size */

    uint32_t offset = bytes;

    while (bytes_remaining != 0) {
        bytes = MIN(bytes_remaining, sizeof(read_data));
        if (bootloader_flash_read(aligned_addr + offset, read_data, ALIGN_UP(bytes, 4), true) != ESP_OK) {
            return false;
        }

        memcpy(&((uint8_t *)dest)[bytes_read], read_data, bytes);

        offset += bytes;
        bytes_read += bytes;
        bytes_remaining -= bytes;
    }

    return true;
}

int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
                    uint32_t len)
{
    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    const uint32_t end_offset = off + len;
    if (end_offset > fa->fa_size) {
        BOOT_LOG_ERR("%s: Out of Bounds (0x%x vs 0x%x)", __func__, end_offset, fa->fa_size);
        return -1;
    }

    bool success = aligned_flash_read(fa->fa_off + off, dst, len);
    if (!success) {
        BOOT_LOG_ERR("%s: Flash read failed", __func__);

        return -1;
    }

    return 0;
}

static bool aligned_flash_write(size_t dest_addr, const void *src, size_t size, bool erase)
{
#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
        bool flash_encryption_enabled = esp_flash_encryption_enabled();
#else
        bool flash_encryption_enabled = false;
#endif

    /* When flash encryption is enabled, write alignment is 32 bytes, however to avoid
     * inconsistences the region may be erased right before writting, thus the alignment
     * is set to the erase required alignment (FLASH_SECTOR_SIZE).
     * When flash encryption is not enabled, regular write alignment is 4 bytes.
     */
    size_t alignment = flash_encryption_enabled ? (erase ? FLASH_SECTOR_SIZE : 32) : 4;

    if (IS_ALIGNED(dest_addr, alignment) && IS_ALIGNED((uintptr_t)src, 4) && IS_ALIGNED(size, alignment)) {
        /* A single write operation is enough when all parameters are aligned */

        if (flash_encryption_enabled && erase) {
            if (bootloader_flash_erase_range(dest_addr, size) != ESP_OK) {
                BOOT_LOG_ERR("%s: Flash erase failed at 0x%08x", __func__, (uint32_t)dest_addr);
                return false;
            }
            flush_cache(dest_addr, size);
        }

        if (bootloader_flash_write(dest_addr, (void *)src, size, flash_encryption_enabled) == ESP_OK) {
            flush_cache(dest_addr, size);
            return true;
        } else {
            BOOT_LOG_ERR("%s: Flash write failed at 0x%08x", __func__, (uint32_t)dest_addr);
            return false;
        }
    }
    BOOT_LOG_DBG("%s: forcing unaligned write dest_addr: 0x%08x src: 0x%08x size: 0x%x erase: %c",
                 __func__, (uint32_t)dest_addr, (uint32_t)src, size, erase ? 't' : 'f');

    uint8_t write_aux_buf[FLASH_SECTOR_SIZE] __attribute__((aligned(32))) = {0};

    size_t write_addr = dest_addr;
    size_t bytes_remaining = size;
    size_t src_offset = 0;

    while (bytes_remaining > 0) {
        size_t aligned_curr_addr = ALIGN_DOWN(write_addr, alignment);
        size_t curr_buf_off = write_addr - aligned_curr_addr;
        size_t chunk_len = MIN(bytes_remaining, FLASH_SECTOR_SIZE - curr_buf_off);

        /* Read data before modifying */
        if (bootloader_flash_read(aligned_curr_addr, write_aux_buf,
                                  ALIGN_UP(chunk_len, alignment), true) != ESP_OK) {
            BOOT_LOG_ERR("%s: Flash read failed at 0x%08x", __func__, (uint32_t)aligned_curr_addr);
            return false;
        }

        /* Erase if needed */
        if (flash_encryption_enabled && erase) {
            if (bootloader_flash_erase_range(aligned_curr_addr,
                                             ALIGN_UP(chunk_len, FLASH_SECTOR_SIZE)) != ESP_OK) {
                BOOT_LOG_ERR("%s: Flash erase failed at 0x%08x", __func__, (uint32_t)aligned_curr_addr);
                return false;
            }
            flush_cache(aligned_curr_addr, ALIGN_UP(chunk_len, FLASH_SECTOR_SIZE));
        }

        /* Merge data into buffer */
        memcpy(&write_aux_buf[curr_buf_off], &((uint8_t *)src)[src_offset], chunk_len);

        /* Write back aligned chunk */
        if (bootloader_flash_write(aligned_curr_addr, write_aux_buf,
                                   ALIGN_UP(chunk_len, alignment),
                                   flash_encryption_enabled) != ESP_OK) {
            BOOT_LOG_ERR("%s: Flash write failed at 0x%08x", __func__, (uint32_t)aligned_curr_addr);
            return false;
        }
        flush_cache(aligned_curr_addr, ALIGN_UP(chunk_len, alignment));

        write_addr     += chunk_len;
        src_offset     += chunk_len;
        bytes_remaining -= chunk_len;
    }

    return true;
}

static bool aligned_flash_erase(size_t addr, size_t size)
{
    if (IS_ALIGNED(addr, FLASH_SECTOR_SIZE) && IS_ALIGNED(size, FLASH_SECTOR_SIZE)) {
        /* A single erase operation is enough when all parameters are aligned */

        if (bootloader_flash_erase_range(addr, size) == ESP_OK) {
            flush_cache(addr, size);
            return true;
        } else {
            BOOT_LOG_ERR("%s: Flash erase failed at 0x%08x", __func__, (uint32_t)addr);
            return false;
        }
    }

    const size_t sector_size = FLASH_SECTOR_SIZE;
    const size_t start_addr = ALIGN_DOWN(addr, sector_size);
    const size_t end_addr = ALIGN_UP(addr + size, sector_size);
    const size_t total_len = end_addr - start_addr;

    BOOT_LOG_DBG("%s: forcing unaligned erase on sector Offset: 0x%08x Length: 0x%x total_len: 0x%x",
                __func__, (uint32_t)addr, (int)size, total_len);

    uint8_t erase_aux_buf[FLASH_SECTOR_SIZE] __attribute__((aligned(32))) = {0};
    size_t current_addr = start_addr;
    while (current_addr < end_addr) {
        bool preserve_head = (addr > current_addr);
        bool preserve_tail = ((addr + size) < (current_addr + sector_size));

        if (preserve_head || preserve_tail) {
            /* Read full sector before erasing */
            if (bootloader_flash_read(current_addr, erase_aux_buf, sector_size, true) != ESP_OK) {
                BOOT_LOG_ERR("%s: Flash read failed at 0x%08x", __func__, (uint32_t)current_addr);
                return false;
            }

            /* Calculate the range of the erase: data between erase_start and erase_end
             * will not be written back
             */
            size_t erase_start = (addr > current_addr) ? (addr - current_addr) : 0;
            size_t erase_end = MIN(current_addr + sector_size, addr + size) - current_addr;

            BOOT_LOG_INF("%s: partial sector erase from: 0x%08x to: 0x%08x Length: 0x%x",
                        __func__, (uint32_t)(current_addr + erase_start),
                        (uint32_t)(current_addr + erase_end), erase_end - erase_start);

            /* Erase full sector */
            if (bootloader_flash_erase_range(current_addr, sector_size) != ESP_OK) {
                BOOT_LOG_ERR("%s: Flash erase failed at 0x%08x", __func__, (uint32_t)current_addr);
                return false;
            }
            flush_cache(current_addr, sector_size);

            if (preserve_head) {
                /* Write back preserved head data up to erase_start */
                if (!aligned_flash_write(current_addr, erase_aux_buf, erase_start, false)) {
                    BOOT_LOG_ERR("%s: Flash write failed at 0x%08x", __func__, (uint32_t)current_addr);
                    return false;
                }
            }

            if (preserve_tail) {
                /* Write back preserved tail data from erase_end up to sector end */
                if (!aligned_flash_write(current_addr + erase_end, &erase_aux_buf[erase_end], sector_size - erase_end, false)) {
                    BOOT_LOG_ERR("%s: Flash write failed at 0x%08x", __func__, (uint32_t)current_addr + erase_end);
                    return false;
                }
            }
            current_addr += sector_size;
        } else {
            /* Full sector erase is safe, erase the next consecutive full sectors */
            size_t contiguous_size = ALIGN_DOWN(addr + size, sector_size) - current_addr;
            BOOT_LOG_DBG("%s: sectors erased from: 0x%08x length: 0x%x",
                         __func__, (uint32_t)current_addr, contiguous_size);
            if (bootloader_flash_erase_range(current_addr, contiguous_size) != ESP_OK) {
                BOOT_LOG_ERR("%s: Flash erase failed at 0x%08x", __func__, (uint32_t)current_addr);
                return false;
            }
            flush_cache(current_addr, contiguous_size);

            current_addr += contiguous_size;
        }
    }

    return true;
}

int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src,
                     uint32_t len)
{
    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    const uint32_t end_offset = off + len;
    if (end_offset > fa->fa_size) {
        BOOT_LOG_ERR("%s: Out of Bounds (0x%x vs 0x%x)", __func__, end_offset, fa->fa_size);
        return -1;
    }

    const uint32_t start_addr = fa->fa_off + off;
    bool erase = false;
    BOOT_LOG_DBG("%s: Addr: 0x%08x Length: %d (0x%x)", __func__, (int)start_addr, (int)len, (int)len);

#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
    if (esp_flash_encryption_enabled()) {
        /* Ensuring flash region has been erased before writing in order to
         * avoid inconsistences when hardware flash encryption is enabled.
         */
        erase = true;
    }
#endif

    if (!aligned_flash_write(start_addr, src, len, erase)) {
        BOOT_LOG_ERR("%s: Flash write failed", __func__);
        return -1;
    }

    return 0;
}

int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    const uint32_t start_addr = fa->fa_off + off;
    BOOT_LOG_DBG("%s: Addr: 0x%08x Length: %d (0x%x)", __func__, (int)start_addr, (int)len, (int)len);

    if (!aligned_flash_erase(start_addr, len)) {
        BOOT_LOG_ERR("%s: Flash erase failed", __func__);
        return -1;
    }

#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
    uint8_t write_data[FLASH_BUFFER_SIZE];
    memset(write_data, flash_area_erased_val(fa), sizeof(write_data));
    uint32_t bytes_remaining = len;
    uint32_t offset = start_addr;

    uint32_t bytes_written = MIN(sizeof(write_data), len);
    if (esp_flash_encryption_enabled()) {
        /* When hardware flash encryption is enabled, force expected erased
         * value (0xFF) into flash when erasing a region.
         *
         * This is handled on this implementation because MCUboot's state
         * machine relies on erased valued data (0xFF) read from a
         * previously erased region that was not written yet, however when
         * hardware flash encryption is enabled, the flash read always
         * decrypts what's being read from flash, thus a region that was
         * erased would not be read as what MCUboot expected (0xFF).
         */
        while (bytes_remaining != 0) {
            if (!aligned_flash_write(offset, write_data, bytes_written, false)) {
                BOOT_LOG_ERR("%s: Flash erase failed", __func__);
                return -1;
            }
            offset += bytes_written;
            bytes_remaining -= bytes_written;
        }
    }
#endif

#if VALIDATE_PROGRAM_OP && !defined(CONFIG_SECURE_FLASH_ENC_ENABLED)
    for (size_t i = 0; i < len; i++) {
        uint8_t *val = (void *)(start_addr + i);
        if (*val != 0xff) {
            BOOT_LOG_ERR("%s: Erase at 0x%x Failed", __func__, (int)val);
            assert(0);
        }
    }
#endif

    return 0;
}

uint32_t flash_area_align(const struct flash_area *area)
{
    static size_t align = 0;

    if (align == 0) {
#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
        bool flash_encryption_enabled = esp_flash_encryption_enabled();
#else
        bool flash_encryption_enabled = false;
#endif

        if (flash_encryption_enabled) {
            align = 32;
        } else {
            align = 4;
        }
    }
    return align;
}

uint8_t flash_area_erased_val(const struct flash_area *area)
{
    return 0xff;
}

int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors)
{
    const struct flash_area *fa = prv_lookup_flash_area(fa_id);
    if (fa->fa_device_id != FLASH_DEVICE_INTERNAL_FLASH) {
        return -1;
    }

    const size_t sector_size = FLASH_SECTOR_SIZE;
    uint32_t total_count = 0;
    for (size_t off = 0; off < fa->fa_size; off += sector_size) {
        // Note: Offset here is relative to flash area, not device
        sectors[total_count].fs_off = off;
        sectors[total_count].fs_size = sector_size;
        total_count++;
    }

    *count = total_count;
    return 0;
}

int flash_area_sector_from_off(uint32_t off, struct flash_sector *sector)
{
    sector->fs_off = (off / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    sector->fs_size = FLASH_SECTOR_SIZE;

    return 0;
}

int flash_area_get_sector(const struct flash_area *fa, uint32_t off,
                          struct flash_sector *sector)
{
    sector->fs_off = (off / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    sector->fs_size = FLASH_SECTOR_SIZE;

    return 0;
}

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    BOOT_LOG_DBG("%s", __func__);
    switch (slot) {
      case 0:
        return FLASH_AREA_IMAGE_PRIMARY(image_index);
      case 1:
        return FLASH_AREA_IMAGE_SECONDARY(image_index);
    }

    BOOT_LOG_ERR("Unexpected Request: image_index=%d, slot=%d", image_index, slot);
    return -1; /* flash_area_open will fail on that */
}

int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}

int flash_area_to_sectors(int idx, int *cnt, struct flash_area *fa)
{
    return -1;
}

void mcuboot_assert_handler(const char *file, int line, const char *func)
{
    ets_printf("assertion failed: file \"%s\", line %d, func: %s\n", file, line, func);
    abort();
}
