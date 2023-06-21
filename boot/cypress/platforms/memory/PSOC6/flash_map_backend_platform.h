/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2020 Cypress Semiconductor Corporation
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

#include "flash_map_backend/flash_map_backend.h"

#include <assert.h>

#define FLASH_DEVICE_INDEX_MASK                 (0x7Fu)
#define FLASH_DEVICE_GET_EXT_INDEX(n)           ((n) & FLASH_DEVICE_INDEX_MASK)
#define FLASH_DEVICE_UNDEFINED                  (0x00u)
#define FLASH_DEVICE_EXTERNAL_FLAG              (0x80u)
#define FLASH_DEVICE_INTERNAL_FLASH             (0x7Fu)
#define FLASH_DEVICE_EXTERNAL_FLASH(index)      (FLASH_DEVICE_EXTERNAL_FLAG | index)

#ifndef CY_BOOT_EXTERNAL_DEVICE_INDEX
/* assume first(one) SMIF device is used */
#define CY_BOOT_EXTERNAL_DEVICE_INDEX           (0u)
#endif

#ifndef INTERNAL_MEMORY_ERASE_VALUE_PLATFORM
/* This is the value of internal flash bytes after an erase */
#define INTERNAL_MEMORY_ERASE_VALUE_PLATFORM  (0x00u)
#endif /* INTERNAL_MEMORY_ERASE_VALUE_PLATFORM */

#ifndef INTERNAL_MEMORY_ERASE_SIZE_PLATFORM
#define INTERNAL_MEMORY_ERASE_SIZE_PLATFORM     (0x200u)
#endif /* INTERNAL_MEMORY_ERASE_SIZE_PLATFORM */

#if defined CY_BOOT_USE_EXTERNAL_FLASH

#ifndef EXTERNAL_MEMORY_ERASE_VALUE_PLATFORM
#define EXTERNAL_MEMORY_ERASE_VALUE_PLATFORM     (0xFFu)
#endif /* EXTERNAL_MEMORY_ERASE_VALUE_PLATFORM */

#ifndef EXTERNAL_MEMORY_ERASE_SIZE_PLATFORM
#define EXTERNAL_MEMORY_ERASE_SIZE_PLATFORM     (0x40000u)
#endif /* EXTERNAL_MEMORY_ERASE_SIZE_PLATFORM */

#ifndef SMIF_MEM_START_PLATFORM
#define SMIF_MEM_START_PLATFORM                 (CY_XIP_BASE)
#endif /* SMIF_MEM_START_PLATFORM */

#endif /* CY_BOOT_USE_EXTERNAL_FLASH */

static inline const struct flash_area_interface* flash_area_get_api(uint8_t fd_id)
{
    if (FLASH_DEVICE_INTERNAL_FLASH == fd_id) {
        extern const struct flash_area_interface internal_mem_interface;
        return &internal_mem_interface;
    }

#if defined CY_BOOT_USE_EXTERNAL_FLASH
    if ((fd_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG) {
        extern const struct flash_area_interface external_mem_interface;
        return &external_mem_interface;
    }
#endif

    assert(false);
    return NULL;
}

#endif /* FLASH_MAP_BACKEND_PLATFORM_H */
