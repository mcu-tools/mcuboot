/*
 * Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <flash_map_backend/flash_map_backend.h>
#include <zephyr/storage/flash_map.h>

#define AARDVARK_FLASH_AREA_ID (0 | FLASH_MAP_CUSTOM_BACKEND_MASK)

int aardvark_flash_area_open(uint8_t id, const struct flash_area **fap);
