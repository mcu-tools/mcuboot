/*
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 * Copyright (c) 2022 Infineon Technologies AG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef SYSFLASH_H
#define SYSFLASH_H

#include <stdint.h>
#include "cy_syslib.h"
#include "memorymap.h"

#ifndef MCUBOOT_IMAGE_NUMBER
#ifdef MCUBootApp
#warning Undefined MCUBOOT_IMAGE_NUMBER. Assuming 1 (single-image).
#endif /* MCUBootApp */
#define MCUBOOT_IMAGE_NUMBER                 1u
#endif /* MCUBOOT_IMAGE_NUMBER */

#define FLASH_AREA_ERROR                    255u  /* Invalid flash area */

#define SLOTS_FOR_IMAGE                     ( 2u)

#if (MCUBOOT_IMAGE_NUMBER < 1 || MCUBOOT_IMAGE_NUMBER > 16)
#error Unsupported MCUBOOT_IMAGE_NUMBER. Set it to between 1 and 16.
#endif /* (MCUBOOT_IMAGE_NUMBER < 1 || MCUBOOT_IMAGE_NUMBER > 16) */

#define BOOT_MAX_SWAP_STATUS_SECTORS        (64)

__STATIC_INLINE uint8_t FLASH_AREA_IMAGE_PRIMARY(uint32_t img_idx)
{
#if defined(MEMORYMAP_GENERATED_AREAS)
    return memory_areas_primary[img_idx];
#else
    uint32_t flash_area_id = FLASH_AREA_ERROR;

    switch (img_idx)
    {
        case 0:
        {
            flash_area_id = FLASH_AREA_IMG_1_PRIMARY;
            break;
        }

        case 1:
        {
            /* (img_idx + 1) because img_idx starts from 0, _IMAGE_NUMBER from 1 */
            flash_area_id = SLOTS_FOR_IMAGE * (img_idx + 1u);
            break;
        }

        default:
        {
            /* 7 now is used for FLASH_AREA_IMAGE_SWAP_STATUS */
            if (img_idx < (uint32_t)MCUBOOT_IMAGE_NUMBER)
            {
                flash_area_id = SLOTS_FOR_IMAGE * (img_idx + 1u) + 2u;
            }
            break;
        }
    }

    if (flash_area_id > (unsigned)UINT8_MAX)
    {
        flash_area_id = FLASH_AREA_ERROR;
    }

    return (uint8_t)flash_area_id;
#endif
}

__STATIC_INLINE uint8_t FLASH_AREA_IMAGE_SECONDARY(uint32_t img_idx)
{
#if defined(MEMORYMAP_GENERATED_AREAS)
    return memory_areas_secondary[img_idx];
#else
    uint32_t flash_area_id = FLASH_AREA_ERROR;

    switch (img_idx)
    {
        case 0:
        {
            flash_area_id = FLASH_AREA_IMG_1_SECONDARY;
            break;
        }

        case 1:
        {
            /* (img_idx + 1) because img_idx starts from 0, _IMAGE_NUMBER from 1 */
            flash_area_id = SLOTS_FOR_IMAGE * (img_idx + 1u) + 1u;
            break;
        }

        default:
        {
            if (img_idx < (uint32_t)MCUBOOT_IMAGE_NUMBER)
            {
                flash_area_id = SLOTS_FOR_IMAGE * (img_idx + 1u) + 3u;
            }
            break;
        }
    }

    if (flash_area_id > (unsigned)UINT8_MAX)
    {
        flash_area_id = FLASH_AREA_ERROR;
    }

    return (uint8_t)flash_area_id;
#endif
}

#endif /* SYSFLASH_H */
