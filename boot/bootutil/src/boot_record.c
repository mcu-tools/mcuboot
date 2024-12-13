/*
 * Copyright (c) 2018-2023 Arm Limited
 * Copyright (c) 2020 Linaro Limited
 * Copyright (c) 2023, Nordic Semiconductor ASA
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
#include "bootutil/crypto/sha.h"

#if defined(MCUBOOT_MEASURED_BOOT) || defined(MCUBOOT_DATA_SHARING)
#include "bootutil/boot_record.h"
#include "bootutil/boot_status.h"
#include "bootutil_priv.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"

#if defined(MCUBOOT_DATA_SHARING_BOOTINFO)
static bool saved_bootinfo = false;
#endif

#if !defined(MCUBOOT_CUSTOM_DATA_SHARING_FUNCTION)
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
    struct shared_boot_data *boot_data;
    uint16_t boot_data_size;
    uintptr_t tlv_end, offset;

    if (data == NULL) {
        return SHARED_MEMORY_GEN_ERROR;
    }

    boot_data = (struct shared_boot_data *)MCUBOOT_SHARED_DATA_BASE;

    /* Check whether first time to call this function. If does then initialise
     * shared data area.
     */
    if (!shared_memory_init_done) {
        memset((void *)MCUBOOT_SHARED_DATA_BASE, 0, MCUBOOT_SHARED_DATA_SIZE);
        boot_data->header.tlv_magic   = SHARED_DATA_TLV_INFO_MAGIC;
        boot_data->header.tlv_tot_len = SHARED_DATA_HEADER_SIZE;
        shared_memory_init_done = true;
    }

    /* Check whether TLV entry is already added.
     * Get the boundaries of TLV section
     */
    tlv_end = MCUBOOT_SHARED_DATA_BASE + boot_data->header.tlv_tot_len;
    offset  = MCUBOOT_SHARED_DATA_BASE + SHARED_DATA_HEADER_SIZE;

    /* Iterates over the TLV section looks for the same entry if found then
     * returns with error: SHARED_MEMORY_OVERWRITE
     */
    while (offset < tlv_end) {
        /* Create local copy to avoid unaligned access */
        memcpy(&tlv_entry, (const void *)offset, SHARED_DATA_ENTRY_HEADER_SIZE);
        if (GET_MAJOR(tlv_entry.tlv_type) == major_type &&
            GET_MINOR(tlv_entry.tlv_type) == minor_type) {
            return SHARED_MEMORY_OVERWRITE;
        }

        offset += SHARED_DATA_ENTRY_SIZE(tlv_entry.tlv_len);
    }

    /* Add TLV entry */
    tlv_entry.tlv_type = SET_TLV_TYPE(major_type, minor_type);
    tlv_entry.tlv_len  = size;

    if (!boot_u16_safe_add(&boot_data_size, boot_data->header.tlv_tot_len,
                           SHARED_DATA_ENTRY_SIZE(size))) {
        return SHARED_MEMORY_GEN_ERROR;
    }

    /* Verify overflow of shared area */
    if (boot_data_size > MCUBOOT_SHARED_DATA_SIZE) {
        return SHARED_MEMORY_OVERFLOW;
    }

    offset = tlv_end;
    memcpy((void *)offset, &tlv_entry, SHARED_DATA_ENTRY_HEADER_SIZE);

    offset += SHARED_DATA_ENTRY_HEADER_SIZE;
    memcpy((void *)offset, data, size);

    boot_data->header.tlv_tot_len += SHARED_DATA_ENTRY_SIZE(size);

    return SHARED_MEMORY_OK;
}
#endif /* MCUBOOT_MEASURED_BOOT OR MCUBOOT_DATA_SHARING */
#endif /* !MCUBOOT_CUSTOM_DATA_SHARING_FUNCTION */

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
    uint8_t image_hash[IMAGE_HASH_SIZE];
    uint8_t buf[MAX_BOOT_RECORD_SZ];
    bool boot_record_found = false;
    bool hash_found = false;
    int rc;

    /* Manifest data is concatenated to the end of the image.
     * It is encoded in TLV format.
     */

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_ANY, false);
    if (rc) {
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

        if (type == IMAGE_TLV_BOOT_RECORD) {
            if (len > sizeof(buf)) {
                return -1;
            }
            rc = flash_area_read(fap, offset, buf, len);
            if (rc) {
                return -1;
            }

            record_len = len;
            boot_record_found = true;

        } else if (type == EXPECTED_HASH_TLV) {
            /* Get the image's hash value from the manifest section. */
            if (len > sizeof(image_hash)) {
                return -1;
            }
            rc = flash_area_read(fap, offset, image_hash, len);
            if (rc) {
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
    }

    if (!boot_record_found || !hash_found) {
        return -1;
    }

    /* Ensure that we have enough in the record for the hash.  This
     * prevents an underflow in the calculation below.
     */
    if (record_len < sizeof(image_hash)) {
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
    offset = record_len - sizeof(image_hash);
    /* The size of 'buf' has already been checked when
     * the BOOT_RECORD TLV was read, it won't overflow.
     */
    memcpy(buf + offset, image_hash, sizeof(image_hash));

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

#ifdef MCUBOOT_DATA_SHARING_BOOTINFO
int boot_save_shared_data(const struct image_header *hdr, const struct flash_area *fap,
                          const uint8_t slot, const struct image_max_size *max_app_sizes)
{
    int rc;
#if !defined(MCUBOOT_SINGLE_APPLICATION_SLOT)
    uint8_t image = 0;
#endif

#if defined(MCUBOOT_SINGLE_APPLICATION_SLOT)
    uint8_t mode = MCUBOOT_MODE_SINGLE_SLOT;
#elif defined(MCUBOOT_SWAP_USING_SCRATCH)
    uint8_t mode = MCUBOOT_MODE_SWAP_USING_SCRATCH;
#elif defined(MCUBOOT_OVERWRITE_ONLY)
    uint8_t mode = MCUBOOT_MODE_UPGRADE_ONLY;
#elif defined(MCUBOOT_SWAP_USING_MOVE)
    uint8_t mode = MCUBOOT_MODE_SWAP_USING_MOVE;
#elif defined(MCUBOOT_DIRECT_XIP)
#if defined(MCUBOOT_DIRECT_XIP_REVERT)
    uint8_t mode = MCUBOOT_MODE_DIRECT_XIP_WITH_REVERT;
#else
    uint8_t mode = MCUBOOT_MODE_DIRECT_XIP;
#endif
#elif defined(MCUBOOT_RAM_LOAD)
    uint8_t mode = MCUBOOT_MODE_RAM_LOAD;
#elif defined(MCUBOOT_FIRMWARE_LOADER)
    uint8_t mode = MCUBOOT_MODE_FIRMWARE_LOADER;
#elif defined(MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD)
    uint8_t mode = MCUBOOT_MODE_SINGLE_SLOT_RAM_LOAD;
#else
#error "Unknown mcuboot operating mode"
#endif

#if defined(MCUBOOT_SIGN_RSA)
    uint8_t signature_type = MCUBOOT_SIGNATURE_TYPE_RSA;
#elif defined(MCUBOOT_SIGN_EC256)
    uint8_t signature_type = MCUBOOT_SIGNATURE_TYPE_ECDSA_P256;
#elif defined(MCUBOOT_SIGN_ED25519)
    uint8_t signature_type = MCUBOOT_SIGNATURE_TYPE_ED25519;
#else
    uint8_t signature_type = MCUBOOT_SIGNATURE_TYPE_NONE;
#endif

#if defined(MCUBOOT_SERIAL_RECOVERY)
    uint8_t recovery = MCUBOOT_RECOVERY_MODE_SERIAL_RECOVERY;
#elif defined(MCUBOOT_USB_DFU)
    uint8_t recovery = MCUBOOT_RECOVERY_MODE_DFU;
#else
    uint8_t recovery = MCUBOOT_RECOVERY_MODE_NONE;
#endif

#if defined(MCUBOOT_VERSION_AVAILABLE)
    struct image_version mcuboot_version = {
        .iv_major = MCUBOOT_VERSION_MAJOR,
        .iv_minor = MCUBOOT_VERSION_MINOR,

#if defined(MCUBOOT_VERSION_PATCHLEVEL)
        .iv_revision = MCUBOOT_VERSION_PATCHLEVEL,
#else
        .iv_revision = 0,
#endif

#if defined(MCUBOOT_VERSION_TWEAK)
        .iv_build_num = MCUBOOT_VERSION_TWEAK,
#else
        .iv_build_num = 0,
#endif
    };
#endif

    if (saved_bootinfo) {
        /* Boot info has already been saved, nothing to do */
        return 0;
    }

    /* Write out all fields */
    rc = boot_add_data_to_shared_area(TLV_MAJOR_BLINFO, BLINFO_MODE,
                                      sizeof(mode), &mode);

    if (!rc) {
        rc = boot_add_data_to_shared_area(TLV_MAJOR_BLINFO,
                                          BLINFO_SIGNATURE_TYPE,
                                          sizeof(signature_type),
                                          &signature_type);
    }

    if (!rc) {
        rc = boot_add_data_to_shared_area(TLV_MAJOR_BLINFO,
                                          BLINFO_RECOVERY,
                                          sizeof(recovery), &recovery);
    }

#if !defined(MCUBOOT_SINGLE_APPLICATION_SLOT)
    if (!rc) {
        rc = boot_add_data_to_shared_area(TLV_MAJOR_BLINFO,
                                          BLINFO_RUNNING_SLOT,
                                          sizeof(slot), (void *)&slot);
    }
#endif

#if defined(MCUBOOT_VERSION_AVAILABLE)
    if (!rc) {
        rc = boot_add_data_to_shared_area(TLV_MAJOR_BLINFO,
                                          BLINFO_BOOTLOADER_VERSION,
                                          sizeof(mcuboot_version),
                                          (void *)&mcuboot_version);
    }
#endif

#if !defined(MCUBOOT_SINGLE_APPLICATION_SLOT)
    while (image < BOOT_IMAGE_NUMBER && !rc) {
        if (max_app_sizes[image].calculated == true) {
            rc = boot_add_data_to_shared_area(TLV_MAJOR_BLINFO,
                                              (BLINFO_MAX_APPLICATION_SIZE + image),
                                              sizeof(max_app_sizes[image].max_size),
                                              (void *)&max_app_sizes[image].max_size);

        }

        ++image;
    }
#endif

    if (!rc) {
        saved_bootinfo = true;
    }

    return rc;
}
#endif /* MCUBOOT_DATA_SHARING_BOOTINFO */
