/*
 * Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <flash_map_backend/flash_map_backend.h>
#include <zephyr/storage/flash_map.h>

#include "aardvark_i2c_flash.h"

static int curr_idx = -1;

static uint8_t known_ids[] = {
	-1, /* Just to show a "failing" image. Next one should work */
	AARDVARK_FLASH_AREA_ID
};

bool
flash_map_id_get_next(uint8_t *id, bool reset)
{
	if (reset) {
		curr_idx = 0;
	} else {
		curr_idx++;
	}

	if (curr_idx >= ARRAY_SIZE(known_ids)) {
		return false;
	}

	*id = known_ids[curr_idx];

	return true;
}

bool
flash_map_id_get_current(uint8_t *id)
{
	if (curr_idx == -1 || curr_idx >= ARRAY_SIZE(known_ids)) {
		return false;
	}

	*id = known_ids[curr_idx];

	return true;
}

int
flash_area_open_custom(uint8_t id, const struct flash_area **fap)
{
	switch (id) {
	case 0:
		return flash_area_open(id, fap);
	case AARDVARK_FLASH_AREA_ID:
		return aardvark_flash_area_open(id, fap);
	default:
		return -1;
	}
}
