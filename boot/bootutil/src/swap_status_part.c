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

#include <assert.h>
#include "crc32c.h"
#include <string.h>
#include "swap_status.h"

#ifdef MCUBOOT_SWAP_USING_STATUS

const uint32_t stat_part_magic[] = {
    BOOT_SWAP_STATUS_MAGIC
};

uint32_t calc_rec_idx(uint32_t value)
{
    uint32_t rec_idx;

    rec_idx = value/BOOT_SWAP_STATUS_PAYLD_SZ;

    return rec_idx;
}

uint32_t calc_record_offs(uint32_t offs)
{
    uint32_t rec_offs;

    rec_offs = BOOT_SWAP_STATUS_ROW_SZ*calc_rec_idx(offs);

    return rec_offs;
}

uint32_t calc_record_crc(const uint8_t *data, uint32_t length)
{
    uint32_t crc;

    crc = crc32c_checksum(data, length);

    return crc;
}

int32_t swap_status_init_offset(uint32_t area_id)
{
    int32_t offset;
    /* calculate an offset caused by area type: primary_x/secondary_x */
    switch (area_id) {
    case FLASH_AREA_IMAGE_0:
        offset = 0x00;
        break;
    case FLASH_AREA_IMAGE_1:
        offset = BOOT_SWAP_STATUS_SIZE;
        break;
#if (MCUBOOT_IMAGE_NUMBER == 2)
    case FLASH_AREA_IMAGE_2:
        offset = 2*BOOT_SWAP_STATUS_SIZE;
        break;
    case FLASH_AREA_IMAGE_3:
        offset = 3*BOOT_SWAP_STATUS_SIZE;
        break;
#endif
    default:
        offset = -1;
        break;
    }
    return offset;
}

int swap_status_read_record(uint32_t rec_offset, uint8_t *data, uint32_t *copy_counter)
{ /* returns BOOT_SWAP_STATUS_PAYLD_SZ of data */
    int rc = -1;

    uint32_t fin_offset, data_offset = 0;
    uint32_t counter, crc, magic;
    uint32_t crc_fail = 0;
    uint32_t magic_fail = 0;
    uint32_t max_cnt = 0;

    int32_t max_idx = 0;

    uint8_t buff[BOOT_SWAP_STATUS_ROW_SZ];

    const struct flash_area *fap_stat;

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    /* loop over copies/duplicates */
    for(uint32_t i = 0; i<BOOT_SWAP_STATUS_MULT; i++) {
        /* calculate final duplicate offset */
        fin_offset = rec_offset + i*BOOT_SWAP_STATUS_D_SIZE;

        rc = flash_area_read(fap_stat, fin_offset, buff, sizeof(buff));
        if (rc != 0) {
            return BOOT_EFLASH;
        }
        /* read magic value to know if area was pre-erased */
        magic = *((uint32_t *)&buff[BOOT_SWAP_STATUS_ROW_SZ -\
                                  BOOT_SWAP_STATUS_MGCREC_SZ -\
                                  BOOT_SWAP_STATUS_CNT_SZ-\
                                  BOOT_SWAP_STATUS_CRC_SZ]);
        if (magic == BOOT_SWAP_STATUS_MAGIC) {   /* read CRC */
            crc = *((uint32_t *)&buff[BOOT_SWAP_STATUS_ROW_SZ -\
                                      BOOT_SWAP_STATUS_CRC_SZ]);
            /* check record data integrity first */
            if (crc == calc_record_crc(buff, BOOT_SWAP_STATUS_ROW_SZ-BOOT_SWAP_STATUS_CRC_SZ)) {
                /* look for counter */
                counter = *((uint32_t *)&buff[BOOT_SWAP_STATUS_ROW_SZ -\
                                              BOOT_SWAP_STATUS_CNT_SZ - \
                                              BOOT_SWAP_STATUS_CRC_SZ]);
                /* find out counter max */
                if (counter >= max_cnt) {
                    max_cnt = counter;
                    max_idx = i;
                    data_offset = fin_offset;
                }
            }
            /* if crc != calculated() */
            else {
                crc_fail++;
            }
        }
        else {
            magic_fail++;
        }
    }
    /* no magic found - status area is pre-erased, start from scratch */
    if (magic_fail == BOOT_SWAP_STATUS_MULT) {
        /* emulate last index was received, so next will start from beginning */
        max_idx = BOOT_SWAP_STATUS_MULT-1;
        *copy_counter = 0;
        /* return all erased values */
        memset(data, flash_area_erased_val(fap_stat), BOOT_SWAP_STATUS_PAYLD_SZ);
    }
    else {
        /* no valid CRC found - status pre-read failure */
        if (crc_fail == BOOT_SWAP_STATUS_MULT)
        {
            max_idx = -1;
        }
        else {
            *copy_counter = max_cnt;
            /* read payload data */
            rc = flash_area_read(fap_stat, data_offset, data, BOOT_SWAP_STATUS_PAYLD_SZ);
            assert (rc == 0);
        }
    }
    flash_area_close(fap_stat);

    /* return back duplicate index */
    return max_idx;
}

static int swap_status_write_record(uint32_t rec_offset, uint32_t copy_num, uint32_t copy_counter, const uint8_t *data)
{ /* it receives explicitly BOOT_SWAP_STATUS_PAYLD_SZ of data */
    int rc = -1;

    uint32_t fin_offset;
    /* increment counter field */
    uint32_t next_counter = copy_counter+1;
    uint32_t next_crc;

    uint8_t buff[BOOT_SWAP_STATUS_ROW_SZ];

    const struct flash_area *fap_stat;

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    /* copy data into buffer */
    memcpy(buff, data, BOOT_SWAP_STATUS_PAYLD_SZ);
    /* append next counter to whole record row */
    memcpy(&buff[BOOT_SWAP_STATUS_ROW_SZ-BOOT_SWAP_STATUS_CNT_SZ-BOOT_SWAP_STATUS_CRC_SZ], \
            &next_counter, \
            BOOT_SWAP_STATUS_CNT_SZ);
    /* append record magic */
    memcpy(&buff[BOOT_SWAP_STATUS_ROW_SZ-\
                    BOOT_SWAP_STATUS_MGCREC_SZ-\
                    BOOT_SWAP_STATUS_CNT_SZ-\
                    BOOT_SWAP_STATUS_CRC_SZ], \
                    stat_part_magic, \
                    BOOT_SWAP_STATUS_MGCREC_SZ);

    /* calculate CRC field*/
    next_crc = calc_record_crc(buff, BOOT_SWAP_STATUS_ROW_SZ-BOOT_SWAP_STATUS_CRC_SZ);

    /* append new CRC to whole record row */
    memcpy(&buff[BOOT_SWAP_STATUS_ROW_SZ-BOOT_SWAP_STATUS_CRC_SZ], \
            &next_crc, \
            BOOT_SWAP_STATUS_CRC_SZ);

    /* we already know what copy number was last and correct */
    /* increment duplicate index */
    /* calculate final duplicate offset */
    if (copy_num == (BOOT_SWAP_STATUS_MULT-1)) {
        copy_num = 0;
    }
    else {
        copy_num++;
    }
    fin_offset = rec_offset + copy_num*BOOT_SWAP_STATUS_D_SIZE;

    /* write prepared record into flash */
    rc = flash_area_write(fap_stat, fin_offset, buff, sizeof(buff));
    assert (rc == 0);

    flash_area_close(fap_stat);

    return rc;
}

/**
 * Updates len bytes of status partition with values from *data-pointer.
 *
 * @param targ_area_id  Target area id for which status is being written.
 *                      Not a status-partition area id.
 * @param offset        Status byte offset inside status table. Should not include CRC and CNT.
 * @param data          Pointer to data status table to needs to be updated with.
 * @param len           Number of bytes to be written
 *
 * @return              0 on success; nonzero on failure.
 */
int swap_status_update(uint32_t targ_area_id, uint32_t offs, const void *data, uint32_t len)
{
    int rc = -1;

    int32_t init_offs;
    int32_t length = (int32_t)len;
    int32_t copy_num;

    uint32_t rec_offs;
    uint32_t copy_sz;
    uint32_t copy_counter;
    uint32_t data_idx = 0;
    uint32_t buff_idx = offs%BOOT_SWAP_STATUS_PAYLD_SZ;

    uint8_t buff[BOOT_SWAP_STATUS_PAYLD_SZ];

    /* check if end of data is still inside writable area */
    // TODO: update for multi image
    assert ((offs + len) <= BOOT_SWAP_STATUS_D_SIZE_RAW);

    /* pre-calculate sub-area offset */
    init_offs = swap_status_init_offset(targ_area_id);
    assert (init_offs >= 0);

    /* will start from it
     * this will be write-aligned */
    rec_offs = init_offs + calc_record_offs(offs);

    /* go over all records to be updated */
    while (length > 0) {
        /* preserve record */
        copy_num = swap_status_read_record(rec_offs, buff, &copy_counter);
        /* it returns copy number */
        if (copy_num < 0)
        {   /* something went wrong while read, exit */
            rc = -1;
            break;
        }
        /* update record data */
        if (length > (int)BOOT_SWAP_STATUS_PAYLD_SZ) {
            copy_sz = BOOT_SWAP_STATUS_PAYLD_SZ - buff_idx;
        }
        else {
            copy_sz = length;
        }
        memcpy((uint8_t *)buff+buff_idx, &((uint8_t *)data)[data_idx], copy_sz);
        buff_idx = 0;

        /* write record back */
        rc = swap_status_write_record(rec_offs, (uint32_t)copy_num, copy_counter, buff);
        assert (rc == 0);

        /* proceed to next record */
        length -= BOOT_SWAP_STATUS_PAYLD_SZ;
        rec_offs += BOOT_SWAP_STATUS_ROW_SZ;
        data_idx += BOOT_SWAP_STATUS_PAYLD_SZ;
    }
    return rc;
}

/**
 * Reads len bytes of status partition with values from *data-pointer.
 *
 * @param targ_area_id  Target area id for which status is being read.
 *                      Not a status-partition area id.
 * @param offset        Status byte offset inside status table. Should not include CRC and CNT.
 * @param data          Pointer to data where status table values will be written.
 * @param len           Number of bytes to be read from status table.
 *
 * @return              0 on success; nonzero on failure.
 */
int swap_status_retrieve(uint32_t target_area_id, uint32_t offs, void *data, uint32_t len)
{
    int rc = 0;

    int32_t init_offs;
    int32_t length = (int32_t)len;
    int32_t copy_num;

    uint32_t rec_offs;
    uint32_t copy_sz;
    uint32_t copy_counter;
    uint32_t data_idx = 0;
    uint32_t buff_idx = offs%BOOT_SWAP_STATUS_PAYLD_SZ;

    uint8_t buff[BOOT_SWAP_STATUS_PAYLD_SZ];

    /* check if end of data is still inside writable area */
    // TODO: update for multi image
    assert ((offs + len) <= BOOT_SWAP_STATUS_D_SIZE_RAW);

    /* pre-calculate sub-area offset */
    init_offs = swap_status_init_offset(target_area_id);
    assert (init_offs >= 0);

    /* will start from it
     * this will be write-aligned */
    rec_offs = init_offs + calc_record_offs(offs);

    /* go over all records to be updated */
    while (length > 0) {
        /* preserve record */
        copy_num = swap_status_read_record(rec_offs, buff, &copy_counter);
        /* it returns copy number */
        if (copy_num < 0) {
            /* something went wrong while read, exit */
            rc = -1;
            break;
        }
        /* update record data */
        if (length > (int)BOOT_SWAP_STATUS_PAYLD_SZ) {
            copy_sz = BOOT_SWAP_STATUS_PAYLD_SZ - buff_idx;
        }
        else {
            copy_sz = length;
        }
        memcpy((uint8_t *)data+data_idx, &buff[buff_idx], copy_sz);
        buff_idx = 0;

        /* proceed to next record */
        length -= BOOT_SWAP_STATUS_PAYLD_SZ;
        rec_offs += BOOT_SWAP_STATUS_ROW_SZ;
        data_idx += BOOT_SWAP_STATUS_PAYLD_SZ;
    }
    return rc;
}

#endif /* MCUBOOT_SWAP_USING_STATUS */
