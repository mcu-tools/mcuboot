/*
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "flash_map_backend/flash_map_backend.h"
#include "bootutil/boot_hooks.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/crypto/sha.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/image.h"
#include "bootutil_loader.h"
#include "bootutil_priv.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#ifdef MCUBOOT_DELTA_DFU

#define BOOT_DELTA_MAGIC   0x314c444d /* "MDL1" */
#define BOOT_DELTA_VERSION 2
#define BOOT_DELTA_HEADER_SIZE 32
#define BOOT_DELTA_F_RESTORE 0x00000001

#ifndef MCUBOOT_DELTA_SECTOR_BUF_SIZE
#define MCUBOOT_DELTA_SECTOR_BUF_SIZE 4096
#endif

#if BOOT_MAX_ALIGN > 1024
#define DELTA_STREAM_BUF_SZ BOOT_MAX_ALIGN
#else
#define DELTA_STREAM_BUF_SZ 1024
#endif

struct boot_delta_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t target_size;
    uint32_t write_size;
    uint32_t record_count;
    uint32_t block_size;
    uint32_t flags;
    uint32_t base_size;
};

struct boot_delta_record {
    uint32_t offset;
    uint32_t size;
};

static uint32_t
boot_delta_align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static bool
boot_delta_is_power_of_two(uint32_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

static int
boot_delta_read_prot_tlv(const struct image_header *hdr,
                         const struct flash_area *fap, uint16_t tlv_type,
                         uint8_t *out, uint16_t out_len)
{
    struct image_tlv_iter it;
    uint32_t off;
    uint16_t len;
    int rc;

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, tlv_type, true);
    if (rc != 0) {
        return rc;
    }

    rc = bootutil_tlv_iter_next(&it, &off, &len, NULL);
    if (rc != 0) {
        return -1;
    }

    if (len != out_len) {
        return -1;
    }

    return LOAD_IMAGE_DATA(hdr, fap, off, out, out_len);
}

static int
boot_delta_image_hash(struct boot_loader_state *state, int slot, uint8_t *hash)
{
    TARGET_STATIC uint8_t tmpbuf[BOOT_TMPBUF_SZ];
    const struct flash_area *fap = BOOT_IMG_AREA(state, slot);
    struct image_header *hdr = boot_img_hdr(state, slot);

    return bootutil_img_hash(state, hdr, fap, tmpbuf, BOOT_TMPBUF_SZ, hash, NULL, 0);
}

static bool
boot_delta_record_fits(uint32_t offset, uint32_t size, uint32_t limit)
{
    return size != 0 && offset <= limit && size <= (limit - offset);
}

static bool
boot_delta_has_restore(const struct boot_delta_header *delta)
{
    return (delta->flags & BOOT_DELTA_F_RESTORE) != 0;
}

static int
boot_delta_copy_record(const struct flash_area *fap_secondary,
                       const struct flash_area *fap_primary,
                       uint32_t data_off, const struct boot_delta_record *rec)
{
    TARGET_STATIC uint8_t buf[DELTA_STREAM_BUF_SZ] __attribute__((aligned(4)));
    uint32_t copied = 0;
    int rc;

    rc = boot_erase_region(fap_primary, rec->offset, rec->size, false);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    while (copied < rec->size) {
        uint32_t chunk = rec->size - copied;

        if (chunk > sizeof(buf)) {
            chunk = sizeof(buf);
        }

        rc = flash_area_read(fap_secondary, data_off + copied, buf, chunk);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        rc = flash_area_write(fap_primary, rec->offset + copied, buf, chunk);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        copied += chunk;
        MCUBOOT_WATCHDOG_FEED();
    }

    return 0;
}

static int
boot_delta_read_record(const struct image_header *patch_hdr,
                       const struct flash_area *fap_secondary,
                       uint32_t payload_end, uint32_t off,
                       const struct boot_delta_header *delta,
                       struct boot_delta_record *rec, uint32_t *new_data_off,
                       uint32_t *old_data_off, uint32_t *next_off)
{
    uint32_t data_size;
    uint32_t data_end;
    int rc;

    (void)patch_hdr;
    (void)delta;

    if (!boot_delta_record_fits(off, sizeof(*rec), payload_end)) {
        return -1;
    }

    rc = LOAD_IMAGE_DATA(patch_hdr, fap_secondary, off, rec, sizeof(*rec));
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    *new_data_off = off + sizeof(*rec);
    if (rec->size > (UINT32_MAX / 2)) {
        return -1;
    }
    data_size = rec->size * 2;
    *old_data_off = *new_data_off + rec->size;

    if (!boot_delta_record_fits(*new_data_off, data_size, payload_end)) {
        return -1;
    }

    data_end = *new_data_off + data_size;
    *next_off = boot_delta_align_up(data_end, 4);

    return 0;
}

static int
boot_delta_record_covers_erase_sectors(const struct flash_area *fap_primary,
                                       const struct boot_delta_record *rec)
{
    uint32_t off = rec->offset;
    uint32_t end = rec->offset + rec->size;
    int rc;

    while (off < end) {
        struct flash_sector sector;
        uint32_t sector_off;
        uint32_t sector_size;

        rc = flash_area_get_sector(fap_primary, off, &sector);
        if (rc != 0) {
            return rc;
        }

        sector_off = flash_sector_get_off(&sector);
        sector_size = flash_sector_get_size(&sector);
        if (sector_size == 0 || off != sector_off ||
            sector_size > (end - sector_off)) {
            return -1;
        }

        off = sector_off + sector_size;
    }

    return off == end ? 0 : -1;
}

static int
boot_delta_validate_record(const struct flash_area *fap_primary,
                           const struct boot_delta_header *delta,
                           uint32_t write_align,
                           const struct boot_delta_record *rec,
                           uint32_t prev_end)
{
    if (!boot_delta_record_fits(rec->offset, rec->size, flash_area_get_size(fap_primary)) ||
        !boot_delta_record_fits(rec->offset, rec->size, delta->write_size)) {
        return -1;
    }

    if (rec->offset < prev_end) {
        return -1;
    }

    if ((rec->offset % write_align) != 0 || (rec->size % write_align) != 0) {
        return -1;
    }

    if (device_requires_erase(fap_primary) &&
        boot_delta_record_covers_erase_sectors(fap_primary, rec) != 0) {
        return -1;
    }

    return 0;
}

static int
boot_delta_validate_records(struct boot_loader_state *state,
                            const struct boot_delta_header *delta)
{
    const struct flash_area *fap_secondary = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
    const struct flash_area *fap_primary = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    const struct image_header *patch_hdr = boot_img_hdr(state, BOOT_SLOT_SECONDARY);
    const uint32_t payload_start = patch_hdr->ih_hdr_size;
    const uint32_t payload_end = payload_start + patch_hdr->ih_img_size;
    const uint32_t write_align = flash_area_align(fap_primary);
    uint32_t off = payload_start + delta->header_size;
    uint32_t prev_end = 0;
    uint32_t i;
    int rc;

    if (write_align == 0 || (delta->block_size % write_align) != 0) {
        return -1;
    }

    for (i = 0; i < delta->record_count; i++) {
        struct boot_delta_record rec;
        uint32_t new_data_off;
        uint32_t old_data_off;
        uint32_t next_off;

        rc = boot_delta_read_record(patch_hdr, fap_secondary, payload_end, off,
                                    delta, &rec, &new_data_off, &old_data_off,
                                    &next_off);
        if (rc != 0) {
            return rc;
        }
        (void)new_data_off;
        (void)old_data_off;

        rc = boot_delta_validate_record(fap_primary, delta, write_align, &rec, prev_end);
        if (rc != 0) {
            return rc;
        }

        prev_end = rec.offset + rec.size;
        off = next_off;
    }

    return off == payload_end ? 0 : -1;
}

static int
boot_delta_apply_records_direct(struct boot_loader_state *state,
                                const struct boot_delta_header *delta,
                                bool restore)
{
    const struct flash_area *fap_secondary = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
    const struct flash_area *fap_primary = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    const struct image_header *patch_hdr = boot_img_hdr(state, BOOT_SLOT_SECONDARY);
    const uint32_t payload_start = patch_hdr->ih_hdr_size;
    const uint32_t payload_end = payload_start + patch_hdr->ih_img_size;
    uint32_t off = payload_start + delta->header_size;
    uint32_t i;
    int rc;

    for (i = 0; i < delta->record_count; i++) {
        struct boot_delta_record rec;
        uint32_t new_data_off;
        uint32_t old_data_off;
        uint32_t next_off;
        uint32_t source_off;

        rc = boot_delta_read_record(patch_hdr, fap_secondary, payload_end, off,
                                    delta, &rec, &new_data_off, &old_data_off,
                                    &next_off);
        if (rc != 0) {
            return rc;
        }

        source_off = restore ? old_data_off : new_data_off;
        rc = boot_delta_copy_record(fap_secondary, fap_primary, source_off, &rec);
        if (rc != 0) {
            return rc;
        }

        off = next_off;
    }

    return 0;
}

static int
boot_delta_overlay_sector(struct boot_loader_state *state,
                          const struct boot_delta_header *delta,
                          uint32_t sector_off, uint32_t sector_size,
                          uint8_t *sector_buf, bool *touched,
                          bool restore)
{
    const struct flash_area *fap_secondary = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
    const struct flash_area *fap_primary = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    const struct image_header *patch_hdr = boot_img_hdr(state, BOOT_SLOT_SECONDARY);
    const uint32_t payload_start = patch_hdr->ih_hdr_size;
    const uint32_t payload_end = payload_start + patch_hdr->ih_img_size;
    const uint32_t sector_end = sector_off + sector_size;
    uint32_t off = payload_start + delta->header_size;
    uint32_t i;
    int rc;

    *touched = false;

    for (i = 0; i < delta->record_count; i++) {
        struct boot_delta_record rec;
        uint32_t new_data_off;
        uint32_t old_data_off;
        uint32_t source_off;
        uint32_t next_off;
        uint32_t rec_end;
        uint32_t overlap_start;
        uint32_t overlap_end;
        uint32_t overlap_len;

        rc = boot_delta_read_record(patch_hdr, fap_secondary, payload_end, off,
                                    delta, &rec, &new_data_off, &old_data_off,
                                    &next_off);
        if (rc != 0) {
            return rc;
        }

        rec_end = rec.offset + rec.size;
        if (rec.offset >= sector_end || rec_end <= sector_off) {
            off = next_off;
            continue;
        }

        if (!*touched) {
            rc = flash_area_read(fap_primary, sector_off, sector_buf, sector_size);
            if (rc != 0) {
                return BOOT_EFLASH;
            }
            *touched = true;
        }

        overlap_start = rec.offset > sector_off ? rec.offset : sector_off;
        overlap_end = rec_end < sector_end ? rec_end : sector_end;
        overlap_len = overlap_end - overlap_start;
        source_off = restore ? old_data_off : new_data_off;

        rc = flash_area_read(fap_secondary,
                             source_off + (overlap_start - rec.offset),
                             sector_buf + (overlap_start - sector_off),
                             overlap_len);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        off = next_off;
    }

    return 0;
}

static int
boot_delta_write_sector(const struct flash_area *fap_primary, uint32_t sector_off,
                        uint32_t sector_size, const uint8_t *sector_buf)
{
    uint32_t written = 0;
    int rc;

    rc = boot_erase_region(fap_primary, sector_off, sector_size, false);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    while (written < sector_size) {
        uint32_t chunk = sector_size - written;

        if (chunk > DELTA_STREAM_BUF_SZ) {
            chunk = DELTA_STREAM_BUF_SZ;
        }

        rc = flash_area_write(fap_primary, sector_off + written,
                              sector_buf + written, chunk);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        written += chunk;
        MCUBOOT_WATCHDOG_FEED();
    }

    return 0;
}

static int
boot_delta_apply_records_with_erase(struct boot_loader_state *state,
                                    const struct boot_delta_header *delta,
                                    bool restore)
{
    TARGET_STATIC uint8_t sector_buf[MCUBOOT_DELTA_SECTOR_BUF_SIZE]
        __attribute__((aligned(4)));
    const struct flash_area *fap_primary = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    uint32_t off = 0;
    int rc;

    while (off < delta->write_size) {
        struct flash_sector sector;
        uint32_t sector_off;
        uint32_t sector_size;
        bool touched;

        rc = flash_area_get_sector(fap_primary, off, &sector);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        sector_off = flash_sector_get_off(&sector);
        sector_size = flash_sector_get_size(&sector);
        if (sector_size == 0 || sector_size > sizeof(sector_buf)) {
            BOOT_LOG_ERR("Delta sector buffer too small: sector=%" PRIu32 ", buffer=%zu",
                         sector_size, sizeof(sector_buf));
            return BOOT_EBADIMAGE;
        }

        rc = boot_delta_overlay_sector(state, delta, sector_off, sector_size,
                                       sector_buf, &touched, restore);
        if (rc != 0) {
            return rc;
        }

        if (touched) {
            rc = boot_delta_write_sector(fap_primary, sector_off, sector_size,
                                         sector_buf);
            if (rc != 0) {
                return rc;
            }
        }

        if (sector_off + sector_size <= off) {
            return BOOT_EBADIMAGE;
        }
        off = sector_off + sector_size;
    }

    return 0;
}

static int
boot_delta_apply_records(struct boot_loader_state *state,
                         const struct boot_delta_header *delta,
                         bool restore)
{
    if (device_requires_erase(BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY))) {
        return boot_delta_apply_records_with_erase(state, delta, restore);
    }

    return boot_delta_apply_records_direct(state, delta, restore);
}

static int
boot_delta_validate_reconstructed_target(struct boot_loader_state *state,
                                         const uint8_t *expected_hash)
{
    TARGET_STATIC uint8_t tmpbuf[BOOT_TMPBUF_SZ];
    uint8_t hash[IMAGE_HASH_SIZE];
    struct image_header *hdr = boot_img_hdr(state, BOOT_SLOT_PRIMARY);
    const struct flash_area *fap = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    int rc;

    rc = boot_read_image_header(state, BOOT_SLOT_PRIMARY, hdr, NULL);
    if (rc != 0 || !boot_check_header_valid(state, BOOT_SLOT_PRIMARY) || IS_DELTA(hdr)) {
        return BOOT_EBADIMAGE;
    }

    rc = boot_delta_image_hash(state, BOOT_SLOT_PRIMARY, hash);
    if (rc != 0) {
        return rc;
    }

    if (memcmp(hash, expected_hash, IMAGE_HASH_SIZE) != 0) {
        BOOT_LOG_ERR("Delta reconstructed image hash mismatch");
        return BOOT_EBADIMAGE;
    }

    FIH_CALL(bootutil_img_validate, fih_rc, state, hdr, fap, tmpbuf, BOOT_TMPBUF_SZ,
             NULL, 0, NULL);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

static int
boot_delta_scramble_slot_trailer(struct boot_loader_state *state, int slot)
{
    const struct flash_area *fap = BOOT_IMG_AREA(state, slot);
    size_t last_sector;

    last_sector = boot_img_num_sectors(state, slot) - 1;
    return boot_scramble_region(fap, boot_img_sector_off(state, slot, last_sector),
                                boot_img_sector_size(state, slot, last_sector), false);
}

static int
boot_delta_scramble_secondary(struct boot_loader_state *state)
{
    const struct flash_area *fap_secondary = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
    int rc;

    rc = boot_scramble_region(fap_secondary,
                              boot_img_sector_off(state, BOOT_SLOT_SECONDARY, 0),
                              boot_img_sector_size(state, BOOT_SLOT_SECONDARY, 0), false);
    if (rc != 0) {
        return rc;
    }

    return boot_delta_scramble_slot_trailer(state, BOOT_SLOT_SECONDARY);
}

static int
boot_delta_read_header(const struct image_header *patch_hdr,
                       const struct flash_area *fap_secondary,
                       struct boot_delta_header *delta)
{
    return LOAD_IMAGE_DATA(patch_hdr, fap_secondary, patch_hdr->ih_hdr_size,
                           delta, BOOT_DELTA_HEADER_SIZE);
}

static int
boot_delta_validate_header(struct boot_loader_state *state,
                           const struct image_header *patch_hdr,
                           const struct boot_delta_header *delta)
{
    uint32_t write_align = flash_area_align(BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY));

    if (delta->magic != BOOT_DELTA_MAGIC ||
        delta->version != BOOT_DELTA_VERSION ||
        delta->target_size == 0 ||
        delta->header_size != BOOT_DELTA_HEADER_SIZE ||
        delta->header_size > patch_hdr->ih_img_size ||
        delta->write_size < delta->target_size ||
        delta->write_size > flash_area_get_size(BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY)) ||
        !boot_delta_is_power_of_two(delta->block_size) ||
        !boot_delta_is_power_of_two(write_align) ||
        (delta->write_size % write_align) != 0 ||
        (delta->flags & ~BOOT_DELTA_F_RESTORE) != 0 ||
        !boot_delta_has_restore(delta) ||
        delta->base_size == 0 ||
        delta->write_size < delta->base_size) {
        return BOOT_EBADIMAGE;
    }

    return 0;
}

static bool
boot_delta_should_stage_restore(const struct boot_delta_header *delta,
                                const struct boot_status *bs)
{
    return bs->swap_type == BOOT_SWAP_TYPE_TEST && boot_delta_has_restore(delta);
}

static int
boot_delta_stage_restore(struct boot_loader_state *state)
{
    const struct flash_area *fap_primary = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    int rc;

    rc = boot_delta_scramble_slot_trailer(state, BOOT_SLOT_SECONDARY);
    if (rc != 0) {
        return rc;
    }

    rc = boot_delta_scramble_slot_trailer(state, BOOT_SLOT_PRIMARY);
    if (rc != 0) {
        return rc;
    }

    rc = boot_write_magic(fap_primary);
    if (rc != 0) {
        return rc;
    }

    return boot_write_copy_done(fap_primary);
}

static int
boot_delta_finish_restore(struct boot_loader_state *state)
{
    int rc;

    rc = boot_delta_scramble_slot_trailer(state, BOOT_SLOT_PRIMARY);
    if (rc != 0) {
        return rc;
    }

    return boot_delta_scramble_secondary(state);
}

int
boot_delta_apply(struct boot_loader_state *state, struct boot_status *bs)
{
    const struct flash_area *fap_secondary = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
    const struct image_header *patch_hdr = boot_img_hdr(state, BOOT_SLOT_SECONDARY);
    struct boot_delta_header delta;
    uint8_t expected_base_hash[IMAGE_HASH_SIZE];
    uint8_t expected_target_hash[IMAGE_HASH_SIZE];
    uint8_t actual_primary_hash[IMAGE_HASH_SIZE];
    const uint8_t *expected_start_hash;
    const uint8_t *expected_end_hash;
    uint32_t image_size;
    bool restore = false;
    bool stage_restore = false;
    bool target_ready = false;
    int rc;

    if (!IS_DELTA(patch_hdr)) {
        return BOOT_EBADIMAGE;
    }

    restore = bs->swap_type == BOOT_SWAP_TYPE_REVERT;
    BOOT_LOG_INF("Image %d %s delta secondary slot -> primary slot",
                 BOOT_CURR_IMG(state), restore ? "restoring" : "applying");

    rc = boot_delta_read_prot_tlv(patch_hdr, fap_secondary,
                                  IMAGE_TLV_DELTA_BASE_SHA,
                                  expected_base_hash, sizeof(expected_base_hash));
    if (rc != 0) {
        BOOT_LOG_ERR("Delta base hash TLV missing");
        return BOOT_EBADIMAGE;
    }

    rc = boot_delta_read_prot_tlv(patch_hdr, fap_secondary,
                                  IMAGE_TLV_DELTA_TARGET_SHA,
                                  expected_target_hash, sizeof(expected_target_hash));
    if (rc != 0) {
        BOOT_LOG_ERR("Delta target hash TLV missing");
        return BOOT_EBADIMAGE;
    }

    if (patch_hdr->ih_img_size < BOOT_DELTA_HEADER_SIZE) {
        BOOT_LOG_ERR("Delta payload too small");
        return BOOT_EBADIMAGE;
    }

    rc = boot_delta_read_header(patch_hdr, fap_secondary, &delta);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = boot_delta_validate_header(state, patch_hdr, &delta);
    if (rc != 0) {
        BOOT_LOG_ERR("Invalid delta header");
        return BOOT_EBADIMAGE;
    }

    if (restore && !boot_delta_has_restore(&delta)) {
        BOOT_LOG_ERR("Delta restore data missing");
        return BOOT_EBADIMAGE;
    }
    stage_restore = boot_delta_should_stage_restore(&delta, bs);

    rc = boot_delta_validate_records(state, &delta);
    if (rc != 0) {
        BOOT_LOG_ERR("Invalid delta records");
        return BOOT_EBADIMAGE;
    }

    rc = boot_delta_image_hash(state, BOOT_SLOT_PRIMARY, actual_primary_hash);
    if (rc != 0) {
        return rc;
    }

    expected_start_hash = restore ? expected_target_hash : expected_base_hash;
    expected_end_hash = restore ? expected_base_hash : expected_target_hash;

    if (memcmp(actual_primary_hash, expected_end_hash, IMAGE_HASH_SIZE) == 0) {
        BOOT_LOG_INF("Delta target image already reconstructed");
        target_ready = true;
    } else if (memcmp(actual_primary_hash, expected_start_hash, IMAGE_HASH_SIZE) != 0) {
        BOOT_LOG_INF("Delta interrupted; restoring base image before retry");
        rc = boot_delta_apply_records(state, &delta, true);
        if (rc != 0) {
            return rc;
        }

        rc = boot_delta_validate_reconstructed_target(state, expected_base_hash);
        if (rc != 0) {
            BOOT_LOG_ERR("Delta base restore failed");
            return rc;
        }

        if (restore) {
            target_ready = true;
        }
    }

    if (!target_ready) {
        rc = boot_delta_apply_records(state, &delta, restore);
        if (rc != 0) {
            return rc;
        }
    }

    rc = boot_delta_validate_reconstructed_target(state, expected_end_hash);
    if (rc != 0) {
        return rc;
    }

#ifdef MCUBOOT_HW_ROLLBACK_PROT
    if (!restore && !stage_restore) {
        rc = boot_update_security_counter(state, BOOT_SLOT_PRIMARY, BOOT_SLOT_PRIMARY);
        if (rc != 0) {
            BOOT_LOG_ERR("Security counter update failed after delta update: %d", rc);
            return rc;
        }
    }
#endif

    if (restore) {
        rc = boot_delta_finish_restore(state);
    } else if (stage_restore) {
        rc = boot_delta_stage_restore(state);
    } else {
        rc = boot_delta_scramble_secondary(state);
    }
    if (rc != 0) {
        return rc;
    }

#if defined(MCUBOOT_OVERWRITE_ONLY) && !defined(MCUBOOT_OVERWRITE_ONLY_FAST)
    image_size = restore ? delta.base_size : delta.target_size;
#else
    rc = boot_read_image_size(state, BOOT_SLOT_PRIMARY, &image_size);
    if (rc != 0) {
        image_size = restore ? delta.base_size : delta.target_size;
    }
#endif

    (void)image_size;

    return BOOT_HOOK_CALL(boot_copy_region_post_hook, 0, BOOT_CURR_IMG(state),
                          BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY), image_size);
}

#endif /* MCUBOOT_DELTA_DFU */
