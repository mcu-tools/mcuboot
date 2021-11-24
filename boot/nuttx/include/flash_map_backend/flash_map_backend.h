/****************************************************************************
 * boot/nuttx/include/flash_map_backend/flash_map_backend.h
 *
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

#ifndef __BOOT_NUTTX_INCLUDE_FLASH_MAP_BACKEND_FLASH_MAP_BACKEND_H
#define __BOOT_NUTTX_INCLUDE_FLASH_MAP_BACKEND_FLASH_MAP_BACKEND_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <inttypes.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Structure describing a flash area. */

struct flash_area
{
  /* MCUboot-API fields */

  uint8_t     fa_id;         /* The slot/scratch ID */
  uint8_t     fa_device_id;  /* The device ID (usually there's only one) */
  uint16_t    pad16;         /* Padding */
  uint32_t    fa_off;        /* The flash offset from the beginning */
  uint32_t    fa_size;       /* The size of this sector */

  /* NuttX implementation-specific fields */

  const char *fa_mtd_path;   /* Path for the MTD partition */
};

/* Structure describing a sector within a flash area. */

struct flash_sector
{
  /* Offset of this sector, from the start of its flash area (not device). */

  uint32_t fs_off;

  /* Size of this sector, in bytes. */

  uint32_t fs_size;
};

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Name: flash_area_get_id
 *
 * Description:
 *   Obtain the ID of a given flash area.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   The ID of the requested flash area.
 *
 ****************************************************************************/

static inline uint8_t flash_area_get_id(const struct flash_area *fa)
{
  return fa->fa_id;
}

/****************************************************************************
 * Name: flash_area_get_device_id
 *
 * Description:
 *   Obtain the ID of the device in which a given flash area resides on.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   The device ID of the requested flash area.
 *
 ****************************************************************************/

static inline uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
  return fa->fa_device_id;
}

/****************************************************************************
 * Name: flash_area_get_off
 *
 * Description:
 *   Obtain the offset, from the beginning of a device, where a given flash
 *   area starts at.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   The offset value of the requested flash area.
 *
 ****************************************************************************/

static inline uint32_t flash_area_get_off(const struct flash_area *fa)
{
  return fa->fa_off;
}

/****************************************************************************
 * Name: flash_area_get_size
 *
 * Description:
 *   Obtain the size, from the offset, of a given flash area.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   The size value of the requested flash area.
 *
 ****************************************************************************/

static inline uint32_t flash_area_get_size(const struct flash_area *fa)
{
  return fa->fa_size;
}

/****************************************************************************
 * Name: flash_sector_get_off
 *
 * Description:
 *   Obtain the offset, from the beginning of its flash area, where a given
 *   flash sector starts at.
 *
 * Input Parameters:
 *   fs - Flash sector.
 *
 * Returned Value:
 *   The offset value of the requested flash sector.
 *
 ****************************************************************************/

static inline uint32_t flash_sector_get_off(const struct flash_sector *fs)
{
    return fs->fs_off;
}

/****************************************************************************
 * Name: flash_sector_get_size
 *
 * Description:
 *   Obtain the size, from the offset, of a given flash sector.
 *
 * Input Parameters:
 *   fs - Flash sector.
 *
 * Returned Value:
 *   The size in bytes of the requested flash sector.
 *
 ****************************************************************************/

static inline uint32_t flash_sector_get_size(const struct flash_sector *fs)
{
    return fs->fs_size;
}

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: flash_area_open
 *
 * Description:
 *   Retrieve flash area from the flash map for a given partition.
 *
 * Input Parameters:
 *   id - ID of the flash partition.
 *
 * Output Parameters:
 *   fa - Pointer which will contain the reference to flash_area.
 *        If ID is unknown, it will be NULL on output.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_open(uint8_t id, const struct flash_area **fa);

/****************************************************************************
 * Name: flash_area_close
 *
 * Description:
 *   Close a given flash area.
 *
 * Input Parameters:
 *   fa - Flash area to be closed.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void flash_area_close(const struct flash_area *fa);

/****************************************************************************
 * Name: flash_area_read
 *
 * Description:
 *   Read data from flash area.
 *   Area readout boundaries are asserted before read request. API has the
 *   same limitation regarding read-block alignment and size as the
 *   underlying flash driver.
 *
 * Input Parameters:
 *   fa  - Flash area to be read.
 *   off - Offset relative from beginning of flash area to be read.
 *   len - Number of bytes to read.
 *
 * Output Parameters:
 *   dst - Buffer to store read data.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_read(const struct flash_area *fa, uint32_t off,
                    void *dst, uint32_t len);

/****************************************************************************
 * Name: flash_area_write
 *
 * Description:
 *   Write data to flash area.
 *   Area write boundaries are asserted before write request. API has the
 *   same limitation regarding write-block alignment and size as the
 *   underlying flash driver.
 *
 * Input Parameters:
 *   fa  - Flash area to be written.
 *   off - Offset relative from beginning of flash area to be written.
 *   src - Buffer with data to be written.
 *   len - Number of bytes to write.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len);

/****************************************************************************
 * Name: flash_area_erase
 *
 * Description:
 *   Erase a given flash area range.
 *   Area boundaries are asserted before erase request. API has the same
 *   limitation regarding erase-block alignment and size as the underlying
 *   flash driver.
 *
 * Input Parameters:
 *   fa  - Flash area to be erased.
 *   off - Offset relative from beginning of flash area to be erased.
 *   len - Number of bytes to be erase.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_erase(const struct flash_area *fa, uint32_t off,
                     uint32_t len);

/****************************************************************************
 * Name: flash_area_align
 *
 * Description:
 *   Get write block size of the flash area.
 *   Write block size might be treated as read block size, although most
 *   drivers support unaligned readout.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   Alignment restriction for flash writes in the given flash area.
 *
 ****************************************************************************/

uint32_t flash_area_align(const struct flash_area *fa);

/****************************************************************************
 * Name: flash_area_erased_val
 *
 * Description:
 *   Get the value expected to be read when accessing any erased flash byte.
 *   This API is compatible with the MCUboot's porting layer.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   Byte value of erased memory.
 *
 ****************************************************************************/

uint8_t flash_area_erased_val(const struct flash_area *fa);

/****************************************************************************
 * Name: flash_area_get_sectors
 *
 * Description:
 *   Retrieve info about sectors within the area.
 *
 * Input Parameters:
 *   fa_id   - ID of the flash area whose info will be retrieved.
 *   count   - On input, represents the capacity of the sectors buffer.
 *
 * Output Parameters:
 *   count   - On output, it shall contain the number of retrieved sectors.
 *   sectors - Buffer for sectors data.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors);

/****************************************************************************
 * Name: flash_area_id_from_multi_image_slot
 *
 * Description:
 *   Return the flash area ID for a given slot and a given image index
 *   (in case of a multi-image setup).
 *
 * Input Parameters:
 *   image_index - Index of the image.
 *   slot        - Image slot, which may be 0 (primary) or 1 (secondary).
 *
 * Returned Value:
 *   Flash area ID (0 or 1), or negative value in case the requested slot
 *   is invalid.
 *
 ****************************************************************************/

int flash_area_id_from_multi_image_slot(int image_index, int slot);

/****************************************************************************
 * Name: flash_area_id_from_image_slot
 *
 * Description:
 *   Return the flash area ID for a given slot.
 *
 * Input Parameters:
 *   slot - Image slot, which may be 0 (primary) or 1 (secondary).
 *
 * Returned Value:
 *   Flash area ID (0 or 1), or negative value in case the requested slot
 *   is invalid.
 *
 ****************************************************************************/

int flash_area_id_from_image_slot(int slot);

/****************************************************************************
 * Name: flash_area_id_to_multi_image_slot
 *
 * Description:
 *   Convert the specified flash area ID and image index (in case of a
 *   multi-image setup) to an image slot index.
 *
 * Input Parameters:
 *   image_index - Index of the image.
 *   area_id     - Unique identifier that is represented by fa_id in the
 *                 flash_area struct.
 * Returned Value:
 *   Image slot index (0 or 1), or negative value in case ID doesn't
 *   correspond to an image slot.
 *
 ****************************************************************************/

int flash_area_id_to_multi_image_slot(int image_index, int area_id);

/****************************************************************************
 * Name: flash_area_id_from_image_offset
 *
 * Description:
 *   Return the flash area ID for a given image offset.
 *
 * Input Parameters:
 *   offset - Image offset.
 *
 * Returned Value:
 *   Flash area ID (0 or 1), or negative value in case the requested offset
 *   is invalid.
 *
 ****************************************************************************/

int flash_area_id_from_image_offset(uint32_t offset);

#endif /* __BOOT_NUTTX_INCLUDE_FLASH_MAP_BACKEND_FLASH_MAP_BACKEND_H */
