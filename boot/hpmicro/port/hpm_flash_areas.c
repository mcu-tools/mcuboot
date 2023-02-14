/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include "hpm_debug_console.h"
#include <sys/types.h>
#include "board.h"
#include "hpm_romapi_xpi_nor_def.h"
#include "hpm_romapi.h"
#include "flash_map.h"
#include "errno1.h"
#include "flash.h"
#include "../include/sysflash/sysflash.h"

#define BOOTLOADER_START_ADDRESS BOOTLOADER_OFFSET_IN_FLASH
#define BOOTLOADER_SIZE CONFIG_BOOTLOADER_SIZE
extern struct device hpm_flash_controller;

#ifdef BUILD_TYPE_MCUBOOTAPP
__attribute__ ((section(".mcuboot_app_header"))) const uint8_t app_head[0x200] = {0x00};
#endif
const struct flash_area default_flash_map[] = {
    {
        .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
        .fa_device_id = 0,
        .pad16 = 1,
        .fa_off = IMAGE0_PRIMARY_START_ADDRESS,
        .fa_size = APPLICATION_SIZE,
        .fa_dev = &hpm_flash_controller,
    },
    {
        .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
        .fa_device_id = 0,
        .pad16 = 1,
        .fa_off = IMAGE0_SECONDARY_START_ADDRESS,
        .fa_size = APPLICATION_SIZE,
        .fa_dev = &hpm_flash_controller,
    },
    {
        .fa_id = FLASH_AREA_IMAGE_SCRATCH,
        .fa_device_id = 0,
        .pad16 = 1,
        .fa_off = SCRATCH_OFFSET,
        .fa_size = SCRATCH_SIZE,
        .fa_dev = &hpm_flash_controller,
    },
};

const int flash_map_entries = ARRAY_SIZE(default_flash_map);
const struct flash_area *flash_map = &default_flash_map[0];

static inline struct flash_area const *get_flash_area_from_id(int idx)
{
	for (int i = 0; i < flash_map_entries; i++) {
		if (flash_map[i].fa_id == idx) {
			return &flash_map[i];
		}
	}

	return NULL;
}


static inline bool is_in_flash_area_bounds(const struct flash_area *fa,
					   off_t off, size_t len)
{
	return (off >= 0) && ((off + len) <= fa->fa_size);
}


/**
 * @brief Retrieve partitions flash area from the flash_map.
 *
 * Function Retrieves flash_area from flash_map for given partition.
 *
 * @param[in]  id ID of the flash partition.
 * @param[out] fa Pointer which has to reference flash_area. If
 * @p ID is unknown, it will be NULL on output.
 *
 * @return  0 on success, -EACCES if the flash_map is not available ,
 * -ENOENT if @p ID is unknown, -ENODEV if there is no driver attached
 * to the area.
 */
int flash_area_open(uint8_t id, const struct flash_area **fap)
{
	const struct flash_area *area;

	if (flash_map == NULL) {
		return -EACCES;
	}

	area = get_flash_area_from_id(id);
	if (area == NULL) {
		return -ENOENT;
	}

	if (!area->fa_dev) {
		return -ENODEV;
	}

	*fap = area;

	return 0;

}

/**
 * @brief Close flash_area
 *
 * Reserved for future usage and external projects compatibility reason.
 * Currently is NOP.
 *
 * @param[in] fa Flash area to be closed.
 */
void flash_area_close(const struct flash_area *fa)
{
	/* nothing to do for now */
}

/**
 * @brief Read flash area data
 *
 * Read data from flash area. Area readout boundaries are asserted before read
 * request. API has the same limitation regard read-block alignment and size
 * as wrapped flash driver.
 *
 * @param[in]  fa  Flash area
 * @param[in]  off Offset relative from beginning of flash area to read
 * @param[out] dst Buffer to store read data
 * @param[in]  len Number of bytes to read
 *
 * @return  0 on success, negative errno code on fail.
 */
int flash_area_read(const struct flash_area *fa, off_t off, void *dst,
		    size_t len)
{
	if (!is_in_flash_area_bounds(fa, off, len)) {
		return -EINVAL;
	}
    const struct device *dev = fa->fa_dev;

    const struct flash_driver_api *api = dev->api;
	return api->read(fa->fa_dev, fa->fa_off + off, dst, len);

}

/**
 * @brief Write data to flash area
 *
 * Write data to flash area. Area write boundaries are asserted before write
 * request. API has the same limitation regard write-block alignment and size
 * as wrapped flash driver.
 *
 * @param[in]  fa  Flash area
 * @param[in]  off Offset relative from beginning of flash area to read
 * @param[out] src Buffer with data to be written
 * @param[in]  len Number of bytes to write
 *
 * @return  0 on success, negative errno code on fail.
 */
int flash_area_write(const struct flash_area *fa, off_t off, const void *src,
		     size_t len)
{
	if (!is_in_flash_area_bounds(fa, off, len)) {
		return -EINVAL;
	}
    const struct device *dev = fa->fa_dev;

    const struct flash_driver_api *api = dev->api;
	return api->write(fa->fa_dev, fa->fa_off + off, src, len);
}

/**
 * @brief Erase flash area
 *
 * Erase given flash area range. Area boundaries are asserted before erase
 * request. API has the same limitation regard erase-block alignment and size
 * as wrapped flash driver.
 *
 * @param[in] fa  Flash area
 * @param[in] off Offset relative from beginning of flash area.
 * @param[in] len Number of bytes to be erase
 *
 * @return  0 on success, negative errno code on fail.
 */
int flash_area_erase(const struct flash_area *fa, off_t off, size_t len)
{
	if (!is_in_flash_area_bounds(fa, off, len)) {
		return -EINVAL;
	}
    const struct device *dev = fa->fa_dev;

    const struct flash_driver_api *api = dev->api;
	return api->erase(fa->fa_dev, fa->fa_off + off, len);

}

/**
 * @brief Get write block size of the flash area
 *
 * Currently write block size might be treated as read block size, although
 * most of drivers supports unaligned readout.
 *
 * @param[in] fa Flash area
 *
 * @return Alignment restriction for flash writes in [B].
 */
uint32_t flash_area_align(const struct flash_area *fa)
{
    const struct device *dev = fa->fa_dev;
    const struct flash_driver_api *api = dev->api;
	return api->get_parameters(dev)->write_block_size;

}

