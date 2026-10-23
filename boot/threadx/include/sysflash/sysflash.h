/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#ifndef H_SYSFLASH_H_
#define H_SYSFLASH_H_

#define FLASH_AREA_BOOTLOADER          0
#define FLASH_AREA_PRIMARY_ID          1
#define FLASH_AREA_SECONDARY_ID        2
#define FLASH_AREA_SCRATCH_ID          3

#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ? FLASH_AREA_PRIMARY_ID : FLASH_AREA_PRIMARY_ID)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ? FLASH_AREA_SECONDARY_ID : FLASH_AREA_SECONDARY_ID)
#define FLASH_AREA_IMAGE_SCRATCH       FLASH_AREA_SCRATCH_ID

#endif /* H_SYSFLASH_H_ */
