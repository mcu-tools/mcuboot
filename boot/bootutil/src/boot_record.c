/*
 * Copyright (c) 2018-2021 Arm Limited
 * Copyright (c) 2020 Linaro Limited
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
 */

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_MEASURED_BOOT) || defined(MCUBOOT_DATA_SHARING)
#include "bootutil/crypto/sha256.h"
#include "bootutil/boot_record.h"
#include "bootutil/boot_status.h"
#include "bootutil_priv.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"

/* Error codes for using the shared memory area. */
#define SHARED_MEMORY_OK            (0)
#define SHARED_MEMORY_OVERFLOW      (1)
#define SHARED_MEMORY_OVERWRITE     (2)
#define SHARED_MEMORY_GEN_ERROR     (3)
#define SHARED_MEMORY_CORRUPTED     (4)

/**
 * @var shared_memory_init_done
 *
 * @brief Indicates whether shared memory area was already initialized.
 *
 */
static bool shared_memory_init_done;

/* See in boot_record.h */
int
boot_add_data_to_shared_area(uint8_t        major_type,
                             uint16_t       minor_type,
                             size_t         size,
                             const uint8_t *data)
{
    struct shared_data_tlv_entry tlv_entry = {0};
    uint16_t type = SET_TLV_TYPE(major_type, minor_type);
    struct shared_boot_data *boot_data;
    uint16_t boot_data_size;
    uintptr_t tlv_end, offset;

    if (data == NULL) {
        return SHARED_MEMORY_GEN_ERROR;
    }

    /* Shared data section must be aligned as 'void*' */
    assert(((uintptr_t)MCUBOOT_SHARED_DATA_BASE & 3u) == 0u);
    boot_data = (struct shared_boot_data *)MCUBOOT_SHARED_DATA_BASE;

    /* Check whether first time to call this function. If does then initialise
     * shared data area.
     */
    if (!shared_memory_init_done) {
        (void)memset((void *)MCUBOOT_SHARED_DATA_BASE, 0, MCUBOOT_SHARED_DATA_SIZE);
        boot_data->header.tlv_magic   = SHARED_DATA_TLV_INFO_MAGIC;
        boot_data->header.tlv_tot_len = (uint16_t)SHARED_DATA_HEADER_SIZE;
        shared_memory_init_done = true;
    }

    /* Check whether TLV entry is already added.
     * Get the boundaries of TLV section
     */
    tlv_end = MCUBOOT_SHARED_DATA_BASE + (uint32_t)(boot_data->header.tlv_tot_len);
    offset  = MCUBOOT_SHARED_DATA_BASE + SHARED_DATA_HEADER_SIZE;

    /* Iterates over the TLV section looks for the same entry if found then
     * returns with error: SHARED_MEMORY_OVERWRITE
     */
    while (offset < tlv_end) {
        /* Create local copy to avoid unaligned access */
        (void)memcpy((void*)&tlv_entry, (const void *)offset, SHARED_DATA_ENTRY_HEADER_SIZE);
        if (tlv_entry.tlv_type == type) {
            return SHARED_MEMORY_OVERWRITE;
        }

        offset += SHARED_DATA_ENTRY_SIZE(tlv_entry.tlv_len);
    }

    /* Add TLV entry */
    tlv_entry.tlv_type = type;

    if (size > (unsigned)UINT16_MAX - SHARED_DATA_ENTRY_HEADER_SIZE) {
        return SHARED_MEMORY_GEN_ERROR;
    }

    tlv_entry.tlv_len = (uint16_t)size;
    if (!boot_u16_safe_add(&boot_data_size, boot_data->header.tlv_tot_len,
                            (uint16_t)SHARED_DATA_ENTRY_SIZE(size))) {
        return SHARED_MEMORY_GEN_ERROR;
    }

    /* Verify overflow of shared area */
    if (boot_data_size > MCUBOOT_SHARED_DATA_SIZE) {
        return SHARED_MEMORY_OVERFLOW;
    }

    offset = tlv_end;
    (void)memcpy((void *)offset, (const void *)&tlv_entry, SHARED_DATA_ENTRY_HEADER_SIZE);

    offset += SHARED_DATA_ENTRY_HEADER_SIZE;
    (void)memcpy((void *)offset, (const void *)data, size);

    boot_data->header.tlv_tot_len = boot_data_size;

    return SHARED_MEMORY_OK;
}
#endif /* MCUBOOT_MEASURED_BOOT OR MCUBOOT_DATA_SHARING */

#ifdef MCUBOOT_MEASURED_BOOT
/* See in boot_record.h */
int
boot_save_boot_status(uint8_t sw_module,
                      const struct image_header *hdr,
                      const struct flash_area *fap)
{

    struct image_tlv_iter it;
    uint32_t offset;
    uint16_t len;
    uint16_t type;
    uint16_t ias_minor;
    size_t record_len = 0;
    uint8_t image_hash[BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE];
    uint8_t buf[MAX_BOOT_RECORD_SZ];
    bool boot_record_found = false;
    bool hash_found = false;
    int rc;

    if (NULL == hdr || NULL == fap) {
        return -1;
    }

    /* Manifest data is concatenated to the end of the image.
     * It is encoded in TLV format.
     */

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_ANY, false);
    if (rc != 0) {
        return -1;
    }

    /* Traverse through the TLV area to find the boot record
     * and image hash TLVs.
     */
    while (true) {
        rc = bootutil_tlv_iter_next(&it, &offset, &len, &type);
        if (rc < 0) {
            return -1;
        } else if (rc > 0) {
            break;
        }
        else {
            /* No action required - for MISRA C-2012 Rule 15.7 rule */
        }

        if ((uint16_t)IMAGE_TLV_BOOT_RECORD == type) {
            if (len > sizeof(buf)) {
                return -1;
            }
            rc = flash_area_read(fap, offset, buf, len);
            if (rc != 0) {
                return -1;
            }

            record_len = len;
            boot_record_found = true;

        } else if ((uint16_t)IMAGE_TLV_SHA256 == type) {
            /* Get the image's hash value from the manifest section. */
            if (len != (uint16_t)BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE) {
                return -1;
            }
            rc = flash_area_read(fap, offset, image_hash, len);
            if (rc != 0) {
                return -1;
            }

            hash_found = true;

            /* The boot record TLV is part of the protected TLV area which is
             * located before the other parts of the TLV area (including the
             * image hash) so at this point it is okay to break the loop
             * as the boot record TLV should have already been found.
             */
            break;
        }
        else {
            /* No action required - for MISRA C-2012 Rule 15.7 rule */
        }
    }


    if (!boot_record_found || !hash_found) {
        return -1;
    }

    /* Update the measurement value (hash of the image) data item in the
     * boot record. It is always the last item in the structure to make
     * it easy to calculate its position.
     * The image hash is computed over the image header, the image itself and
     * the protected TLV area (which should already include the image hash as
     * part of the boot record TLV). For this reason this field has been
     * filled with zeros during the image signing process.
     */
    if (record_len < sizeof(image_hash)) {
        return -1;
    }

    offset = record_len - sizeof(image_hash);
    /* The size of 'buf' has already been checked when
     * the BOOT_RECORD TLV was read, it won't overflow.
     */
    (void)memcpy(buf + offset, image_hash, sizeof(image_hash));

    /* Add the CBOR encoded boot record to the shared data area. */
    ias_minor = SET_IAS_MINOR(sw_module, SW_BOOT_RECORD);
    rc = boot_add_data_to_shared_area(TLV_MAJOR_IAS,
                                      ias_minor,
                                      record_len,
                                      buf);
    if (rc != SHARED_MEMORY_OK) {
        return rc;
    }

    return 0;
}

#endif /* MCUBOOT_MEASURED_BOOT */

#ifdef MCUBOOT_DATA_SHARING

int boot_save_shared_data(const struct image_header *hdr,
                          const struct flash_area *fap)
{
    uint16_t fwu_minor;
    const struct flash_area *temp_fap;
    uint8_t fwu_img_id = 0;
    uint8_t i;

    if (NULL == hdr || NULL == fap) {
        return -1;
    }

    for (i = 0; i < (uint8_t)MCUBOOT_IMAGE_NUMBER; i++) {
        if (flash_area_open(FLASH_AREA_IMAGE_PRIMARY(i),
                            &temp_fap) != 0) {
            return -1;
        }

        if (fap == temp_fap) {
            fwu_img_id = i;
            break;
        }
    }

    if ((uint8_t)MCUBOOT_IMAGE_NUMBER == i) {
        return -1;
    }

    /* Currently hardcode it to 0 which indicates the full image. */
    fwu_minor = SET_FWU_MINOR(fwu_img_id, SW_VERSION);
    return boot_add_data_to_shared_area(TLV_MAJOR_FWU,
                                        fwu_minor,
                                        sizeof(hdr->ih_ver),
                                        (const uint8_t *)&hdr->ih_ver);
}
#endif /* MCUBOOT_DATA_SHARING */
