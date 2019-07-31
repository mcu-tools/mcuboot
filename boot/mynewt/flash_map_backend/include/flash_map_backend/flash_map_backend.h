/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __FLASH_MAP_BACKEND_H__
#define __FLASH_MAP_BACKEND_H__

#include <flash_map/flash_map.h>

#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0 : \
                                         FLASH_AREA_IMAGE_0)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_1 : \
                                         FLASH_AREA_IMAGE_1)

int flash_area_id_from_multi_image_slot(int image_index, int slot);
int flash_area_id_to_multi_image_slot(int image_index, int area_id);

#endif /* __FLASH_MAP_BACKEND_H__ */
