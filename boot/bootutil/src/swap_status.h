/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2020 Arm Limited
 * Copyright (c) 2020 Cypress Semiconductors
 *
 * Original license:
 *
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

#ifndef BOOT_BOOTUTIL_SRC_SWAP_STATUS_H_
#define BOOT_BOOTUTIL_SRC_SWAP_STATUS_H_

#include <stdint.h>
#include "sysflash/sysflash.h"
#include "bootutil_priv.h"

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SWAP_USING_STATUS

extern const uint32_t stat_part_magic[1];

#define BOOT_SWAP_STATUS_MAGIC       (0xDEADBEAF)

#define BOOT_SWAP_STATUS_ENCK1_SZ       16UL
#define BOOT_SWAP_STATUS_ENCK2_SZ       16UL

struct image_status_trailer {
    uint8_t enc_key1[BOOT_SWAP_STATUS_ENCK1_SZ];
    uint8_t enc_key2[BOOT_SWAP_STATUS_ENCK2_SZ];
    uint32_t swap_size;
    uint8_t swap_type;
    uint8_t copy_done;
    uint8_t image_ok;
    uint8_t magic[BOOT_MAGIC_SZ];
};

#define BOOT_SWAP_STATUS_SWAPSZ_SZ      4UL
#define BOOT_SWAP_STATUS_SWAPINF_SZ     1UL
#define BOOT_SWAP_STATUS_COPY_DONE_SZ   1UL
#define BOOT_SWAP_STATUS_IMG_OK_SZ      1UL

#define BOOT_SWAP_STATUS_MAGIC_SZ       BOOT_MAGIC_SZ

#define BOOT_SWAP_STATUS_MGCREC_SZ      4UL
#define BOOT_SWAP_STATUS_CNT_SZ         4UL
#define BOOT_SWAP_STATUS_CRC_SZ         4UL

#define BOOT_SWAP_STATUS_ROW_SZ         CY_FLASH_ALIGN

/* agreed to name it "a record" */
#define BOOT_SWAP_STATUS_PAYLD_SZ       (BOOT_SWAP_STATUS_ROW_SZ -\
                                            BOOT_SWAP_STATUS_MGCREC_SZ - \
                                            BOOT_SWAP_STATUS_CNT_SZ - \
                                            BOOT_SWAP_STATUS_CRC_SZ)
#define BOOT_SWAP_STATUS_ROW_SZ_MIN     16UL

/* INFO: defining record structure for better understanding */
struct status_part_record{
    uint8_t payload[BOOT_SWAP_STATUS_PAYLD_SZ];
    uint8_t magic[BOOT_SWAP_STATUS_MGCREC_SZ];
    uint8_t counter[BOOT_SWAP_STATUS_CNT_SZ];
    uint8_t crc[BOOT_SWAP_STATUS_CRC_SZ];
};

#if (BOOT_SWAP_STATUS_ROW_SZ % BOOT_SWAP_STATUS_ROW_SZ_MIN != 0)
    #error "BOOT_SWAP_STATUS_ROW_SZ size is less then min value of 16 bytes"
#endif

/* number of rows sector-status area should fit into */
#define BOOT_SWAP_STATUS_SECT_ROWS_NUM  (((BOOT_MAX_IMG_SECTORS-1)/BOOT_SWAP_STATUS_PAYLD_SZ)+1)

/*
    Number of flash rows used to store swap info. It consists
    of following fields. 16 bytes is a minimum required row size,
    thus 64 bytes required at minimum size of swap info size.

    16 bytes - uint8_t enc_key1[BOOT_SWAP_STATUS_ENCK1_SZ];
    16 bytes - uint8_t enc_key2[BOOT_SWAP_STATUS_ENCK2_SZ];
    4 bytes - uint32_t swap_size;
    1 byte - uint8_t swap_type;
    1 byte - uint8_t copy_done;
    1 byte - uint8_t image_ok;
    16 bytes -  uint8_t magic[BOOT_MAGIC_SZ];
    = 55 bytes
 */
#define BOOT_SWAP_STATUS_TRAILER_SIZE 64UL
// TODO: check if min write size is 64 or larger
// TODO: small-magic, coutner and crc aren't coutned here

/* number of rows trailer data should fit into */
#define BOOT_SWAP_STATUS_TRAIL_ROWS_NUM  (((BOOT_SWAP_STATUS_TRAILER_SIZE-1)/BOOT_SWAP_STATUS_PAYLD_SZ)+1)

/* the size of one copy of status area */
#define BOOT_SWAP_STATUS_D_SIZE     (BOOT_SWAP_STATUS_ROW_SZ * \
                                    (BOOT_SWAP_STATUS_SECT_ROWS_NUM + \
                                    BOOT_SWAP_STATUS_TRAIL_ROWS_NUM))

/* the size of one copy of status area without cnt and crc fields */
#define BOOT_SWAP_STATUS_D_SIZE_RAW (BOOT_SWAP_STATUS_PAYLD_SZ * \
                                    (BOOT_SWAP_STATUS_SECT_ROWS_NUM + \
                                    BOOT_SWAP_STATUS_TRAIL_ROWS_NUM))

/* multiplier which defines how many blocks will be used to reduce Flash wear
 * 1 is for single write wear, 2 - twice less wear, 3 - three times less wear, etc */
#define BOOT_SWAP_STATUS_MULT       2

#define BOOT_SWAP_STATUS_SIZE       (BOOT_SWAP_STATUS_MULT * BOOT_SWAP_STATUS_D_SIZE)

#define BOOT_SWAP_STATUS_SZ_PRIM    BOOT_SWAP_STATUS_SIZE
#define BOOT_SWAP_STATUS_SZ_SEC     BOOT_SWAP_STATUS_SIZE

#define BOOT_SWAP_STATUS_OFFS_PRIM  0UL
#define BOOT_SWAP_STATUS_OFFS_SEC   (BOOT_SWAP_STATUS_OFFS_PRIM + \
                                    BOOT_SWAP_STATUS_SZ_PRIM)

int32_t swap_status_init_offset(uint32_t area_id);
int swap_status_update(uint32_t target_area_id, uint32_t offs, const void *data, uint32_t len);
int swap_status_retrieve(uint32_t target_area_id, uint32_t offs, void *data, uint32_t len);

int boot_write_trailer(const struct flash_area *fap, uint32_t off,
                        const uint8_t *inbuf, uint8_t inlen);

#endif /* MCUBOOT_SWAP_USING_STATUS */

#endif /* BOOT_BOOTUTIL_SRC_SWAP_STATUS_H_ */
