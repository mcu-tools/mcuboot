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

#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "flash_map/flash_map.h"
#include "os/os.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil_priv.h"

#define MCUBOOT_VERSION(maj, min, api)   (((maj) << 16) + ((min) << 8) + (api))

struct mcuboot_api_flash_info {
    /* inputs */
    uint8_t  index;
    /* outputs */
    uint8_t  id;
    uint32_t offset;
    uint32_t size;
};

static int
_flash_map_size(void *arg)
{
    uint32_t *amount = (uint32_t *) arg;
    *amount = sizeof sysflash_map_dflt / sizeof sysflash_map_dflt[0];
    return 0;
}

static int
_flash_map_info(void *arg)
{
    struct mcuboot_api_flash_info *info = (struct mcuboot_api_flash_info *) arg;

    if (info->index >= sizeof sysflash_map_dflt / sizeof sysflash_map_dflt[0]) {
        return -1;
    }

    info->id     = sysflash_map_dflt[info->index].fa_id;
    info->offset = sysflash_map_dflt[info->index].fa_off;
    info->size   = sysflash_map_dflt[info->index].fa_size;
    return 0;
}

static int
_mcuboot_ioctl(int req, void *arg)
{
    switch (req) {
    case MCUBOOT_REQ_FLASH_MAP_SIZE:
        return _flash_map_size(arg);
    case MCUBOOT_REQ_FLASH_MAP_INFO:
        return _flash_map_info(arg);
    /* just for health checking... */
    case 0x1234:
        return 0x5678;
    default:
        return -1;
    }
}

__attribute__((section(".text")))
const struct mcuboot_api_itf mcuboot_api_vt = {
    .mcuboot_api_magic  = MCUBOOT_API_MAGIC,
    .mcuboot_version    = MCUBOOT_VERSION(0, 9, 1),
    .mcuboot_ioctl      = _mcuboot_ioctl,
};

struct mcuboot_api_itf *p_mcuboot_api_vt;
