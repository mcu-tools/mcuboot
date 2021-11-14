/****************************************************************************
 * boot/nuttx/src/flash_map_backend/flash_map_backend.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>

#include <bootutil/bootutil_log.h>

#include "flash_map_backend/flash_map_backend.h"
#include "os/os_malloc.h"
#include "sysflash/sysflash.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ARRAYSIZE(x)                (sizeof((x)) / sizeof((x)[0]))

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct flash_device_s
{
  /* Reference to the flash area configuration parameters */

  struct flash_area *fa_cfg;

  /* Geometry characteristics of the underlying MTD device */

  struct mtd_geometry_s mtdgeo;

  /* Partition information */

  struct partition_info_s partinfo;

  int     fd;          /* File descriptor for an open flash area */
  int32_t refs;        /* Reference counter */
  uint8_t erase_state; /* Byte value of the flash erased state */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct flash_area g_primary_img0 =
{
  .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
  .fa_device_id = 0,
  .fa_off = 0,
  .fa_size = 0,
  .fa_mtd_path = CONFIG_MCUBOOT_PRIMARY_SLOT_PATH
};

static struct flash_device_s g_primary_priv =
{
  .fa_cfg = &g_primary_img0,
  .mtdgeo =
            {
              0
            },
  .partinfo =
              {
                0
              },
  .fd = -1,
  .refs = 0,
  .erase_state = CONFIG_MCUBOOT_DEFAULT_FLASH_ERASE_STATE
};

static struct flash_area g_secondary_img0 =
{
  .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
  .fa_device_id = 0,
  .fa_off = 0,
  .fa_size = 0,
  .fa_mtd_path = CONFIG_MCUBOOT_SECONDARY_SLOT_PATH
};

static struct flash_device_s g_secondary_priv =
{
  .fa_cfg = &g_secondary_img0,
  .mtdgeo =
            {
              0
            },
  .partinfo =
              {
                0
              },
  .fd = -1,
  .refs = 0,
  .erase_state = CONFIG_MCUBOOT_DEFAULT_FLASH_ERASE_STATE
};

static struct flash_area g_scratch_img0 =
{
  .fa_id = FLASH_AREA_IMAGE_SCRATCH,
  .fa_device_id = 0,
  .fa_off = 0,
  .fa_size = 0,
  .fa_mtd_path = CONFIG_MCUBOOT_SCRATCH_PATH
};

static struct flash_device_s g_scratch_priv =
{
  .fa_cfg = &g_scratch_img0,
  .mtdgeo =
            {
              0
            },
  .partinfo =
              {
                0
              },
  .fd = -1,
  .refs = 0,
  .erase_state = CONFIG_MCUBOOT_DEFAULT_FLASH_ERASE_STATE
};

static struct flash_device_s *g_flash_devices[] =
{
  &g_primary_priv,
  &g_secondary_priv,
  &g_scratch_priv,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lookup_flash_device_by_id
 *
 * Description:
 *   Retrieve flash device from a given flash area ID.
 *
 * Input Parameters:
 *   fa_id - ID of the flash area.
 *
 * Returned Value:
 *   Reference to the found flash device, or NULL in case it does not exist.
 *
 ****************************************************************************/

static struct flash_device_s *lookup_flash_device_by_id(uint8_t fa_id)
{
  size_t i;

  for (i = 0; i < ARRAYSIZE(g_flash_devices); i++)
    {
      struct flash_device_s *dev = g_flash_devices[i];

      if (fa_id == dev->fa_cfg->fa_id)
        {
          return dev;
        }
    }

  return NULL;
}

/****************************************************************************
 * Name: lookup_flash_device_by_offset
 *
 * Description:
 *   Retrieve flash device from a given flash area offset.
 *
 * Input Parameters:
 *   offset - Offset of the flash area.
 *
 * Returned Value:
 *   Reference to the found flash device, or NULL in case it does not exist.
 *
 ****************************************************************************/

static struct flash_device_s *lookup_flash_device_by_offset(uint32_t offset)
{
  size_t i;

  for (i = 0; i < ARRAYSIZE(g_flash_devices); i++)
    {
      struct flash_device_s *dev = g_flash_devices[i];

      if (offset == dev->fa_cfg->fa_off)
        {
          return dev;
        }
    }

  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: flash_area_open
 *
 * Description:
 *   Retrieve flash area from the flash map for a given ID.
 *
 * Input Parameters:
 *   fa_id - ID of the flash area.
 *
 * Output Parameters:
 *   fa    - Pointer which will contain the reference to flash_area.
 *           If ID is unknown, it will be NULL on output.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_open(uint8_t id, const struct flash_area **fa)
{
  struct flash_device_s *dev;
  int fd;
  int ret;

  BOOT_LOG_INF("ID:%" PRIu8, id);

  dev = lookup_flash_device_by_id(id);
  if (dev == NULL)
    {
      BOOT_LOG_ERR("Undefined flash area: %d", id);

      return ERROR;
    }

  *fa = dev->fa_cfg;

  if (dev->refs++ > 0)
    {
      BOOT_LOG_INF("Flash area ID %d already open, count: %d (+)",
                   id, dev->refs);

      return OK;
    }

  fd = open(dev->fa_cfg->fa_mtd_path, O_RDWR);
  if (fd < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Error opening MTD device: %d", errcode);

      goto errout;
    }

  ret = ioctl(fd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&dev->mtdgeo));
  if (ret < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Error retrieving MTD device geometry: %d", errcode);

      goto errout_with_fd;
    }

  ret = ioctl(fd, BIOC_PARTINFO, (unsigned long)((uintptr_t)&dev->partinfo));
  if (ret < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Error retrieving MTD partition info: %d", errcode);

      goto errout_with_fd;
    }

  ret = ioctl(fd, MTDIOC_ERASESTATE,
              (unsigned long)((uintptr_t)&dev->erase_state));
  if (ret < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Error retrieving MTD device erase state: %d", errcode);

      goto errout_with_fd;
    }

  dev->fa_cfg->fa_off = dev->partinfo.startsector * dev->partinfo.sectorsize;
  dev->fa_cfg->fa_size = dev->partinfo.numsectors * dev->partinfo.sectorsize;

  BOOT_LOG_INF("Flash area offset: 0x%" PRIx32, dev->fa_cfg->fa_off);
  BOOT_LOG_INF("Flash area size: %" PRIu32, dev->fa_cfg->fa_size);
  BOOT_LOG_INF("MTD erase state: 0x%" PRIx8, dev->erase_state);

  dev->fd = fd;

  BOOT_LOG_INF("Flash area %d open, count: %d (+)", id, dev->refs);

  return OK;

errout_with_fd:
  close(fd);

errout:
  --dev->refs;

  return ERROR;
}

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

void flash_area_close(const struct flash_area *fa)
{
  BOOT_LOG_INF("ID:%" PRIu8, fa->fa_id);

  struct flash_device_s *dev = lookup_flash_device_by_id(fa->fa_id);

  DEBUGASSERT(dev != NULL);

  if (dev->refs == 0)
    {
      /* No need to close an unopened flash area, avoid an overflow of the
       * counter.
       */

      return;
    }

  BOOT_LOG_INF("Close request for flash area %" PRIu8 ", count: %d (-)",
               fa->fa_id, dev->refs);

  if (--dev->refs == 0)
    {
      close(dev->fd);
      dev->fd = -1;

      BOOT_LOG_INF("Flash area %" PRIu8 " closed", fa->fa_id);
    }
}

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
                    void *dst, uint32_t len)
{
  struct flash_device_s *dev;
  off_t seekpos;
  ssize_t nbytes;

  BOOT_LOG_INF("ID:%" PRIu8 " offset:%" PRIu32 " length:%" PRIu32,
               fa->fa_id, off, len);

  dev = lookup_flash_device_by_id(fa->fa_id);

  DEBUGASSERT(dev != NULL);

  if (off + len > fa->fa_size)
    {
      BOOT_LOG_ERR("Attempt to read out of flash area bounds");

      return ERROR;
    }

  /* Reposition the file offset from the beginning of the flash area */

  seekpos = lseek(dev->fd, (off_t)off, SEEK_SET);
  if (seekpos != off)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Seek to offset %" PRIu32 " failed: %d", off, errcode);

      return ERROR;
    }

  /* Read the flash block into memory */

  nbytes = read(dev->fd, dst, len);
  if (nbytes < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Read from %s failed: %d", fa->fa_mtd_path, errcode);

      return ERROR;
    }

  return OK;
}

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
                     const void *src, uint32_t len)
{
  struct flash_device_s *dev;
  off_t seekpos;
  ssize_t nbytes;

  BOOT_LOG_INF("ID:%" PRIu8 " offset:%" PRIu32 " length:%" PRIu32,
               fa->fa_id, off, len);

  dev = lookup_flash_device_by_id(fa->fa_id);

  DEBUGASSERT(dev != NULL);

  if (off + len > fa->fa_size)
    {
      BOOT_LOG_ERR("Attempt to write out of flash area bounds");

      return ERROR;
    }

  /* Reposition the file offset from the beginning of the flash area */

  seekpos = lseek(dev->fd, (off_t)off, SEEK_SET);
  if (seekpos != off)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Seek to offset %" PRIu32 " failed: %d", off, errcode);

      return ERROR;
    }

  /* Write the buffer to the flash block */

  nbytes = write(dev->fd, src, len);
  if (nbytes < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Write to %s failed: %d", fa->fa_mtd_path, errcode);

      return ERROR;
    }

  return OK;
}

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

int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
  int ret;
  void *buffer;
  size_t i;
  struct flash_device_s *dev = lookup_flash_device_by_id(fa->fa_id);
  const size_t sector_size = dev->mtdgeo.erasesize;
  const uint8_t erase_val = dev->erase_state;

  BOOT_LOG_INF("ID:%" PRIu8 " offset:%" PRIu32 " length:%" PRIu32,
               fa->fa_id, off, len);

  buffer = malloc(sector_size);
  if (buffer == NULL)
    {
      BOOT_LOG_ERR("Failed to allocate erase buffer");

      return ERROR;
    }

  memset(buffer, erase_val, sizeof(*buffer));

  i = 0;

  do
    {
      BOOT_LOG_DBG("Erasing %zu bytes at offset %" PRIu32,
                   sector_size, off + i);

      ret = flash_area_write(fa, off + i, buffer, sector_size);
      i += sector_size;
    }
  while (ret == OK && i < (len - sector_size));

  if (ret == OK)
    {
      BOOT_LOG_DBG("Erasing %zu bytes at offset %" PRIu32,
                   len - i, off + i);

      ret = flash_area_write(fa, off + i, buffer, len - i);
    }

  free(buffer);

  return ret;
}

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

uint8_t flash_area_align(const struct flash_area *fa)
{
  /* MTD access alignment is handled by the character and block device
   * drivers.
   */

  const uint8_t minimum_write_length = 1;

  BOOT_LOG_INF("ID:%" PRIu8 " align:%" PRIu8,
               fa->fa_id, minimum_write_length);

  return minimum_write_length;
}

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

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
  struct flash_device_s *dev;
  uint8_t erased_val;

  dev = lookup_flash_device_by_id(fa->fa_id);

  DEBUGASSERT(dev != NULL);

  erased_val = dev->erase_state;

  BOOT_LOG_INF("ID:%" PRIu8 " erased_val:0x%" PRIx8, fa->fa_id, erased_val);

  return erased_val;
}

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
                           struct flash_sector *sectors)
{
  size_t off;
  uint32_t total_count = 0;
  struct flash_device_s *dev = lookup_flash_device_by_id(fa_id);
  const size_t sector_size = dev->mtdgeo.erasesize;
  const struct flash_area *fa = fa = dev->fa_cfg;

  for (off = 0; off < fa->fa_size; off += sector_size)
    {
      /* Note: Offset here is relative to flash area, not device */

      sectors[total_count].fs_off = off;
      sectors[total_count].fs_size = sector_size;
      total_count++;
    }

  *count = total_count;

  DEBUGASSERT(total_count == dev->mtdgeo.neraseblocks);

  BOOT_LOG_INF("ID:%d count:%" PRIu32, fa_id, *count);

  return OK;
}

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

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
  BOOT_LOG_INF("image_index:%d slot:%d", image_index, slot);

  switch (slot)
    {
      case 0:
        return FLASH_AREA_IMAGE_PRIMARY(image_index);
      case 1:
        return FLASH_AREA_IMAGE_SECONDARY(image_index);
    }

  BOOT_LOG_ERR("Unexpected Request: image_index:%d, slot:%d",
               image_index, slot);

  return ERROR; /* flash_area_open will fail on that */
}

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

int flash_area_id_from_image_slot(int slot)
{
  BOOT_LOG_INF("slot:%d", slot);

  return flash_area_id_from_multi_image_slot(0, slot);
}

/****************************************************************************
 * Name: flash_area_id_to_multi_image_slot
 *
 * Description:
 *   Convert the specified flash area ID and image index (in case of a
 *   multi-image setup) to an image slot index.
 *
 * Input Parameters:
 *   image_index - Index of the image.
 *   fa_id       - Image slot, which may be 0 (primary) or 1 (secondary).
 *
 * Returned Value:
 *   Image slot index (0 or 1), or negative value in case ID doesn't
 *   correspond to an image slot.
 *
 ****************************************************************************/

int flash_area_id_to_multi_image_slot(int image_index, int fa_id)
{
  BOOT_LOG_INF("image_index:%d fa_id:%d", image_index, fa_id);

  if (fa_id == FLASH_AREA_IMAGE_PRIMARY(image_index))
    {
      return 0;
    }

  if (fa_id == FLASH_AREA_IMAGE_SECONDARY(image_index))
    {
      return 1;
    }

  BOOT_LOG_ERR("Unexpected Request: image_index:%d, fa_id:%d",
               image_index, fa_id);

  return ERROR; /* flash_area_open will fail on that */
}

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
 *   Flash area ID (0 or 1), or negative value in case the requested slot
 *   is invalid.
 *
 ****************************************************************************/

int flash_area_id_from_image_offset(uint32_t offset)
{
  struct flash_device_s *dev = lookup_flash_device_by_offset(offset);

  BOOT_LOG_INF("offset:%" PRIu32, offset);

  return dev->fa_cfg->fa_id;
}
