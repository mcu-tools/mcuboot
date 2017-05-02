/*
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

#include "boot_test.h"

/** Internal flash layout. */
struct flash_area boot_test_area_descs[] = {
    [0] = { .fa_off = 0x00020000, .fa_size = 128 * 1024 },
    [1] = { .fa_off = 0x00040000, .fa_size = 128 * 1024 },
    [2] = { .fa_off = 0x00060000, .fa_size = 128 * 1024 },
    [3] = { .fa_off = 0x00080000, .fa_size = 128 * 1024 },
    [4] = { .fa_off = 0x000a0000, .fa_size = 128 * 1024 },
    [5] = { .fa_off = 0x000c0000, .fa_size = 128 * 1024 },
    [6] = { .fa_off = 0x000e0000, .fa_size = 128 * 1024 },
    [7] = { 0 },
};

/** Areas representing the beginning of image slots. */
uint8_t boot_test_slot_areas[] = {
    0, 3,
};

/** Flash offsets of the two image slots. */
struct boot_test_img_addrs boot_test_img_addrs[] = {
    { 0, 0x20000 },
    { 0, 0x80000 },
};

#define BOOT_TEST_AREA_IDX_SCRATCH 6

uint8_t
boot_test_util_byte_at(int img_msb, uint32_t image_offset)
{
    uint32_t u32;
    uint8_t *u8p;

    TEST_ASSERT(image_offset < 0x01000000);
    u32 = image_offset + (img_msb << 24);
    u8p = (void *)&u32;
    return u8p[image_offset % 4];
}

uint8_t
boot_test_util_flash_align(void)
{
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    TEST_ASSERT_FATAL(rc == 0);

    return flash_area_align(fap);
}

void
boot_test_util_init_flash(void)
{
    const struct flash_area *area_desc;
    int rc;

    rc = hal_flash_init();
    TEST_ASSERT(rc == 0);

    for (area_desc = boot_test_area_descs;
         area_desc->fa_size != 0;
         area_desc++) {

        rc = flash_area_erase(area_desc, 0, area_desc->fa_size);
        TEST_ASSERT(rc == 0);
    }
}

void
boot_test_util_copy_area(int from_area_idx, int to_area_idx)
{
    const struct flash_area *from_area_desc;
    const struct flash_area *to_area_desc;
    void *buf;
    int rc;

    from_area_desc = boot_test_area_descs + from_area_idx;
    to_area_desc = boot_test_area_descs + to_area_idx;

    TEST_ASSERT(from_area_desc->fa_size == to_area_desc->fa_size);

    buf = malloc(from_area_desc->fa_size);
    TEST_ASSERT(buf != NULL);

    rc = flash_area_read(from_area_desc, 0, buf,
                         from_area_desc->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_erase(to_area_desc,
                          0,
                          to_area_desc->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_write(to_area_desc, 0, buf,
                          to_area_desc->fa_size);
    TEST_ASSERT(rc == 0);

    free(buf);
}

static uint32_t
boot_test_util_area_write_size(int dst_idx, uint32_t off, uint32_t size)
{
    const struct flash_area *desc;
    int64_t diff;
    uint32_t trailer_start;
    uint8_t elem_sz;

    if (dst_idx != BOOT_TEST_AREA_IDX_SCRATCH - 1) {
        return size;
    }

    /* Don't include trailer in copy to second slot. */
    desc = boot_test_area_descs + dst_idx;
    elem_sz = boot_test_util_flash_align();
    trailer_start = desc->fa_size - boot_trailer_sz(elem_sz);
    diff = off + size - trailer_start;
    if (diff > 0) {
        if (diff > size) {
            size = 0;
        } else {
            size -= diff;
        }
    }

    return size;
}

void
boot_test_util_swap_areas(int area_idx1, int area_idx2)
{
    const struct flash_area *area_desc1;
    const struct flash_area *area_desc2;
    uint32_t size;
    void *buf1;
    void *buf2;
    int rc;

    area_desc1 = boot_test_area_descs + area_idx1;
    area_desc2 = boot_test_area_descs + area_idx2;

    TEST_ASSERT(area_desc1->fa_size == area_desc2->fa_size);

    buf1 = malloc(area_desc1->fa_size);
    TEST_ASSERT(buf1 != NULL);

    buf2 = malloc(area_desc2->fa_size);
    TEST_ASSERT(buf2 != NULL);

    rc = flash_area_read(area_desc1, 0, buf1, area_desc1->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_read(area_desc2, 0, buf2, area_desc2->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_erase(area_desc1, 0, area_desc1->fa_size);
    TEST_ASSERT(rc == 0);

    rc = flash_area_erase(area_desc2, 0, area_desc2->fa_size);
    TEST_ASSERT(rc == 0);

    size = boot_test_util_area_write_size(area_idx1, 0, area_desc1->fa_size);
    rc = flash_area_write(area_desc1, 0, buf2, size);
    TEST_ASSERT(rc == 0);

    size = boot_test_util_area_write_size(area_idx2, 0, area_desc2->fa_size);
    rc = flash_area_write(area_desc2, 0, buf1, size);
    TEST_ASSERT(rc == 0);

    free(buf1);
    free(buf2);
}

void
boot_test_util_write_image(const struct image_header *hdr, int slot)
{
    uint32_t image_off;
    uint32_t off;
    uint8_t flash_id;
    uint8_t buf[256];
    int chunk_sz;
    int rc;
    int i;

    TEST_ASSERT(slot == 0 || slot == 1);

    flash_id = boot_test_img_addrs[slot].flash_id;
    off = boot_test_img_addrs[slot].address;

    rc = hal_flash_write(flash_id, off, hdr, sizeof *hdr);
    TEST_ASSERT(rc == 0);

    off += hdr->ih_hdr_size;

    image_off = 0;
    while (image_off < hdr->ih_img_size) {
        if (hdr->ih_img_size - image_off > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = hdr->ih_img_size - image_off;
        }

        for (i = 0; i < chunk_sz; i++) {
            buf[i] = boot_test_util_byte_at(slot, image_off + i);
        }

        rc = hal_flash_write(flash_id, off + image_off, buf, chunk_sz);
        TEST_ASSERT(rc == 0);

        image_off += chunk_sz;
    }
}


void
boot_test_util_write_hash(const struct image_header *hdr, int slot)
{
    uint8_t tmpdata[1024];
    uint8_t hash[32];
    int rc;
    uint32_t off;
    uint32_t blk_sz;
    uint32_t sz;
    mbedtls_sha256_context ctx;
    uint8_t flash_id;
    uint32_t addr;
    struct image_tlv tlv;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    flash_id = boot_test_img_addrs[slot].flash_id;
    addr = boot_test_img_addrs[slot].address;

    sz = hdr->ih_hdr_size + hdr->ih_img_size;
    for (off = 0; off < sz; off += blk_sz) {
        blk_sz = sz - off;
        if (blk_sz > sizeof(tmpdata)) {
            blk_sz = sizeof(tmpdata);
        }
        rc = hal_flash_read(flash_id, addr + off, tmpdata, blk_sz);
        TEST_ASSERT(rc == 0);
        mbedtls_sha256_update(&ctx, tmpdata, blk_sz);
    }
    mbedtls_sha256_finish(&ctx, hash);

    tlv.it_type = IMAGE_TLV_SHA256;
    tlv._pad = 0;
    tlv.it_len = sizeof(hash);

    memcpy(tmpdata, &tlv, sizeof tlv);
    memcpy(tmpdata + sizeof tlv, hash, sizeof hash);
    rc = hal_flash_write(flash_id, addr + off, tmpdata,
                         sizeof tlv + sizeof hash);
    TEST_ASSERT(rc == 0);
}

static void
boot_test_util_write_swap_state(int flash_area_id,
                                const struct boot_swap_state *state)
{
    const struct flash_area *fap;
    int rc;

    rc = flash_area_open(flash_area_id, &fap);
    TEST_ASSERT_FATAL(rc == 0);

    switch (state->magic) {
    case 0:
        break;

    case BOOT_MAGIC_GOOD:
        rc = boot_write_magic(fap);
        TEST_ASSERT_FATAL(rc == 0);
        break;

    default:
        TEST_ASSERT_FATAL(0);
        break;
    }

    if (state->copy_done != 0xff) {
        rc = boot_write_copy_done(fap);
        TEST_ASSERT_FATAL(rc == 0);
    }

    if (state->image_ok != 0xff) {
        rc = boot_write_image_ok(fap);
        TEST_ASSERT_FATAL(rc == 0);
    }
}

void
boot_test_util_mark_revert(void)
{
    struct boot_swap_state state_slot0 = {
        .magic = BOOT_MAGIC_GOOD,
        .copy_done = 0x01,
        .image_ok = 0xff,
    };

    boot_test_util_write_swap_state(FLASH_AREA_IMAGE_0, &state_slot0);
}

void
boot_test_util_mark_swap_perm(void)
{
    struct boot_swap_state state_slot0 = {
        .magic = BOOT_MAGIC_GOOD,
        .copy_done = 0x01,
        .image_ok = 0x01,
    };

    boot_test_util_write_swap_state(FLASH_AREA_IMAGE_0, &state_slot0);
}

void
boot_test_util_verify_area(const struct flash_area *area_desc,
                           const struct image_header *hdr,
                           uint32_t image_addr, int img_msb)
{
    struct image_header temp_hdr;
    uint32_t area_end;
    uint32_t img_size;
    uint32_t img_off;
    uint32_t img_end;
    uint32_t addr;
    uint8_t buf[256];
    int rem_area;
    int past_image;
    int chunk_sz;
    int rem_img;
    int rc;
    int i;

    addr = area_desc->fa_off;

    if (hdr != NULL) {
        img_size = hdr->ih_img_size;

        if (addr == image_addr) {
            rc = hal_flash_read(area_desc->fa_device_id, image_addr,
                                &temp_hdr, sizeof temp_hdr);
            TEST_ASSERT(rc == 0);
            TEST_ASSERT(memcmp(&temp_hdr, hdr, sizeof *hdr) == 0);

            addr += hdr->ih_hdr_size;
        }
    } else {
        img_size = 0;
    }

    area_end = area_desc->fa_off + area_desc->fa_size;
    img_end = image_addr + img_size;
    past_image = addr >= img_end;

    while (addr < area_end) {
        rem_area = area_end - addr;
        rem_img = img_end - addr;

        if (hdr != NULL) {
            img_off = addr - image_addr - hdr->ih_hdr_size;
        } else {
            img_off = 0;
        }

        if (rem_area > sizeof buf) {
            chunk_sz = sizeof buf;
        } else {
            chunk_sz = rem_area;
        }

        rc = hal_flash_read(area_desc->fa_device_id, addr, buf, chunk_sz);
        TEST_ASSERT(rc == 0);

        for (i = 0; i < chunk_sz; i++) {
            if (rem_img > 0) {
                TEST_ASSERT(buf[i] == boot_test_util_byte_at(img_msb,
                                                        img_off + i));
            } else if (past_image) {
#if 0
                TEST_ASSERT(buf[i] == 0xff);
#endif
            }
        }

        addr += chunk_sz;
    }
}


void
boot_test_util_verify_status_clear(void)
{
    struct boot_swap_state state_slot0;
    int rc;

    rc = boot_read_swap_state_img(0, &state_slot0);
    assert(rc == 0);

    TEST_ASSERT(state_slot0.magic != BOOT_MAGIC_UNSET ||
                state_slot0.copy_done != 0);
}


void
boot_test_util_verify_flash(const struct image_header *hdr0, int orig_slot_0,
                            const struct image_header *hdr1, int orig_slot_1)
{
    const struct flash_area *area_desc;
    int area_idx;

    area_idx = 0;

    while (1) {
        area_desc = boot_test_area_descs + area_idx;
        if (area_desc->fa_off == boot_test_img_addrs[1].address &&
            area_desc->fa_device_id == boot_test_img_addrs[1].flash_id) {
            break;
        }

        boot_test_util_verify_area(area_desc, hdr0,
                                   boot_test_img_addrs[0].address, orig_slot_0);
        area_idx++;
    }

    while (1) {
        if (area_idx == BOOT_TEST_AREA_IDX_SCRATCH) {
            break;
        }

        area_desc = boot_test_area_descs + area_idx;
        boot_test_util_verify_area(area_desc, hdr1,
                                   boot_test_img_addrs[1].address, orig_slot_1);
        area_idx++;
    }
}

void
boot_test_util_verify_all(int expected_swap_type,
                          const struct image_header *hdr0,
                          const struct image_header *hdr1)
{
    const struct image_header *slot0hdr;
    const struct image_header *slot1hdr;
    struct boot_rsp rsp;
    uint32_t flash_base;
    int orig_slot_0;
    int orig_slot_1;
    int num_swaps;
    int rc;
    int i;

    TEST_ASSERT_FATAL(hdr0 != NULL || hdr1 != NULL);

    num_swaps = 0;
    for (i = 0; i < 3; i++) {
        rc = boot_go(&rsp);
        TEST_ASSERT_FATAL(rc == 0);

        if (expected_swap_type != BOOT_SWAP_TYPE_NONE) {
            num_swaps++;
        }

        if (num_swaps % 2 == 0) {
            if (hdr0 != NULL) {
                slot0hdr = hdr0;
                slot1hdr = hdr1;
            } else {
                slot0hdr = hdr1;
                slot1hdr = hdr0;
            }
            orig_slot_0 = 0;
            orig_slot_1 = 1;
        } else {
            if (hdr1 != NULL) {
                slot0hdr = hdr1;
                slot1hdr = hdr0;
            } else {
                slot0hdr = hdr0;
                slot1hdr = hdr1;
            }
            orig_slot_0 = 1;
            orig_slot_1 = 0;
        }

        rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
        TEST_ASSERT_FATAL(rc == 0);

        TEST_ASSERT(memcmp(rsp.br_hdr, slot0hdr, sizeof *slot0hdr) == 0);
        TEST_ASSERT(rsp.br_flash_dev_id == boot_test_img_addrs[0].flash_id);
        TEST_ASSERT(flash_base + rsp.br_image_off ==
                    boot_test_img_addrs[0].address);

        boot_test_util_verify_flash(slot0hdr, orig_slot_0,
                                    slot1hdr, orig_slot_1);
        boot_test_util_verify_status_clear();

        if (expected_swap_type != BOOT_SWAP_TYPE_NONE) {
            switch (expected_swap_type) {
            case BOOT_SWAP_TYPE_TEST:
                expected_swap_type = BOOT_SWAP_TYPE_REVERT;
                break;

            case BOOT_SWAP_TYPE_PERM:
                expected_swap_type = BOOT_SWAP_TYPE_NONE;
                break;

            case BOOT_SWAP_TYPE_REVERT:
                expected_swap_type = BOOT_SWAP_TYPE_NONE;
                break;

            default:
                TEST_ASSERT_FATAL(0);
                break;
            }
        }
    }
}
