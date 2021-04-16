/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* This file mocks zephyr's flash map / DT macro */

#ifndef __DEVICETREE_H__
#define __DEVICETREE_H__

#define FLASH_AREA_ID(x) FLASH_AREA_ID_##x

#define FLASH_AREA_ID_image_0 1
#define FLASH_AREA_ID_image_1 2
#define FLASH_AREA_ID_image_scratch 3
#define FLASH_AREA_ID_image_2 4
#define FLASH_AREA_ID_image_3 5

#define FLASH_AREA_IMAGE_0     FLASH_AREA_ID(image_0)
#define FLASH_AREA_IMAGE_1     FLASH_AREA_ID(image_1)
#define FLASH_AREA_IMAGE_2     FLASH_AREA_ID(image_2)
#define FLASH_AREA_IMAGE_3     FLASH_AREA_ID(image_3)

#define FLASH_AREA_IMAGE_SWAP_STATUS FLASH_AREA_ID(image_scratch)

#endif /*__DEVICETREE_H__*/
