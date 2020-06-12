/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
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
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 /*******************************************************************************/
#ifdef MCUBOOT_HAVE_ASSERT_H
#include "mcuboot_config/mcuboot_assert.h"
#else
#include <assert.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "mcuboot_config/mcuboot_config.h"

#include "flash_map_backend/flash_map_backend.h"
#include <sysflash/sysflash.h>

#include "bootutil/bootutil_log.h"

#include "cy_pdl.h"

#define CY_BOOT_INTERNAL_FLASH_ERASE_VALUE      (0x00)

/*
* Writes `len` bytes of flash memory at `off` from the buffer at `src`
 */
int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len)
{
    cy_en_flashdrv_status_t rc = CY_FLASH_DRV_SUCCESS;
    size_t write_start_addr;
    
    assert(off < fa->fa_off);
    assert(off + len < fa->fa_off);

    /* convert to absolute address inside a device */
    write_start_addr = fa->fa_off + off;

    if (fa->fa_device_id == FLASH_DEVICE_INTERNAL_FLASH)
    {
        uint8_t writeBuffer[CY_FLASH_SIZEOF_ROW];
        uint32_t rowId;
        uint32_t dstIndex;
        uint32_t srcIndex = 0u;
        uint32_t eeOffset;
        uint32_t byteOffset;
        uint32_t rowsNotEqual;
        uint8_t *dataPtr = (uint8_t *)src;
            /* get an absolute address first */
        eeOffset = write_start_addr;

        rowId = eeOffset / CY_FLASH_SIZEOF_ROW;
        byteOffset = CY_FLASH_SIZEOF_ROW * rowId;

        while((srcIndex < len) && (rc == CY_FLASH_DRV_SUCCESS))
        {
            rowsNotEqual = 0u;
            /* Copy data to the write buffer either from the source buffer or from the flash */
            for(dstIndex = 0u; dstIndex < CY_FLASH_SIZEOF_ROW; dstIndex++)
            {
                if((byteOffset >= eeOffset) && (srcIndex < len))
                {
                    writeBuffer[dstIndex] = dataPtr[srcIndex];
                    /* Detect that row programming is required */
                    if(rowsNotEqual == 0u)
                    {
                        rowsNotEqual = 1u;
                    }
                    srcIndex++;
                }
                else
                {   /* fill in buffer w erased value, so row contents will not be effected by row Program */
                    writeBuffer[dstIndex] = CY_BOOT_INTERNAL_FLASH_ERASE_VALUE;
                }
                byteOffset++;
            }
            if(rowsNotEqual != 0u)
            {
                /* Program flash row */
                rc = Cy_Flash_ProgramRow((rowId * CY_FLASH_SIZEOF_ROW), (const uint32_t *)writeBuffer);
            }
            /* Go to the next row */
            rowId++;
        }
    }
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    else if ((fa->fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) == FLASH_DEVICE_EXTERNAL_FLAG)
    {
        rc = psoc6_smif_write(fa, write_start_addr, src, len);
    }
#endif
    else
    {
        /* incorrect/non-existing flash device id */
        rc = -1;
    }
    return (int) rc;
}
