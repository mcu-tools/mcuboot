/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2025 Cypress Semiconductor Corporation
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
 *  www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*******************************************************************************/

#ifndef FLASH_MAP_BACKEND_PLATFORM_H
#define FLASH_MAP_BACKEND_PLATFORM_H

#include <assert.h>

#include "cy_flash.h"
#include "flash_map_backend/flash_map_backend.h"
#include "memorymap.h"

static inline const struct flash_area_interface* flash_area_get_api(uint8_t fd_id)
{
#if defined(USE_INTERNAL_FLASH_CODE_LARGE) || defined(USE_INTERNAL_FLASH_CODE_SMALL)
    extern const struct flash_area_interface internal_mem_interface;
#endif /* defined(USE_INTERNAL_FLASH_CODE_LARGE) || defined(USE_INTERNAL_FLASH_CODE_SMALL) */

#if defined(USE_INTERNAL_FLASH_WORK_LARGE) || defined(USE_INTERNAL_FLASH_WORK_SMALL)
    extern const struct flash_area_interface internal_mem_eeprom_interface;
#endif /* defined(USE_INTERNAL_FLASH_WORK_LARGE) || defined(USE_INTERNAL_FLASH_WORK_SMALL) */

    const struct flash_area_interface* interface = NULL;

    switch (fd_id) {
#if defined(USE_INTERNAL_FLASH_CODE_LARGE)
        case INTERNAL_FLASH_CODE_LARGE:
            interface = &internal_mem_interface;
            break;
#endif /* defined(USE_INTERNAL_FLASH_CODE_LARGE) */

#if defined(USE_INTERNAL_FLASH_CODE_SMALL)
        case INTERNAL_FLASH_CODE_SMALL:
            interface = &internal_mem_interface;
            break;
#endif /* defined(USE_INTERNAL_FLASH_CODE_SMALL) */

#if defined(USE_INTERNAL_FLASH_WORK_LARGE)
        case INTERNAL_FLASH_WORK_LARGE:
            interface = &internal_mem_eeprom_interface;
            break;
#endif /* defined(USE_INTERNAL_FLASH_WORK_LARGE) */

#if defined(USE_INTERNAL_FLASH_WORK_SMALL)
        case INTERNAL_FLASH_WORK_SMALL:
            interface = &internal_mem_eeprom_interface;
            break;
#endif /* defined(USE_INTERNAL_FLASH_WORK_SMALL) */

        default:
            assert(false);
            interface = NULL;
            break;
    }

    return interface;
}

#endif /* FLASH_MAP_BACKEND_PLATFORM_H */
