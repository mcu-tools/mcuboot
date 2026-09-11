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
#define BOOT_DELTA_VERSION 1
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
boot_delta_image_end(const struct image_header *hdr,
                     const struct flash_area *fap, uint32_t *image_end)
{
    struct image_tlv_iter it = {0};
    int rc;

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_DELTA_BASE_SHA, true);
    if (rc != 0) {
        return rc;
    }

    *image_end = it.tlv_end;
    return 0;
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
    if (write_align == 0) {
        return -1;
    }

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
    const uint32_t write_align = flash_area_align(fap_primary);
    uint32_t off = payload_start + delta->header_size;
    uint32_t prev_end = 0;
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

        rc = boot_delta_validate_record(fap_primary, delta, write_align, &rec, prev_end);
        if (rc != 0) {
            return BOOT_EBADIMAGE;
        }
        prev_end = rec.offset + rec.size;

        source_off = restore ? old_data_off : new_data_off;
        rc = boot_delta_copy_record(fap_secondary, fap_primary, source_off, &rec);
        if (rc != 0) {
            return rc;
        }

        off = next_off;
    }

    return off == payload_end ? 0 : BOOT_EBADIMAGE;
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
    const uint32_t write_align = flash_area_align(fap_primary);
    uint32_t off = payload_start + delta->header_size;
    uint32_t prev_end = 0;
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

        rc = boot_delta_validate_record(fap_primary, delta, write_align, &rec, prev_end);
        if (rc != 0) {
            return BOOT_EBADIMAGE;
        }
        rec_end = rec.offset + rec.size;
        prev_end = rec_end;

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

    return off == payload_end ? 0 : BOOT_EBADIMAGE;
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
                                    uint8_t *sector_buf,
                                    bool restore)
{
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
        if (sector_size == 0 || sector_size > MCUBOOT_DELTA_SECTOR_BUF_SIZE) {
            BOOT_LOG_ERR("Delta sector buffer too small: sector=%" PRIu32 ", buffer=%zu",
                         sector_size, (size_t)MCUBOOT_DELTA_SECTOR_BUF_SIZE);
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
                         uint8_t *sector_buf,
                         bool restore)
{
    if (device_requires_erase(BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY))) {
        return boot_delta_apply_records_with_erase(state, delta, sector_buf, restore);
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
boot_delta_clear_primary_trailer(struct boot_loader_state *state,
                                 uint8_t *sector_buf)
{
    const struct flash_area *fap = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    const uint32_t area_size = flash_area_get_size(fap);
    const uint32_t trailer_off = boot_swap_info_off(fap) + BOOT_MAX_ALIGN;
    struct flash_sector sector;
    uint32_t sector_off;
    uint32_t sector_size;
    uint32_t preserved_size;
    int rc;

    if (!device_requires_erase(fap)) {
        return boot_scramble_region(fap, trailer_off, area_size - trailer_off, false);
    }

    rc = flash_area_get_sector(fap, trailer_off, &sector);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    sector_off = flash_sector_get_off(&sector);
    sector_size = flash_sector_get_size(&sector);
    if (sector_size == 0 || sector_size > MCUBOOT_DELTA_SECTOR_BUF_SIZE ||
        sector_off > trailer_off || sector_size > (area_size - sector_off)) {
        return BOOT_EBADIMAGE;
    }

    preserved_size = trailer_off - sector_off;
    if (preserved_size > sector_size ||
        (area_size - trailer_off) > (sector_size - preserved_size)) {
        return BOOT_EBADIMAGE;
    }

    rc = flash_area_read(fap, sector_off, sector_buf, sector_size);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    rc = flash_area_erase(fap, sector_off, sector_size);
    if (rc != 0) {
        return BOOT_EFLASH;
    }

    if (preserved_size == 0) {
        return 0;
    }

    rc = flash_area_write(fap, sector_off, sector_buf, preserved_size);
    return rc == 0 ? 0 : BOOT_EFLASH;
}

static int
boot_delta_clear_secondary_trailer(struct boot_loader_state *state)
{
    const struct flash_area *fap = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
    const uint32_t area_size = flash_area_get_size(fap);
    const uint32_t trailer_off = boot_swap_info_off(fap) + BOOT_MAX_ALIGN;
    size_t last_sector;

    if (!device_requires_erase(fap)) {
        return boot_scramble_region(fap, trailer_off, area_size - trailer_off, false);
    }

    last_sector = boot_img_num_sectors(state, BOOT_SLOT_SECONDARY) - 1;
    return boot_scramble_region(fap,
                                boot_img_sector_off(state, BOOT_SLOT_SECONDARY,
                                                    last_sector),
                                boot_img_sector_size(state, BOOT_SLOT_SECONDARY,
                                                     last_sector),
                                false);
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

    return boot_delta_clear_secondary_trailer(state);
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

static int
boot_delta_validate_patch_storage(struct boot_loader_state *state,
                                  const struct image_header *patch_hdr,
                                  const struct boot_delta_header *delta)
{
    const struct flash_area *fap_primary =
        BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    const struct flash_area *fap_secondary =
        BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
    uint32_t image_end;
    uint32_t primary_limit;
    uint32_t secondary_limit;
    size_t last_sector;
    int rc;

    if (device_requires_erase(fap_primary)) {
        if (boot_img_num_sectors(state, BOOT_SLOT_PRIMARY) == 0) {
            return BOOT_EBADIMAGE;
        }

        last_sector = boot_img_num_sectors(state, BOOT_SLOT_PRIMARY) - 1;
        primary_limit =
            boot_img_sector_off(state, BOOT_SLOT_PRIMARY, last_sector);
    } else {
        primary_limit = boot_swap_info_off(fap_primary);
    }
    if (delta->write_size > primary_limit) {
        return BOOT_EBADIMAGE;
    }

    rc = boot_delta_image_end(patch_hdr, fap_secondary, &image_end);
    if (rc != 0) {
        return BOOT_EBADIMAGE;
    }

    if (device_requires_erase(fap_secondary)) {
        if (boot_img_num_sectors(state, BOOT_SLOT_SECONDARY) == 0) {
            return BOOT_EBADIMAGE;
        }

        last_sector = boot_img_num_sectors(state, BOOT_SLOT_SECONDARY) - 1;
        secondary_limit =
            boot_img_sector_off(state, BOOT_SLOT_SECONDARY, last_sector);
    } else {
        secondary_limit = boot_swap_info_off(fap_secondary);
    }

    return image_end <= secondary_limit ? 0 : BOOT_EBADIMAGE;
}

static bool
boot_delta_should_stage_restore(const struct boot_delta_header *delta,
                                const struct boot_status *bs)
{
    return bs->swap_type == BOOT_SWAP_TYPE_TEST && boot_delta_has_restore(delta);
}

static int
boot_delta_stage_restore(struct boot_loader_state *state, uint8_t *sector_buf)
{
    const struct flash_area *fap_primary = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    int rc;

    /*
     * Keep the secondary TEST marker intact until the primary REVERT marker is
     * durable. A reset before the final secondary cleanup therefore retries the
     * forward delta instead of losing both recovery states.
     */
    rc = boot_delta_clear_primary_trailer(state, sector_buf);
    if (rc != 0) {
        return rc;
    }

    rc = boot_write_magic(fap_primary);
    if (rc != 0) {
        return rc;
    }

    rc = boot_write_copy_done(fap_primary);
    if (rc != 0) {
        return rc;
    }

    return boot_delta_clear_secondary_trailer(state);
}

static int
boot_delta_finish_restore(struct boot_loader_state *state)
{
    const struct flash_area *fap_primary =
        BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    int rc;

    /*
     * Commit the restored base by setting image_ok. This is an aligned trailer
     * write and does not erase image bytes from the primary's final sector.
     */
    rc = boot_write_image_ok(fap_primary);
    if (rc != 0) {
        return rc;
    }

    return boot_delta_scramble_secondary(state);
}

int
boot_delta_apply(struct boot_loader_state *state, struct boot_status *bs)
{
    TARGET_STATIC uint8_t sector_buf[MCUBOOT_DELTA_SECTOR_BUF_SIZE]
        __attribute__((aligned(4)));
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
    bool primary_hashed;
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
                                  IMAGE_TLV_OUTPUT_SHA,
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

    rc = boot_delta_validate_patch_storage(state, patch_hdr, &delta);
    if (rc != 0) {
        BOOT_LOG_ERR("Delta image data overlaps a trailer sector");
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

    /* Probe what the primary slot currently holds.  The hash is taken over the
     * span its own header describes, and an apply interrupted while the first
     * sector was being rewritten can leave that header erased or half written,
     * in which case the span is not hashable at all.  That is not an error: it
     * means the slot is in neither the start nor the end state, and the restore
     * path below is what recovers it.
     */
    rc = boot_delta_image_hash(state, BOOT_SLOT_PRIMARY, actual_primary_hash);
    primary_hashed = (rc == 0);
    if (!primary_hashed) {
        BOOT_LOG_INF("Delta primary slot header unusable; treating as interrupted");
    }

    expected_start_hash = restore ? expected_target_hash : expected_base_hash;
    expected_end_hash = restore ? expected_base_hash : expected_target_hash;

    if (primary_hashed &&
        memcmp(actual_primary_hash, expected_end_hash, IMAGE_HASH_SIZE) == 0) {
        rc = boot_delta_validate_reconstructed_target(state, expected_end_hash);
        if (rc == 0) {
            BOOT_LOG_INF("Delta target image already reconstructed");
            target_ready = true;
        } else {
            BOOT_LOG_INF("Delta image hash complete but validation failed; rewriting records");
        }
    } else if (!primary_hashed ||
               memcmp(actual_primary_hash, expected_start_hash, IMAGE_HASH_SIZE) != 0) {
        BOOT_LOG_INF("Delta interrupted; restoring base image before retry");
        rc = boot_delta_apply_records(state, &delta, sector_buf, true);
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
        rc = boot_delta_apply_records(state, &delta, sector_buf, restore);
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
        rc = boot_delta_stage_restore(state, sector_buf);
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
