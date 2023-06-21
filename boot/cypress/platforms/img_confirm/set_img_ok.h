/********************************************************************************
* Copyright 2021 Infineon Technologies AG
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/

#if !defined(SET_IMG_OK_H)
#define SET_IMG_OK_H

#include "cy_flash.h"
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829)
#include "flash_qspi.h"
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829) */
#include <string.h>

#define FLASH_ROW_BUF_SZ        MEMORY_ALIGN
#define IMG_TRAILER_SZ          MEMORY_ALIGN

#define USER_SWAP_IMAGE_OK_OFFS (24)
#define USER_SWAP_IMAGE_OK      (1)
#define IMG_OK_ADDR             (PRIMARY_IMG_START + USER_APP_SIZE - USER_SWAP_IMAGE_OK_OFFS)

#define IMG_OK_SET_FAILED       -1
#define IMG_OK_ALREADY_SET      1
#define IMG_OK_SET_SUCCESS      0

int set_img_ok(uint32_t address, uint8_t value);

#endif /* SET_IMG_OK_H */
