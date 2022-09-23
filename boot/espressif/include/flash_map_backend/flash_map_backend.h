/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <inttypes.h>

struct flash_area {
  uint8_t  fa_id;         /** The slot/scratch identification */
  uint8_t  fa_device_id;  /** The device id (usually there's only one) */
  uint16_t pad16;
  uint32_t fa_off;        /** The flash offset from the beginning */
  uint32_t fa_size;       /** The size of this sector */
};

//! Structure describing a sector within a flash area.
struct flash_sector {
  //! Offset of this sector, from the start of its flash area (not device).
  uint32_t fs_off;

  //! Size of this sector, in bytes.
  uint32_t fs_size;
};

static inline uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
    return (uint8_t)fa->fa_device_id;
}
static inline uint32_t flash_area_get_off(const struct flash_area *fa)
{
	return (uint32_t)fa->fa_off;
}

static inline uint32_t flash_area_get_size(const struct flash_area *fa)
{
	return (uint32_t)fa->fa_size;
}

static inline uint8_t flash_area_get_id(const struct flash_area *fa)
{
	return fa->fa_id;
}

static inline uint32_t flash_sector_get_off(const struct flash_sector *fs)
{
	return fs->fs_off;
}

static inline uint32_t flash_sector_get_size(const struct flash_sector *fs)
{
	return fs->fs_size;
}

//! Opens the area for use. id is one of the `fa_id`s */
int flash_area_open(uint8_t id, const struct flash_area **area_outp);
void flash_area_close(const struct flash_area *fa);

//! Reads `len` bytes of flash memory at `off` to the buffer at `dst`
int flash_area_read(const struct flash_area *fa, uint32_t off,
                    void *dst, uint32_t len);
//! Writes `len` bytes of flash memory at `off` from the buffer at `src`
int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len);
//! Erases `len` bytes of flash memory at `off`
int flash_area_erase(const struct flash_area *fa,
                     uint32_t off, uint32_t len);

//! Returns this `flash_area`s alignment
uint32_t flash_area_align(const struct flash_area *area);
//! Returns the value read from an erased flash area byte
uint8_t flash_area_erased_val(const struct flash_area *area);

//! Given flash area ID, return info about sectors within the area
int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors);

//! Retrieve the flash sector a given offset belongs to.
int flash_area_sector_from_off(uint32_t off, struct flash_sector *sector);

//! Returns the `fa_id` for slot, where slot is 0 (primary) or 1 (secondary).
//!
//! `image_index` (0 or 1) is the index of the image. Image index is
//!  relevant only when multi-image support support is enabled
int flash_area_id_from_multi_image_slot(int image_index, int slot);
int flash_area_id_from_image_slot(int slot);
int flash_area_to_sectors(int idx, int *cnt, struct flash_area *fa);
