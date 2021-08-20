/****************************************************************************
 * boot/nuttx/include/sysflash/sysflash.h
 *
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
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
 *
 ****************************************************************************/

#ifndef __BOOT_NUTTX_INCLUDE_SYSFLASH_SYSFLASH_H
#define __BOOT_NUTTX_INCLUDE_SYSFLASH_SYSFLASH_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PRIMARY_ID      0
#define SECONDARY_ID    1
#define SCRATCH_ID      2

#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?        \
                                         PRIMARY_ID :       \
                                         PRIMARY_ID)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?        \
                                         SECONDARY_ID :     \
                                         SECONDARY_ID)
#define FLASH_AREA_IMAGE_SCRATCH       SCRATCH_ID

#endif /* __BOOT_NUTTX_INCLUDE_SYSFLASH_SYSFLASH_H */
