/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG
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

/*
 * Fingerprint-based swap status tracking.
 *
 * Pre-computes SHA256 fingerprint tables at init time, verifies each
 * copy operation against the table, and uses the tables on recovery
 * to determine swap resume point.  Content-only hashing (no address
 * binding) enables verification across primary, secondary, and scratch.
 */

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SWAP_FINGERPRINT

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>

#include "bootutil/bootutil.h"
#include "bootutil_priv.h"
#include "swap_priv.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/crypto/sha.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

/*
 * Static offset helpers — fixed offsets from the start of the
 * dedicated fingerprint partition (FLASH_AREA_IMAGE_FINGERPRINT).
 */
static inline uint32_t fp_pri_table_off(void) { return 0; }

static inline uint32_t fp_sec_table_off(void)
{
    return BOOT_FINGERPRINT_MAX_ENTRIES * FINGERPRINT_HASH_SZ;
}

static inline uint32_t fp_table_checksum_off(void)
{
    return BOOT_FINGERPRINT_MAX_ENTRIES * FINGERPRINT_HASH_SZ * 2;
}

static inline uint32_t fp_step_count_off(void)
{
    return fp_table_checksum_off() + FINGERPRINT_TABLE_CHECKSUM_SZ;
}

static inline uint32_t fp_magic_off(void)
{
    return fp_step_count_off() + BOOT_MAX_ALIGN;
}

/*
 * Partition open/close helpers for the dedicated fingerprint area.
 */
static int fp_open(const struct flash_area **fap_fp)
{
    return flash_area_open(FLASH_AREA_IMAGE_FINGERPRINT, fap_fp);
}

static void fp_close(const struct flash_area *fap_fp)
{
    flash_area_close(fap_fp);
}

/*
 * MAGIC write/check helpers for the fingerprint partition.
 *
 * boot_write_magic() writes at flash_area_get_size(fap) - BOOT_MAGIC_SZ
 * (end of flash area), which is wrong for the fingerprint partition.
 * We write MAGIC at the fixed fp_magic_off() instead.
 */
static int fp_write_magic(const struct flash_area *fap_fp)
{
    return flash_area_write(fap_fp, fp_magic_off(),
                            BOOT_IMG_MAGIC, BOOT_MAGIC_SZ);
}

static int fp_check_magic(const struct flash_area *fap_fp)
{
    uint8_t magic[BOOT_MAGIC_SZ];
    int rc;

    rc = flash_area_read(fap_fp, fp_magic_off(), magic, BOOT_MAGIC_SZ);
    if (rc != 0) {
        return -1;
    }
    if (memcmp(magic, BOOT_IMG_MAGIC, BOOT_MAGIC_SZ) != 0) {
        return -1;
    }
    return 0;
}

/*
 * Content-only SHA256 hash of a sector.
 *
 * Computes SHA256(sector_data) with no address or device binding so the
 * same hash can verify data regardless of location (primary, secondary,
 * or scratch).
 */
static int
fingerprint_hash_sector(const struct flash_area *fap,
                        uint32_t read_off,
                        uint32_t sz,
                        uint8_t *out_hash)
{
    bootutil_sha_context ctx;
    uint8_t buf[BOOT_TMPBUF_SZ];
    uint32_t remain;
    uint32_t chunk;
    int rc;

    bootutil_sha_init(&ctx);

    remain = sz;
    while (remain > 0) {
        chunk = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        rc = flash_area_read(fap, read_off + (sz - remain), buf, chunk);
        if (rc != 0) {
            bootutil_sha_drop(&ctx);
            return rc;
        }
        bootutil_sha_update(&ctx, buf, chunk);
        remain -= chunk;
    }

    bootutil_sha_finish(&ctx, out_hash);
    bootutil_sha_drop(&ctx);
    return 0;
}

/*
 * Trailer I/O helpers for fingerprint metadata.
 */
static int
fingerprint_write_step_count(const struct flash_area *fap, uint32_t count)
{
    uint32_t off = fp_step_count_off();

    return boot_write_trailer(fap, off, (const uint8_t *)&count, 4);
}

static int
fingerprint_read_step_count(const struct flash_area *fap, uint32_t *count)
{
    uint32_t off = fp_step_count_off();
    int rc = flash_area_read(fap, off, count, sizeof(*count));

    if (rc != 0) {
        return BOOT_EFLASH;
    }
    return 0;
}

static int
fingerprint_read_entry(const struct flash_area *fap, uint32_t table_base_off,
                       uint32_t index, uint8_t *out_hash)
{
    uint32_t off = table_base_off + index * FINGERPRINT_HASH_SZ;
    int rc = flash_area_read(fap, off, out_hash, FINGERPRINT_HASH_SZ);

    if (rc != 0) {
        return BOOT_EFLASH;
    }
    return 0;
}

/*
 * Compute a checksum over both fingerprint tables (primary and secondary)
 * to verify table integrity on recovery.
 */
static int
fingerprint_compute_table_checksum(const struct flash_area *fap,
                                   uint32_t num_entries,
                                   uint8_t *out_hash)
{
    bootutil_sha_context ctx;
    uint8_t buf[BOOT_TMPBUF_SZ];
    uint32_t total_sz;
    uint32_t off;
    uint32_t remain;
    uint32_t chunk;
    int rc;

    bootutil_sha_init(&ctx);

    off = fp_pri_table_off();
    total_sz = num_entries * FINGERPRINT_HASH_SZ * 2;
    remain = total_sz;

    while (remain > 0) {
        chunk = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        rc = flash_area_read(fap, off + (total_sz - remain), buf, chunk);
        if (rc != 0) {
            bootutil_sha_drop(&ctx);
            return rc;
        }
        bootutil_sha_update(&ctx, buf, chunk);
        remain -= chunk;
    }

    bootutil_sha_finish(&ctx, out_hash);
    bootutil_sha_drop(&ctx);
    return 0;
}

/*
 * swap_status_init — pre-compute fingerprint tables and write swap metadata.
 *
 * Pre-computes two tables in the dedicated fingerprint partition:
 *   primary_table[i]   = SHA256(secondary_data[i])
 *   secondary_table[i] = SHA256(primary_data[i])
 *
 * Then writes table checksum, step count, and MAGIC to the fingerprint
 * partition.  Standard swap metadata (swap_info, swap_size, image_ok,
 * boot_write_magic) still goes to `fap` as before.
 *
 * In SCRATCH swap, swap_status_init is called in two flows:
 *
 *   !use_scratch (trailer does NOT overlap image data):
 *     1. fap = scratch — skip table write
 *     2. swap_scramble_trailer_sectors(primary)
 *     3. fap = primary — WRITE tables (post-scramble data)
 *
 *   use_scratch (trailer overlaps image data):
 *     1. fap = scratch — WRITE tables (only call, no scramble happens)
 *
 * The fingerprint partition is erased by boot_swap_image() at the start
 * of each fresh swap, ensuring stale MAGIC from a prior swap is cleared.
 */
int
swap_status_init(const struct boot_loader_state *state,
                 const struct flash_area *fap,
                 const struct boot_status *bs)
{
    const struct flash_area *fap_pri;
    const struct flash_area *fap_sec;
    const struct flash_area *fap_fp = NULL;
    struct boot_swap_state swap_state;
    uint8_t image_index;
    uint32_t num_sectors;
    uint32_t sector_sz;
    uint32_t off;
    uint32_t tbl_off;
    uint8_t hash[FINGERPRINT_HASH_SZ];
    uint8_t checksum[FINGERPRINT_HASH_SZ];
    uint32_t i;
    int rc;

    image_index = BOOT_CURR_IMG(state);

    fap_pri = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    fap_sec = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);

    BOOT_LOG_DBG("fingerprint: swap_status_init; fa_id=%d",
                 flash_area_get_id(fap));

    sector_sz = boot_img_sector_size(state, BOOT_SLOT_PRIMARY, 0);
    num_sectors = (bs->swap_size + sector_sz - 1) / sector_sz;

    BOOT_LOG_DBG("fingerprint: swap_size=%lu sector_sz=%lu num_sectors=%lu",
                 (unsigned long)bs->swap_size,
                 (unsigned long)sector_sz,
                 (unsigned long)num_sectors);

    if (num_sectors > BOOT_FINGERPRINT_MAX_ENTRIES) {
        return BOOT_EBADARGS;
    }

    /* Write fingerprint tables at the correct point in the swap init.
     *
     * !use_scratch (trailer does NOT overlap image data):
     *   1. swap_status_init(scratch) — skip (pre-scramble data)
     *   2. swap_scramble_trailer_sectors(primary)
     *   3. swap_status_init(primary) — WRITE (post-scramble matches copies)
     *
     * use_scratch (trailer overlaps image data):
     *   1. swap_status_init(scratch) — WRITE (only call, no scramble)
     *
     * The fap==primary check handles the !use_scratch path.
     * The bs->use_scratch check handles the use_scratch path.
     */
    if (flash_area_get_id(fap) == FLASH_AREA_IMAGE_PRIMARY(image_index) ||
        bs->use_scratch) {

        rc = fp_open(&fap_fp);
        if (rc != 0) {
            return BOOT_EFLASH;
        }

        /* Erase the entire fingerprint partition */
        rc = flash_area_erase(fap_fp, 0, flash_area_get_size(fap_fp));
        if (rc != 0) {
            fp_close(fap_fp);
            return BOOT_EFLASH;
        }

        for (i = 0; i < num_sectors; i++) {
            uint32_t hash_sz = sector_sz;

            off = boot_img_sector_off(state, BOOT_SLOT_PRIMARY, i);

            /* For the last sector in use_scratch mode, only copy_sz bytes
             * are moved (the rest is trailer area).  Hash only the data
             * portion so that verification and recovery match what's
             * actually copied.
             */
            if (bs->use_scratch && i == num_sectors - 1) {
                uint32_t trailer_sz_init = boot_trailer_sz(
                                            BOOT_WRITE_SZ(state));
                uint32_t slot_sz_init = flash_area_get_size(fap_pri);
                uint32_t copy_sz_init = slot_sz_init - off - trailer_sz_init;

                if (copy_sz_init < sector_sz) {
                    hash_sz = copy_sz_init;
                }
            }

            /* Primary table: SHA256(secondary_data[i]) —
             * what primary[i] should contain after swap */
            rc = fingerprint_hash_sector(fap_sec, off, hash_sz, hash);
            if (rc != 0) {
                fp_close(fap_fp);
                return rc;
            }

            tbl_off = fp_pri_table_off() + i * FINGERPRINT_HASH_SZ;
            rc = flash_area_write(fap_fp, tbl_off, hash, FINGERPRINT_HASH_SZ);
            if (rc != 0) {
                fp_close(fap_fp);
                return BOOT_EFLASH;
            }

            /* Secondary table: SHA256(primary_data[i]) —
             * what secondary[i] should contain after swap */
            rc = fingerprint_hash_sector(fap_pri, off, hash_sz, hash);
            if (rc != 0) {
                fp_close(fap_fp);
                return rc;
            }

            tbl_off = fp_sec_table_off() + i * FINGERPRINT_HASH_SZ;
            rc = flash_area_write(fap_fp, tbl_off, hash, FINGERPRINT_HASH_SZ);
            if (rc != 0) {
                fp_close(fap_fp);
                return BOOT_EFLASH;
            }
        }

        /* Table checksum: SHA256(primary_table || secondary_table) */
        rc = fingerprint_compute_table_checksum(fap_fp, num_sectors,
                                                checksum);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }

        tbl_off = fp_table_checksum_off();
        rc = flash_area_write(fap_fp, tbl_off, checksum,
                              FINGERPRINT_TABLE_CHECKSUM_SZ);
        if (rc != 0) {
            fp_close(fap_fp);
            return BOOT_EFLASH;
        }

        /* Step count */
        rc = fingerprint_write_step_count(fap_fp, num_sectors);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }

        /* Fingerprint MAGIC — written last as atomic commit marker */
        rc = fp_write_magic(fap_fp);
        if (rc != 0) {
            fp_close(fap_fp);
            return BOOT_EFLASH;
        }
        fp_close(fap_fp);
    }

    /* Standard swap metadata — same sequence as original swap_status_init */
    rc = boot_read_swap_state(
            state->imgs[image_index][BOOT_SLOT_SECONDARY].area,
            &swap_state);
    assert(rc == 0);

    if (bs->swap_type != BOOT_SWAP_TYPE_NONE) {
        rc = boot_write_swap_info(fap, bs->swap_type, image_index);
        assert(rc == 0);
    }

    if (swap_state.image_ok == BOOT_FLAG_SET) {
        rc = boot_write_image_ok(fap);
        assert(rc == 0);
    }

    rc = boot_write_swap_size(fap, bs->swap_size);
    assert(rc == 0);

#ifdef MCUBOOT_SWAP_USING_OFFSET
    rc = boot_write_unprotected_tlv_sizes(fap,
            BOOT_IMG_UNPROTECTED_TLV_SIZE(state, BOOT_SLOT_PRIMARY),
            BOOT_IMG_UNPROTECTED_TLV_SIZE(state, BOOT_SLOT_SECONDARY));
    assert(rc == 0);
#endif

#ifdef MCUBOOT_ENC_IMAGES
    rc = boot_write_enc_keys(fap, bs);
#endif

    /* Standard trailer MAGIC written last */
    rc = boot_write_magic(fap);
    assert(rc == 0);

    return 0;
}

/*
 * boot_write_status — verified copy.
 *
 * Called after each state transition during swap_run.  Hashes the
 * destination of the just-completed copy and compares against the
 * fingerprint table in the dedicated partition.  Returns 0 on match,
 * BOOT_EBADIMAGE on mismatch (fatal — swap aborted).
 *
 * For SCRATCH swap:
 *   STATE_0: secondary[i] copied to scratch
 *            -> verify scratch matches primary_table[sector_idx]
 *            (scratch now holds original secondary data)
 *   STATE_1: primary[i] copied to secondary[i]
 *            -> verify secondary[i] matches secondary_table[sector_idx]
 *            (secondary now holds original primary data)
 *   STATE_2: scratch copied to primary[i]
 *            -> verify primary[i] matches primary_table[sector_idx]
 *            (primary now holds original secondary data)
 */
int
boot_write_status(const struct boot_loader_state *state,
                  struct boot_status *bs)
{
    const struct flash_area *fap_pri;
    const struct flash_area *fap_fp = NULL;
    const struct flash_area *fap_dest;
    uint32_t sector_sz;
    uint32_t num_sectors;
    uint32_t swap_idx;
    uint32_t sector_idx;
    uint32_t off;
    uint8_t hash_live[FINGERPRINT_HASH_SZ];
    uint8_t hash_tbl[FINGERPRINT_HASH_SZ];
    int rc;

    fap_pri = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    sector_sz = boot_img_sector_size(state, BOOT_SLOT_PRIMARY, 0);
    num_sectors = (bs->swap_size + sector_sz - 1) / sector_sz;

    /* Open the fingerprint partition */
    rc = fp_open(&fap_fp);
    if (rc != 0) {
        return 0; /* No fingerprint partition — skip verification */
    }

    /* Validate by reading the step count: if it doesn't match
     * num_sectors, tables are absent and verification must be skipped. */
    {
        uint32_t stored_steps;
        rc = fingerprint_read_step_count(fap_fp, &stored_steps);
        if (rc != 0 || stored_steps != num_sectors) {
            fp_close(fap_fp);
            return 0;
        }
    }

    /* Convert swap_idx back to physical sector index.
     * Swap processes sectors from last to first:
     *   swap_idx 0 -> sector N-1
     *   swap_idx 1 -> sector N-2
     *   swap_idx K -> sector N-1-K
     */
    swap_idx = bs->idx - BOOT_STATUS_IDX_0;
    sector_idx = (num_sectors - 1) - swap_idx;
    off = sector_idx * sector_sz;

    /* For the partial-copy sector (last physical sector overlapping the
     * trailer), use the reduced hash size that matches what was stored
     * in the fingerprint table during init.
     */
    if (swap_idx == 0) {
        uint32_t trailer_sz_v = boot_trailer_sz(BOOT_WRITE_SZ(state));
        uint32_t slot_sz_v = flash_area_get_size(fap_pri);

        if (off + sector_sz > slot_sz_v - trailer_sz_v) {
            sector_sz = slot_sz_v - off - trailer_sz_v;
        }
    }

    switch (bs->state) {
    case BOOT_STATUS_STATE_0:
        /* STATE_0: secondary[i] copied to scratch; scratch should contain
         * original secondary data, matching primary_table[sector_idx]
         * (primary_table = SHA256(secondary_data)). */
#if MCUBOOT_SWAP_USING_SCRATCH
        fap_dest = state->scratch.area;
        rc = fingerprint_hash_sector(fap_dest, 0, sector_sz, hash_live);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }
        rc = fingerprint_read_entry(fap_fp, fp_pri_table_off(),
                                    sector_idx, hash_tbl);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }
#else
        fp_close(fap_fp);
        return 0;
#endif
        break;

    case BOOT_STATUS_STATE_1:
        /* STATE_1: primary[i] copied to secondary[i]; secondary should contain
         * original primary data, matching secondary_table[sector_idx]
         * (secondary_table = SHA256(primary_data)). */
        fap_dest = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);
        rc = fingerprint_hash_sector(fap_dest, off, sector_sz, hash_live);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }
        rc = fingerprint_read_entry(fap_fp, fp_sec_table_off(),
                                    sector_idx, hash_tbl);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }
        break;

    case BOOT_STATUS_STATE_2:
        /* STATE_2: scratch copied to primary[i]; primary should contain
         * original secondary data, matching primary_table[sector_idx]
         * (primary_table = SHA256(secondary_data)). */
        rc = fingerprint_hash_sector(fap_pri, off, sector_sz, hash_live);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }
        rc = fingerprint_read_entry(fap_fp, fp_pri_table_off(),
                                    sector_idx, hash_tbl);
        if (rc != 0) {
            fp_close(fap_fp);
            return rc;
        }
        break;

    default:
        fp_close(fap_fp);
        return 0;
    }

    fp_close(fap_fp);

    if (memcmp(hash_live, hash_tbl, FINGERPRINT_HASH_SZ) != 0) {
        BOOT_LOG_ERR("fingerprint: copy verification failed; "
                     "sector=%lu state=%d — aborting swap",
                     (unsigned long)sector_idx, bs->state);
        return BOOT_EBADIMAGE;
    }

    return 0;
}

/*
 * swap_read_status_bytes — hash-based recovery with scratch checking.
 *
 * Scans sectors in swap order (last first).  For the interrupted sector,
 * hashes scratch to determine exact state.  Reads fingerprint tables
 * from the dedicated fingerprint partition.
 *
 * SCRATCH swap direction:
 *   STATE_0: secondary[i] → scratch     (backup new/secondary data)
 *   STATE_1: primary[i]   → secondary[i] (move old data to secondary)
 *   STATE_2: scratch      → primary[i]   (put new data in primary)
 *
 * Recovery state machine (6 cases):
 *   1. pri_match AND sec_match               → fully swapped, continue
 *   2. !pri AND !sec AND !scratch            → restart STATE_0
 *   3. !pri AND !sec AND scratch_match       → resume STATE_1
 *   4. !pri AND sec_match AND scratch_match  → resume STATE_2
 *   5. !pri AND sec_match AND !scratch       → scratch lost, error
 *   6. pri_match AND !sec                    → corruption error
 */
int
swap_read_status_bytes(const struct flash_area *fap,
                       struct boot_loader_state *state,
                       struct boot_status *bs)
{
    const struct flash_area *fap_pri;
    const struct flash_area *fap_sec;
    const struct flash_area *fap_fp = NULL;
    uint8_t hash_live[FINGERPRINT_HASH_SZ];
    uint8_t hash_tbl[FINGERPRINT_HASH_SZ];
    uint8_t hash_scratch[FINGERPRINT_HASH_SZ];
    uint8_t hash_sec_entry[FINGERPRINT_HASH_SZ];
    uint8_t checksum_stored[FINGERPRINT_HASH_SZ];
    uint8_t checksum_computed[FINGERPRINT_HASH_SZ];
    uint32_t num_sectors;
    uint32_t sector_sz;
    uint32_t cur_sz;
    uint32_t off;
    uint32_t swap_idx;
    int pri_match;
    int sec_match;
    int scratch_match;
    int32_t i;
    int rc;
    int ret = 0;

    (void)fap;

    fap_pri = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    fap_sec = BOOT_IMG_AREA(state, BOOT_SLOT_SECONDARY);

    /* Open the fingerprint partition */
    rc = fp_open(&fap_fp);
    if (rc != 0) {
        BOOT_LOG_DBG("fingerprint: cannot open partition, returning reset");
        return 0;
    }

    /* Layer 1: MAGIC validation — guarantees init completed */
    if (fp_check_magic(fap_fp) != 0) {
        BOOT_LOG_DBG("fingerprint: MAGIC absent, returning reset");
        goto out;
    }

    /* Read step count */
    rc = fingerprint_read_step_count(fap_fp, &num_sectors);
    if (rc != 0) {
        ret = rc;
        goto out;
    }

    if (num_sectors == 0 || num_sectors > BOOT_FINGERPRINT_MAX_ENTRIES) {
        BOOT_LOG_DBG("fingerprint: invalid step count %lu",
                     (unsigned long)num_sectors);
        goto out;
    }

    /* Layer 2: Table checksum — verifies table integrity */
    rc = fingerprint_compute_table_checksum(fap_fp, num_sectors,
                                            checksum_computed);
    if (rc != 0) {
        ret = rc;
        goto out;
    }

    off = fp_table_checksum_off();
    rc = flash_area_read(fap_fp, off, checksum_stored,
                         FINGERPRINT_TABLE_CHECKSUM_SZ);
    if (rc != 0) {
        ret = BOOT_EFLASH;
        goto out;
    }

    if (memcmp(checksum_stored, checksum_computed,
               FINGERPRINT_TABLE_CHECKSUM_SZ) != 0) {
        BOOT_LOG_DBG("fingerprint: table checksum mismatch, returning reset");
        goto out;
    }

    sector_sz = boot_img_sector_size(state, BOOT_SLOT_PRIMARY, 0);

    BOOT_LOG_DBG("fingerprint: recovery scan; num_sectors=%lu sector_sz=%lu",
                 (unsigned long)num_sectors, (unsigned long)sector_sz);

    /* Scan in swap order: last sector first (matches swap_run iteration) */
    for (i = (int32_t)(num_sectors - 1); i >= 0; i--) {
        swap_idx = (num_sectors - 1) - (uint32_t)i;
        off = (uint32_t)i * sector_sz;

        /* For the partial-copy sector (last physical sector overlapping the
         * trailer), use the reduced hash size that matches what was stored
         * in the fingerprint table during init.
         */
        cur_sz = sector_sz;
        if (swap_idx == 0) {
            uint32_t trailer_sz_r = boot_trailer_sz(BOOT_WRITE_SZ(state));
            uint32_t slot_sz_r = flash_area_get_size(fap_pri);

            if (off + sector_sz > slot_sz_r - trailer_sz_r) {
                cur_sz = slot_sz_r - off - trailer_sz_r;
            }
        }

        /* Hash current primary[i] */
        rc = fingerprint_hash_sector(fap_pri, off, cur_sz, hash_live);
        if (rc != 0) {
            ret = rc;
            goto out;
        }

        rc = fingerprint_read_entry(fap_fp, fp_pri_table_off(),
                                    (uint32_t)i, hash_tbl);
        if (rc != 0) {
            ret = rc;
            goto out;
        }

        pri_match = (memcmp(hash_live, hash_tbl, FINGERPRINT_HASH_SZ) == 0);

        /* Hash current secondary[i] */
        rc = fingerprint_hash_sector(fap_sec, off, cur_sz, hash_live);
        if (rc != 0) {
            ret = rc;
            goto out;
        }

        rc = fingerprint_read_entry(fap_fp, fp_sec_table_off(),
                                    (uint32_t)i, hash_tbl);
        if (rc != 0) {
            ret = rc;
            goto out;
        }

        sec_match = (memcmp(hash_live, hash_tbl, FINGERPRINT_HASH_SZ) == 0);

        if (pri_match && sec_match) {
            BOOT_LOG_DBG("fingerprint: sector %ld fully swapped", (long)i);
            continue;
        }

        /* Found interrupted sector — hash scratch to determine state */
#if MCUBOOT_SWAP_USING_SCRATCH
        {
            const struct flash_area *fap_scratch = state->scratch.area;

            rc = fingerprint_hash_sector(fap_scratch, 0, cur_sz,
                                         hash_scratch);
            if (rc != 0) {
                ret = rc;
                goto out;
            }

            rc = fingerprint_read_entry(fap_fp, fp_pri_table_off(),
                                        (uint32_t)i, hash_sec_entry);
            if (rc != 0) {
                ret = rc;
                goto out;
            }

            scratch_match = (memcmp(hash_scratch, hash_sec_entry,
                                    FINGERPRINT_HASH_SZ) == 0);
        }
#else
        scratch_match = 0;
        (void)hash_scratch;
        (void)hash_sec_entry;
#endif

        if (!pri_match && !sec_match && !scratch_match) {
            /* CASE 2: Scratch corrupted/empty. Restart STATE_0. */
            BOOT_LOG_DBG("fingerprint: sector %ld — restart STATE_0",
                         (long)i);
            bs->idx = swap_idx + BOOT_STATUS_IDX_0;
            bs->state = BOOT_STATUS_STATE_0;
            goto out;
        }

        if (!pri_match && !sec_match && scratch_match) {
            /* CASE 3: Scratch valid, primary not done. Resume STATE_1. */
            BOOT_LOG_DBG("fingerprint: sector %ld — resume STATE_1",
                         (long)i);
            bs->idx = swap_idx + BOOT_STATUS_IDX_0;
            bs->state = BOOT_STATUS_STATE_1;
            goto out;
        }

        if (!pri_match && sec_match && scratch_match) {
            /* CASE 4: STATE_1 completed (secondary has primary's data),
             * scratch still has secondary's data. Resume STATE_2. */
            BOOT_LOG_DBG("fingerprint: sector %ld — resume STATE_2",
                         (long)i);
            bs->idx = swap_idx + BOOT_STATUS_IDX_0;
            bs->state = BOOT_STATUS_STATE_2;
            goto out;
        }

        if (!pri_match && sec_match && !scratch_match) {
            /* CASE 5: STATE_1 completed but scratch data lost.
             * Unrecoverable — cannot complete STATE_2 without scratch. */
            BOOT_LOG_ERR("fingerprint: sector %ld — scratch data lost "
                         "after STATE_1",
                         (long)i);
            ret = BOOT_EBADIMAGE;
            goto out;
        }

        /* CASE 6: pri_match && !sec_match — primary updated but secondary
         * wasn't. Impossible in normal flow since STATE_2 writes primary
         * AFTER STATE_1 writes secondary. */
        BOOT_LOG_ERR("fingerprint: sector %ld — corruption detected "
                     "(pri_match=%d sec_match=%d scratch_match=%d)",
                     (long)i, pri_match, sec_match, scratch_match);
        ret = BOOT_EBADIMAGE;
        goto out;
    }

    /* All sectors match — swap complete */
    BOOT_LOG_DBG("fingerprint: all sectors match, swap complete");

out:
    fp_close(fap_fp);
    return ret;
}

/*
 * boot_status_internal_off — compute offset within the fingerprint
 * status area for a given boot_status index.
 */
uint32_t
boot_status_internal_off(const struct boot_status *bs, int elem_sz)
{
    (void)elem_sz;
    return (bs->idx - BOOT_STATUS_IDX_0) * FINGERPRINT_HASH_SZ;
}

#endif /* MCUBOOT_SWAP_FINGERPRINT */
