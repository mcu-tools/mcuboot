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

#include "crc32c.h"
#include <string.h>
#include <stdlib.h>
#include "swap_status.h"
#include "sysflash/sysflash.h"

#ifdef MCUBOOT_SWAP_USING_STATUS

static uint8_t record_buff[BOOT_SWAP_STATUS_ROW_SZ];
static uint8_t status_buff[BOOT_SWAP_STATUS_PAYLD_SZ];
static const struct flash_area *last_fap_stat;
static uint32_t last_fin_offset;

static void status_buff_cache_inv(void) 
{
    last_fap_stat = NULL;
}

static void status_buff_cache_upd(const struct flash_area* fap, uint32_t offset) 
{
    last_fap_stat = fap;
    last_fin_offset = offset;
}

static bool status_buff_cache_valid(const struct flash_area* fap, uint32_t offset) 
{
    return (last_fap_stat == fap) && (last_fin_offset == offset);
}

const uint32_t stat_part_magic[] = {
    BOOT_SWAP_STATUS_MAGIC
};

static inline uint32_t calc_rec_idx(uint32_t value)
{
    return value / BOOT_SWAP_STATUS_PAYLD_SZ;
}

static inline uint32_t calc_record_offs(uint32_t offs)
{
    return BOOT_SWAP_STATUS_ROW_SZ * calc_rec_idx(offs);
}

static inline uint32_t calc_record_crc(const uint8_t *data, uint32_t length)
{
    return crc32c_checksum(data, length);
}

static inline uint32_t pack_bytes_u32(const uint8_t *data)
{
    uint32_t result = 0U;

    result = ((uint32_t)data[3U] << 24U) | ((uint32_t)data[2U] << 16U) |
             ((uint32_t)data[1U] << 8U) | (uint32_t)data[0U];

    return result;
}

int32_t swap_status_init_offset(uint8_t area_id)
{
    int32_t result = -1;
    int32_t offset = 0;
    uint32_t i;

#ifdef MCUBOOT_SWAP_USING_SCRATCH
    #define ADD_ARRAY_MEMBER_FOR_SCRATCH (1U)
#else
    #define ADD_ARRAY_MEMBER_FOR_SCRATCH (0U)
#endif /* MCUBOOT_SWAP_USING_SCRATCH */

    /* we always have at least 2 images in BOOT and UPGRADE slots */
#define ARR_SIZE  (SLOTS_FOR_IMAGE + ADD_ARRAY_MEMBER_FOR_SCRATCH + ((uint8_t)BOOT_IMAGE_NUMBER - 1U) * SLOTS_FOR_IMAGE)

    uint8_t slots_ids[ARR_SIZE];

    slots_ids[0] = FLASH_AREA_IMAGE_PRIMARY(0U);
    slots_ids[1] = FLASH_AREA_IMAGE_SECONDARY(0U);

#ifdef MCUBOOT_SWAP_USING_SCRATCH
    /* The third position of SCRATCH is saved as it was before */
    slots_ids[2] = FLASH_AREA_IMAGE_SCRATCH;
#endif /* MCUBOOT_SWAP_USING_SCRATCH */

#if (BOOT_IMAGE_NUMBER > 1)

    uint8_t primary_slots_per_id = SLOTS_FOR_IMAGE + ADD_ARRAY_MEMBER_FOR_SCRATCH;
    uint8_t secondary_slots_per_id = primary_slots_per_id + 1U;

    for (i = 1U; i < (uint32_t) BOOT_IMAGE_NUMBER; ++i)
    {
        slots_ids[primary_slots_per_id] = FLASH_AREA_IMAGE_PRIMARY( i );
        slots_ids[secondary_slots_per_id] = FLASH_AREA_IMAGE_SECONDARY( i );
        primary_slots_per_id += 2U;
        secondary_slots_per_id += 2U;
    }

#endif /* BOOT_IMAGE_NUMBER > 1 */

    for (i = 0U; i < ARR_SIZE; i++) {
        if (slots_ids[i] == area_id) {
            result = offset;
            break;
        }
        offset += BOOT_SWAP_STATUS_SIZE;
    }

    return result;
}

static int swap_status_read_record(uint32_t rec_offset, uint8_t *data, uint32_t *copy_counter, uint32_t *max_idx)
{ /* returns BOOT_SWAP_STATUS_PAYLD_SZ of data */
    int rc = -1;

    uint32_t fin_offset;
    uint32_t data_offset = 0;
    uint32_t counter, crc, magic;
    uint32_t crc_fail = 0;
    uint32_t magic_fail = 0;
    uint32_t max_cnt = 0;

    const struct flash_area *fap_stat = NULL;

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return -1;
    }

    /* loop over copies/duplicates */
    for (uint32_t i = 0; i < BOOT_SWAP_STATUS_MULT; i++) {
        /* calculate final duplicate offset */
        fin_offset = rec_offset + i * BOOT_SWAP_STATUS_D_SIZE;

        rc = flash_area_read(fap_stat, fin_offset + BOOT_SWAP_STATUS_PAYLD_SZ, &magic, sizeof(magic));
        if (rc != 0) {
            return -1;
        }

        if (magic == BOOT_SWAP_STATUS_MAGIC) {
            rc = flash_area_read(fap_stat, fin_offset, record_buff, sizeof(record_buff));
            if (rc != 0) {
                return -1;
            }

            crc = pack_bytes_u32(&record_buff[BOOT_SWAP_STATUS_ROW_SZ -
                                              BOOT_SWAP_STATUS_CRC_SZ]);
            /* check record data integrity first */
            if (crc == calc_record_crc(record_buff, BOOT_SWAP_STATUS_ROW_SZ - BOOT_SWAP_STATUS_CRC_SZ)) {
                /* look for counter */
                counter = pack_bytes_u32(&record_buff[BOOT_SWAP_STATUS_ROW_SZ -
                                                      BOOT_SWAP_STATUS_CNT_SZ -
                                                      BOOT_SWAP_STATUS_CRC_SZ]);
                /* find out counter max */
                if (counter >= max_cnt) {
                    max_cnt = counter;
                    *max_idx = i;
                    data_offset = fin_offset;
                }
            }
            /* if crc != calculated() */
            else {
                (void) flash_area_erase(fap_stat, fin_offset, sizeof(record_buff));
                crc_fail++;
            }

        } else {
            magic_fail++;
        }
    }

    /* no magic found - status area is pre-erased, start from scratch */
    if (magic_fail == BOOT_SWAP_STATUS_MULT) {
        /* emulate last index was received, so next will start from beginning */
        *max_idx = BOOT_SWAP_STATUS_MULT - 1U;
        *copy_counter = 0;
        /* return all erased values */
        if (status_buff_cache_valid(fap_stat, fin_offset) == false) {
            (void)memset(data, (int32_t)flash_area_erased_val(fap_stat), BOOT_SWAP_STATUS_PAYLD_SZ);
            status_buff_cache_upd(fap_stat, fin_offset);
        }
    }
    else {
        /* no valid CRC found - status pre-read failure */
        if (crc_fail == BOOT_SWAP_STATUS_MULT) {
            rc = -1;
        }
        else {
            *copy_counter = max_cnt;
            /* read payload data */
            rc = flash_area_read(fap_stat, data_offset, data, BOOT_SWAP_STATUS_PAYLD_SZ);
            status_buff_cache_inv();
            if (rc != 0) {
                rc = -1;
            }
        }
    }
    flash_area_close(fap_stat);

    /* return back duplicate index */
    return rc;
}

static int swap_status_write_record(uint32_t rec_offset, uint32_t copy_num, uint32_t copy_counter, const uint8_t *data)
{ /* it receives explicitly BOOT_SWAP_STATUS_PAYLD_SZ of data */
    int rc = -1;

    uint32_t fin_offset, fin_offset_prev;
    /* increment counter field */
    uint32_t next_counter = copy_counter + 1U;
    uint32_t next_crc;
    uint32_t copy_num_prev;

    const struct flash_area *fap_stat = NULL;

    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return -1;
    }

    /* copy data into buffer */
    (void)memcpy(record_buff, data, BOOT_SWAP_STATUS_PAYLD_SZ);
    /* append next counter to whole record row */
    (void)memcpy(&record_buff[BOOT_SWAP_STATUS_ROW_SZ-BOOT_SWAP_STATUS_CNT_SZ-BOOT_SWAP_STATUS_CRC_SZ],
            (uint8_t *)&next_counter,
            BOOT_SWAP_STATUS_CNT_SZ);
    /* append record magic */
    (void)memcpy(&record_buff[BOOT_SWAP_STATUS_PAYLD_SZ], (const uint8_t *)stat_part_magic, BOOT_SWAP_STATUS_MGCREC_SZ);

    /* calculate CRC field*/
    next_crc = calc_record_crc(record_buff, BOOT_SWAP_STATUS_ROW_SZ-BOOT_SWAP_STATUS_CRC_SZ);

    /* append new CRC to whole record row */
    (void)memcpy(&record_buff[BOOT_SWAP_STATUS_ROW_SZ-BOOT_SWAP_STATUS_CRC_SZ],
            (uint8_t *)&next_crc,
            BOOT_SWAP_STATUS_CRC_SZ);

    /* we already know what copy number was last and correct */
    /* increment duplicate index */
    /* calculate final duplicate offset */
    copy_num_prev = copy_num;

    if (copy_num == (BOOT_SWAP_STATUS_MULT - 1U)) {
        copy_num = 0;
    }
    else {
        copy_num++;
    }
    fin_offset_prev = rec_offset + copy_num_prev*BOOT_SWAP_STATUS_D_SIZE;
    fin_offset = rec_offset + copy_num*BOOT_SWAP_STATUS_D_SIZE;

    /* erase obsolete status record before write */
    rc = flash_area_erase(fap_stat, fin_offset, sizeof(record_buff));
    if (rc != 0) {
        return -1;
    }

    /* write prepared record into flash */
    rc = flash_area_write(fap_stat, fin_offset, record_buff, sizeof(record_buff));
    if (rc != 0) {
        return -1;
    }

    rc = flash_area_erase(fap_stat, fin_offset_prev, sizeof(record_buff));

    flash_area_close(fap_stat);

    return rc;
}

static int boot_magic_decode(uint8_t *magic)
{
    if (memcmp((const void *)magic, (const void *)&boot_img_magic.val, BOOT_MAGIC_SZ) == 0) {
        return BOOT_MAGIC_GOOD;
    }
    return BOOT_MAGIC_BAD;
}

/**
 * Updates len bytes of status partition with values from *data-pointer.
 *
 * @param target_area_id Target area id for which status is being written.
 *                       Not a status-partition area id.
 * @param offset         Status byte offset inside status table. Should not include CRC and CNT.
 * @param data           Pointer to data status table to needs to be updated with.
 * @param len            Number of bytes to be written
 *
 * @return               0 on success; -1 on failure.
 */
int swap_status_update(uint8_t target_area_id, uint32_t offs, const void *data, uint32_t len)
{
    int rc = -1;

    int32_t init_offs;
    uint32_t copy_num = 0;

    uint32_t rec_offs;
    uint32_t copy_sz;
    uint32_t copy_counter = 0;
    uint32_t data_idx = 0;
    uint32_t buff_idx = offs % BOOT_SWAP_STATUS_PAYLD_SZ;

    if ((UINT32_MAX - offs) < len) {
        return -1;
    }

    /* check if end of data is still inside writable area */
    if ((offs + len) > BOOT_SWAP_STATUS_D_SIZE_RAW) {
        return -1;
    }

    /* pre-calculate sub-area offset */
    init_offs = swap_status_init_offset(target_area_id);
    if (init_offs < 0) {
        return -1;
    }

    /* will start from it
     * this will be write-aligned */
    rec_offs = (uint32_t)init_offs + calc_record_offs(offs);

    /* go over all records to be updated */
    while (len != 0U) {
        /* preserve record */
        rc = swap_status_read_record(rec_offs, status_buff, &copy_counter, &copy_num);
        /* it returns copy number */
        if (rc < 0)
        {   /* something went wrong while read, exit */
            rc = -1;
            break;
        }
        /* update record data */
        if (len > BOOT_SWAP_STATUS_PAYLD_SZ) {
            copy_sz = BOOT_SWAP_STATUS_PAYLD_SZ - buff_idx;
        }
        else {
            copy_sz = len;
        }

        (void)memcpy(status_buff + buff_idx, (const uint8_t *)data + data_idx, copy_sz);
        status_buff_cache_inv();
        buff_idx = 0;

        /* write record back */
        rc = swap_status_write_record(rec_offs, copy_num, copy_counter, status_buff);
        if (rc != 0) {
            break;
        }

        /* proceed to next record */
        if (len < BOOT_SWAP_STATUS_PAYLD_SZ) {
            len = 0;
        }
        else {
            len -= BOOT_SWAP_STATUS_PAYLD_SZ;
            rec_offs += BOOT_SWAP_STATUS_ROW_SZ;
            data_idx += BOOT_SWAP_STATUS_PAYLD_SZ;
        }

    }
    return rc;
}

/**
 * Reads len bytes of status partition with values from *data-pointer.
 *
 * @param target_area_id Target area id for which status is being read.
 *                       Not a status-partition area id.
 * @param offset         Status byte offset inside status table. Should not include CRC and CNT.
 * @param data           Pointer to data where status table values will be written.
 * @param len            Number of bytes to be read from status table.
 *
 * @return               0 on success; -1 on failure.
 */
int swap_status_retrieve(uint8_t target_area_id, uint32_t offs, void *data, uint32_t len)
{
    int rc = 0;

    int32_t init_offs;
    uint32_t copy_num = 0;

    uint32_t rec_offs;
    uint32_t copy_sz;
    uint32_t copy_counter = 0;
    uint32_t data_idx = 0;
    uint32_t buff_idx = offs % BOOT_SWAP_STATUS_PAYLD_SZ;

    if ((UINT32_MAX - offs) < len) {
        return -1;
    }

    /* check if end of data is still inside writable area */
    if ((offs + len) > BOOT_SWAP_STATUS_D_SIZE_RAW) {
        return -1;
    }

    /* pre-calculate sub-area offset */
    init_offs = swap_status_init_offset(target_area_id);
    if (init_offs < 0) {
        return -1;
    }

    /* will start from it
     * this will be write-aligned */
    rec_offs = (uint32_t)init_offs + calc_record_offs(offs);

    /* go over all records to be updated */
    while (len != 0U) {
        /* preserve record */
        rc = swap_status_read_record(rec_offs, status_buff, &copy_counter, &copy_num);
        /* it returns copy number */
        if (rc < 0) {
            /* something went wrong while read, exit */
            rc = -1;
            break;
        }
        /* update record data */
        if (len > BOOT_SWAP_STATUS_PAYLD_SZ) {
            copy_sz = BOOT_SWAP_STATUS_PAYLD_SZ - buff_idx;
        }
        else {
            copy_sz = len;
        }

        (void)memcpy((uint8_t *)data + data_idx, status_buff + buff_idx, copy_sz);
        buff_idx = 0;

        /* proceed to next record */
        if (len < BOOT_SWAP_STATUS_PAYLD_SZ) {
            len = 0;
        }
        else {
            len -= BOOT_SWAP_STATUS_PAYLD_SZ;
            rec_offs += BOOT_SWAP_STATUS_ROW_SZ;
            data_idx += BOOT_SWAP_STATUS_PAYLD_SZ;
        }
    }
    return rc;
}

/**
 * Copy trailer from status partition to primary image and set copy_done flag.
 * This function calls only once, before set copy_done flag in status trailer
 *
 * @param fap           Target area id for which status is being read.
 *
 * @return              0 on success; -1 on failure.
 */
int swap_status_to_image_trailer(const struct flash_area *fap)
{
    uint8_t erased_val;
    uint8_t stat_erased_val;
    const uint8_t *copy_src;
    uint32_t cur_trailer_pos;
    uint32_t primary_trailer_sz;
    uint32_t primary_trailer_buf_sz;
    uint32_t align = MEMORY_ALIGN;
    int rc = 0;
    const struct flash_area *fap_stat = NULL;
    uint8_t primary_trailer_buf[MAX_TRAILER_BUF_SIZE];

    /* get the erased flash byte for status partition */
    rc = flash_area_open(FLASH_AREA_IMAGE_SWAP_STATUS, &fap_stat);
    if (rc != 0) {
        return rc;
    }
    stat_erased_val = flash_area_erased_val(fap_stat);
    flash_area_close(fap_stat);

    /* get status partition trailer size and copy it to buffer */
    const uint32_t status_trailer_buf_sz = BOOT_SWAP_STATUS_SWAPSZ_SZ + BOOT_SWAP_STATUS_SWAPINF_SZ +
                BOOT_SWAP_STATUS_COPY_DONE_SZ + BOOT_SWAP_STATUS_IMG_OK_SZ + BOOT_SWAP_STATUS_MAGIC_SZ;
    uint8_t status_trailer_buf[status_trailer_buf_sz];
    (void)memset(&status_trailer_buf, 0, status_trailer_buf_sz);
    rc = swap_status_retrieve(fap->fa_id, boot_swap_size_off(fap), (uint8_t *)status_trailer_buf, status_trailer_buf_sz);
    if (rc != 0) {
        return rc;
    }

    /* check trailer magic in status partition */
    if (boot_magic_decode(&status_trailer_buf[status_trailer_buf_sz - BOOT_SWAP_STATUS_MAGIC_SZ]) != BOOT_MAGIC_GOOD) {
        return -1;
    }

    /* get primary slot trailer size without status data fields */
    primary_trailer_sz = boot_trailer_sz(0);

    /* align image trailer buffer size to minimal write size */
#if !defined(__BOOTSIM__)
    align = flash_area_align(fap);
    /* TODO: this code needs to be refined. It is introduced to overcome
     * situation when MCUBootApp lives in internal memory, but user app
     * is executed from different type memory - external in XIP mode in
     * this case. This situation now arise on PSOC6 when XIP execution is
     * used, bay may be applicable to other devices, where solution is
     * distributed between memories with different write/erase sizes.
     */
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    if (align > MEMORY_ALIGN) {
        align = MEMORY_ALIGN;
    }
#endif
#else
    align = MEMORY_ALIGN;
#endif

    if ((align > MAX_TRAILER_BUF_SIZE) || (align == 0U)) {
        return -1;
    }

    primary_trailer_buf_sz = align;
    while (primary_trailer_buf_sz < primary_trailer_sz) {
        primary_trailer_buf_sz += align;
    }
    if (primary_trailer_buf_sz > MAX_TRAILER_BUF_SIZE) {
        return -1;
    }

    /* erase primary slot trailer */
    rc= flash_area_erase(fap, fap->fa_size - primary_trailer_buf_sz, primary_trailer_buf_sz);
    if (rc != 0) {
        return rc;
    }

    /* erase trailer area */
    erased_val = flash_area_erased_val(fap); /* stat_erased_val and erased_val may differ! */
    (void)memset(&primary_trailer_buf[primary_trailer_buf_sz-primary_trailer_sz], (int) erased_val, primary_trailer_sz);

    /* copy and align flags and data from status_trailer_buf to primary_trailer_buf
                                         Status part trailer --> Pimary image trailer */

    /* copy trailer magic */
    cur_trailer_pos = primary_trailer_buf_sz - BOOT_SWAP_STATUS_MAGIC_SZ;
    copy_src = &status_trailer_buf[status_trailer_buf_sz - BOOT_SWAP_STATUS_MAGIC_SZ];
    if (stat_erased_val != erased_val &&
        bootutil_buffer_is_filled(copy_src, stat_erased_val, BOOT_SWAP_STATUS_MAGIC_SZ)) {

        (void)memset(&primary_trailer_buf[cur_trailer_pos], (int)erased_val, BOOT_SWAP_STATUS_MAGIC_SZ);
    }
    else {
        (void)memcpy(&primary_trailer_buf[cur_trailer_pos], copy_src, BOOT_SWAP_STATUS_MAGIC_SZ);
    }

    /* copy image_ok flag */
    cur_trailer_pos -= BOOT_MAX_ALIGN;
    copy_src = &status_trailer_buf[BOOT_SWAP_STATUS_SWAPSZ_SZ + BOOT_SWAP_STATUS_SWAPINF_SZ + BOOT_SWAP_STATUS_COPY_DONE_SZ];
    if (stat_erased_val != erased_val && stat_erased_val == *copy_src) {
        copy_src = &erased_val;
    }
    primary_trailer_buf[cur_trailer_pos] = *copy_src;

    /* set copy_done flag */
    cur_trailer_pos -= BOOT_MAX_ALIGN;
    primary_trailer_buf[cur_trailer_pos] = BOOT_FLAG_SET;

    /* copy swap_type flag */
    cur_trailer_pos -= BOOT_MAX_ALIGN;
    copy_src = &status_trailer_buf[BOOT_SWAP_STATUS_SWAPSZ_SZ];
    if (stat_erased_val != erased_val && stat_erased_val == *copy_src) {
        copy_src = &erased_val;
    }
    primary_trailer_buf[cur_trailer_pos] = *copy_src;

    /* copy swap_size field */
    cur_trailer_pos -= BOOT_MAX_ALIGN;
    if (stat_erased_val != erased_val &&
        bootutil_buffer_is_filled(status_trailer_buf, stat_erased_val, BOOT_SWAP_STATUS_SWAPSZ_SZ)) {

        (void)memset(&primary_trailer_buf[cur_trailer_pos], (int)erased_val, BOOT_SWAP_STATUS_SWAPSZ_SZ);
    }
    else {
        (void)memcpy(&primary_trailer_buf[cur_trailer_pos], status_trailer_buf, BOOT_SWAP_STATUS_SWAPSZ_SZ);
    }

    /* write primary image trailer with copy_done flag set */
    rc = flash_area_write(fap, fap->fa_size - primary_trailer_buf_sz, primary_trailer_buf, primary_trailer_buf_sz);

    return rc;
}

#endif /* MCUBOOT_SWAP_USING_STATUS */
