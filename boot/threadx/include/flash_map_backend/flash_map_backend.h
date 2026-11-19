/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#ifndef H_FLASH_MAP_BACKEND_H_
#define H_FLASH_MAP_BACKEND_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Structure describing a flash area / partition.
 */
struct flash_area {
    uint8_t  fa_id;            /**< Partition identifier (e.g. PRIMARY, SECONDARY, SCRATCH) */
    uint8_t  fa_device_id;     /**< Flash device identifier (0 = internal flash) */
    uint16_t pad16;            /**< Alignment padding */
    uint32_t fa_off;           /**< Byte offset from the beginning of device flash */
    uint32_t fa_size;          /**< Size of the flash area in bytes */
    void    *fa_driver_data;   /**< Optional driver private context */
};

/**
 * Structure describing a single physical sector within a flash area.
 */
struct flash_sector {
    uint32_t fs_off;           /**< Offset of this sector relative to start of flash area */
    uint32_t fs_size;          /**< Size of this sector in bytes */
};

/*
 * Inline property getters.
 */
static inline uint8_t flash_area_get_id(const struct flash_area *fa)
{
    return fa->fa_id;
}

static inline uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
    return fa->fa_device_id;
}

static inline uint32_t flash_area_get_off(const struct flash_area *fa)
{
    return fa->fa_off;
}

static inline uint32_t flash_area_get_size(const struct flash_area *fa)
{
    return fa->fa_size;
}

static inline uint32_t flash_sector_get_off(const struct flash_sector *fs)
{
    return fs->fs_off;
}

static inline uint32_t flash_sector_get_size(const struct flash_sector *fs)
{
    return fs->fs_size;
}

/*
 * MCUboot Flash Map API functions.
 */

int flash_area_open(uint8_t id, const struct flash_area **fa);
void flash_area_close(const struct flash_area *fa);

int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst, uint32_t len);
int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src, uint32_t len);
int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len);

uint32_t flash_area_align(const struct flash_area *fa);
uint8_t flash_area_erased_val(const struct flash_area *fa);

int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors);
int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector);
int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret);

int flash_area_id_from_multi_image_slot(int image_index, int slot);
int flash_area_id_from_image_slot(int slot);
int flash_area_id_to_multi_image_slot(int image_index, int area_id);
int flash_area_id_from_image_offset(uint32_t offset);

/*
 * Backend lifecycle, simulation, and verification controls.
 */
int flash_map_backend_init(void);
void flash_map_backend_reset(void);
void flash_map_set_interruption(bool enable, uint32_t ops_until_cut, jmp_buf *env);
uint8_t *flash_map_get_memory_buffer(uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif /* H_FLASH_MAP_BACKEND_H_ */
