/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mcuboot_config/mcuboot_config.h>

//! A user-defined identifier for different storage mediums
//! (i.e internal flash, external NOR flash, eMMC, etc)
#define FLASH_DEVICE_INTERNAL_FLASH 0

//! An arbitrarily high slot ID we will use to indicate that
//! there is not slot
#define FLASH_SLOT_DOES_NOT_EXIST 255

//! The slot we will use to track the bootloader allocation
#define FLASH_AREA_BOOTLOADER 0

#define FLASH_AREA_IMAGE_0_PRIMARY 1
#define FLASH_AREA_IMAGE_0_SECONDARY 2
#define FLASH_AREA_IMAGE_SCRATCH 3
#define FLASH_AREA_IMAGE_1_PRIMARY 4
#define FLASH_AREA_IMAGE_1_SECONDARY 5

#if (MCUBOOT_IMAGE_NUMBER == 1)
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_PRIMARY : \
                                         FLASH_SLOT_DOES_NOT_EXIST)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_SECONDARY : \
                                         FLASH_SLOT_DOES_NOT_EXIST)

#elif (MCUBOOT_IMAGE_NUMBER == 2)
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_PRIMARY : \
                                        ((x) == 1) ?          \
                                         FLASH_AREA_IMAGE_1_PRIMARY : \
                                         FLASH_SLOT_DOES_NOT_EXIST)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?          \
                                         FLASH_AREA_IMAGE_0_SECONDARY : \
                                        ((x) == 1) ?          \
                                         FLASH_AREA_IMAGE_1_SECONDARY : \
                                         FLASH_SLOT_DOES_NOT_EXIST)

#else
#warning "Image slot and flash area mapping is not defined"
#endif
