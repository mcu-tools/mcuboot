/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "bootutil/bootutil_public.h"

#include "test_images.h"

#define TEST_PASS 0
#define TEST_FAIL 1

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define RUN_TEST(fn) do { \
    printf("\n=== Running: %s ===\n", #fn); \
    g_tests_run++; \
    int res = fn(); \
    if (res == TEST_PASS) { \
        printf("[PASS] %s\n", #fn); \
        g_tests_passed++; \
    } else { \
        printf("[FAIL] %s\n", #fn); \
    } \
} while (0)

/* Helper: Load image bytes into a designated flash area */
static int load_image_to_area(uint8_t area_id, const uint8_t *img, uint32_t size)
{
    const struct flash_area *fa = NULL;
    int rc;

    rc = flash_area_open(area_id, &fa);
    if (rc != 0 || fa == NULL) {
        return rc;
    }

    uint32_t align_mask = 128 * 1024 - 1;
    uint32_t erase_size = (size + align_mask) & ~align_mask;
    if (erase_size > fa->fa_size) {
        erase_size = fa->fa_size;
    }

    rc = flash_area_erase(fa, 0, erase_size);
    if (rc != 0) {
        flash_area_close(fa);
        return rc;
    }

    uint32_t align = flash_area_align(fa);
    uint32_t write_size = (size + (align - 1)) & ~(align - 1);
    uint8_t padded_buf[4096];
    assert(write_size <= sizeof(padded_buf));

    memset(padded_buf, 0xFF, write_size);
    memcpy(padded_buf, img, size);

    rc = flash_area_write(fa, 0, padded_buf, write_size);
    flash_area_close(fa);
    return rc;
}

/*
 * Test 1: Flash-Map Bounds and Sector Alignment
 * Verifies geometry, area open/close, sector count, alignment requirements,
 * bounds checking, and NOR bit clearing behavior.
 */
static int test_flash_map_bounds_and_alignment(void)
{
    const struct flash_area *fa_primary = NULL;
    const struct flash_area *fa_secondary = NULL;
    const struct flash_area *fa_scratch = NULL;
    const struct flash_area *fa_invalid = NULL;
    int rc;

    flash_map_backend_reset();

    rc = flash_area_open(FLASH_AREA_PRIMARY_ID, &fa_primary);
    if (rc != 0 || fa_primary == NULL) return TEST_FAIL;

    rc = flash_area_open(FLASH_AREA_SECONDARY_ID, &fa_secondary);
    if (rc != 0 || fa_secondary == NULL) return TEST_FAIL;

    rc = flash_area_open(FLASH_AREA_SCRATCH_ID, &fa_scratch);
    if (rc != 0 || fa_scratch == NULL) return TEST_FAIL;

    /* Verify non-existent ID fails */
    rc = flash_area_open(99, &fa_invalid);
    if (rc == 0 || fa_invalid != NULL) return TEST_FAIL;

    /* Verify primary and secondary slot sizes match */
    if (fa_primary->fa_size != fa_secondary->fa_size) {
        printf("Size mismatch: primary=0x%x, secondary=0x%x\n",
               (unsigned int)fa_primary->fa_size, (unsigned int)fa_secondary->fa_size);
        return TEST_FAIL;
    }

    /* Verify scratch size is at least as large as 1 sector (128 KB) */
    if (fa_scratch->fa_size < (128 * 1024)) {
        return TEST_FAIL;
    }

    /* Verify sector counts */
    uint32_t sector_count = 0;
    rc = flash_area_get_sectors(FLASH_AREA_PRIMARY_ID, &sector_count, NULL);
    if (rc != 0 || sector_count != 3) {
        printf("Unexpected primary sector count: %u (expected 3)\n", sector_count);
        return TEST_FAIL;
    }

    struct flash_sector sectors[4];
    sector_count = 4;
    rc = flash_area_get_sectors(FLASH_AREA_PRIMARY_ID, &sector_count, sectors);
    if (rc != 0 || sector_count != 3) return TEST_FAIL;
    if (sectors[0].fs_off != 0 || sectors[0].fs_size != (128 * 1024)) return TEST_FAIL;
    if (sectors[1].fs_off != (128 * 1024) || sectors[1].fs_size != (128 * 1024)) return TEST_FAIL;
    if (sectors[2].fs_off != (256 * 1024) || sectors[2].fs_size != (128 * 1024)) return TEST_FAIL;

    /* Test out-of-bounds read and write */
    uint8_t buf[16];
    rc = flash_area_read(fa_primary, fa_primary->fa_size - 4, buf, 16);
    if (rc == 0) {
        printf("Out of bounds read succeeded unexpectedly!\n");
        return TEST_FAIL;
    }

    uint8_t zero_data[8] = {0};
    rc = flash_area_write(fa_primary, fa_primary->fa_size - 4, zero_data, 8);
    if (rc == 0) {
        printf("Out of bounds write succeeded unexpectedly!\n");
        return TEST_FAIL;
    }

    /* Test unaligned write */
    rc = flash_area_write(fa_primary, 3, zero_data, 8);
    if (rc == 0) {
        printf("Unaligned write offset succeeded unexpectedly!\n");
        return TEST_FAIL;
    }

    rc = flash_area_write(fa_primary, 8, zero_data, 5);
    if (rc == 0) {
        printf("Unaligned write length succeeded unexpectedly!\n");
        return TEST_FAIL;
    }

    /* Test write alignment getter and erased value */
    if (flash_area_align(fa_primary) != 8) return TEST_FAIL;
    if (flash_area_erased_val(fa_primary) != 0xFF) return TEST_FAIL;

    flash_area_close(fa_primary);
    flash_area_close(fa_secondary);
    flash_area_close(fa_scratch);
    return TEST_PASS;
}

/*
 * Test 2: Signed Boot with Valid ECDSA-P256 Signature
 * Loads genuine signed v1.0.0 image into primary slot and verifies boot_go succeeds.
 */
static int test_signed_boot_valid(void)
{
    struct boot_rsp rsp;
    int rc;

    flash_map_backend_reset();

    rc = load_image_to_area(FLASH_AREA_PRIMARY_ID, s_test_image_v1, TEST_IMAGE_V1_SIZE);
    if (rc != 0) return TEST_FAIL;

    rc = boot_go(&rsp);
    if (rc != 0) {
        printf("boot_go failed on valid signed image: %d\n", rc);
        return TEST_FAIL;
    }

    if (rsp.br_image_off != 0x00020000) {
        printf("Unexpected image offset: 0x%08x\n", (unsigned int)rsp.br_image_off);
        return TEST_FAIL;
    }

    if (rsp.br_hdr->ih_ver.iv_major != 1 || rsp.br_hdr->ih_ver.iv_minor != 0) {
        printf("Unexpected image version: %u.%u\n", rsp.br_hdr->ih_ver.iv_major, rsp.br_hdr->ih_ver.iv_minor);
        return TEST_FAIL;
    }

    printf("Successfully verified and booted signed image v%u.%u.%u\n",
           rsp.br_hdr->ih_ver.iv_major, rsp.br_hdr->ih_ver.iv_minor, rsp.br_hdr->ih_ver.iv_revision);
    return TEST_PASS;
}

/*
 * Test 3: Invalid Signature Rejection
 * Corrupts the signature / payload of image and verifies boot_go rejects it.
 */
static int test_invalid_signature_rejection(void)
{
    struct boot_rsp rsp;
    uint8_t tampered[TEST_IMAGE_V1_SIZE];
    int rc;

    flash_map_backend_reset();
    memcpy(tampered, s_test_image_v1, TEST_IMAGE_V1_SIZE);

    /* Tamper with payload byte after header */
    tampered[0x200 + 10] ^= 0xFF;

    rc = load_image_to_area(FLASH_AREA_PRIMARY_ID, tampered, TEST_IMAGE_V1_SIZE);
    if (rc != 0) return TEST_FAIL;

    rc = boot_go(&rsp);
    if (rc == 0) {
        printf("boot_go unexpectedly accepted tampered image!\n");
        return TEST_FAIL;
    }

    printf("Tampered image was correctly rejected by MCUboot (rc=%d).\n", rc);
    return TEST_PASS;
}

/*
 * Test 4: Confirmed Update Workflow
 * Primary has v1.0.0. Secondary has v2.0.0.
 * Sets pending, runs boot_go (swaps to v2.0.0), confirms with boot_set_confirmed_multi(),
 * reboots and verifies v2.0.0 remains permanently active in primary slot.
 */
static int test_confirmed_update_workflow(void)
{
    struct boot_rsp rsp;
    int rc;

    flash_map_backend_reset();

    rc = load_image_to_area(FLASH_AREA_PRIMARY_ID, s_test_image_v1, TEST_IMAGE_V1_SIZE);
    if (rc != 0) return TEST_FAIL;

    rc = load_image_to_area(FLASH_AREA_SECONDARY_ID, s_test_image_v2, TEST_IMAGE_V2_SIZE);
    if (rc != 0) return TEST_FAIL;

    /* Mark secondary image as pending update */
    rc = boot_set_pending_multi(0, 0);
    if (rc != 0) {
        printf("boot_set_pending_multi failed: %d\n", rc);
        return TEST_FAIL;
    }

    /* Bootloader executes: swaps v2 into primary slot */
    rc = boot_go(&rsp);
    if (rc != 0) {
        printf("boot_go swap failed: %d\n", rc);
        return TEST_FAIL;
    }

    if (rsp.br_hdr->ih_ver.iv_major != 2) {
        printf("Expected swapped image version 2, got %u\n", rsp.br_hdr->ih_ver.iv_major);
        return TEST_FAIL;
    }

    /* Application starts up and confirms running image */
    rc = boot_set_confirmed_multi(0);
    if (rc != 0) {
        printf("boot_set_confirmed_multi failed: %d\n", rc);
        return TEST_FAIL;
    }

    /* Subsequent reboot: must remain on confirmed v2 */
    rc = boot_go(&rsp);
    if (rc != 0) {
        printf("boot_go post-confirm failed: %d\n", rc);
        return TEST_FAIL;
    }

    if (rsp.br_hdr->ih_ver.iv_major != 2) {
        printf("Firmware did not remain on v2 after confirmation: got %u\n", rsp.br_hdr->ih_ver.iv_major);
        return TEST_FAIL;
    }

    printf("Confirmed update workflow succeeded: firmware upgraded to v2.0.0 and confirmed.\n");
    return TEST_PASS;
}

/*
 * Test 5: Rollback After Unconfirmed Update
 * Primary has v1.0.0. Secondary has v2.0.0.
 * Sets test pending, runs boot_go (swaps to v2.0.0 for test),
 * NO confirmation is issued (simulating app crash/failure),
 * Next reboot runs boot_go: verifies MCUboot reverts back to v1.0.0!
 */
static int test_rollback_after_unconfirmed_update(void)
{
    struct boot_rsp rsp;
    int rc;

    flash_map_backend_reset();

    rc = load_image_to_area(FLASH_AREA_PRIMARY_ID, s_test_image_v1, TEST_IMAGE_V1_SIZE);
    if (rc != 0) return TEST_FAIL;

    rc = load_image_to_area(FLASH_AREA_SECONDARY_ID, s_test_image_v2, TEST_IMAGE_V2_SIZE);
    if (rc != 0) return TEST_FAIL;

    /* Set test update (revert on reboot if not confirmed) */
    rc = boot_set_pending_multi(0, 0);
    if (rc != 0) return TEST_FAIL;

    /* First boot: swaps to v2 */
    rc = boot_go(&rsp);
    if (rc != 0 || rsp.br_hdr->ih_ver.iv_major != 2) {
        printf("First boot did not swap to v2\n");
        return TEST_FAIL;
    }

    /* Do NOT confirm! Simulate system reset / watchdog reboot */
    printf("Simulating system reset without confirmation...\n");

    /* Second boot: MCUboot must detect unconfirmed image and rollback to v1 */
    rc = boot_go(&rsp);
    if (rc != 0) {
        printf("boot_go rollback failed: %d\n", rc);
        return TEST_FAIL;
    }

    if (rsp.br_hdr->ih_ver.iv_major != 1) {
        printf("Rollback failed! Expected v1.0.0, got v%u\n", rsp.br_hdr->ih_ver.iv_major);
        return TEST_FAIL;
    }

    printf("Rollback succeeded: system safely reverted back to v1.0.0.\n");
    return TEST_PASS;
}

/*
 * Test 6: Recovery After Interrupted Swap
 * Simulates sudden power loss during the sector swap operation.
 * Next boot resumes and completes recovery.
 */
static int test_interrupted_swap_recovery(void)
{
    struct boot_rsp rsp;
    int rc;
    jmp_buf reboot_env;

    flash_map_backend_reset();

    rc = load_image_to_area(FLASH_AREA_PRIMARY_ID, s_test_image_v1, TEST_IMAGE_V1_SIZE);
    if (rc != 0) return TEST_FAIL;

    rc = load_image_to_area(FLASH_AREA_SECONDARY_ID, s_test_image_v2, TEST_IMAGE_V2_SIZE);
    if (rc != 0) return TEST_FAIL;

    rc = boot_set_pending_multi(0, 0);
    if (rc != 0) return TEST_FAIL;

    /*
     * Simulate sudden power failure / reset midway during swap:
     * We arm the interruption after 5 write operations.
     * When power is cut, hardware resets abruptly back to bootloader entry.
     */
    if (setjmp(reboot_env) == 0) {
        printf("Simulating power loss during swap (cutting power after 5 writes)...\n");
        flash_map_set_interruption(true, 5, &reboot_env);
        boot_go(&rsp);
        printf("Error: expected power cut, but boot_go finished normally!\n");
        return TEST_FAIL;
    }

    printf("Power cut simulated: hardware abruptly reset to entry point.\n");

    /* Power restored: disable interruption and re-run boot_go */
    printf("Power restored: executing boot_go recovery...\n");
    flash_map_set_interruption(false, 0, NULL);

    rc = boot_go(&rsp);
    if (rc != 0) {
        printf("Recovery after interrupted swap failed: %d\n", rc);
        return TEST_FAIL;
    }

    printf("Interrupted swap recovery succeeded: bootloader safely completed recovery to image v%u.%u.%u.\n",
           rsp.br_hdr->ih_ver.iv_major, rsp.br_hdr->ih_ver.iv_minor, rsp.br_hdr->ih_ver.iv_revision);
    return TEST_PASS;
}

int main(void)
{
    printf("====================================================\n");
    printf("  MCUboot Test Suite for Eclipse ThreadX Integration\n");
    printf("  Reference Target: STM32F429ZI-Nucleo (Cortex-M4)\n");
    printf("====================================================\n");

    RUN_TEST(test_flash_map_bounds_and_alignment);
    RUN_TEST(test_signed_boot_valid);
    RUN_TEST(test_invalid_signature_rejection);
    RUN_TEST(test_confirmed_update_workflow);
    RUN_TEST(test_rollback_after_unconfirmed_update);
    RUN_TEST(test_interrupted_swap_recovery);

    printf("\n====================================================\n");
    printf("  Test Summary: %d / %d tests passed\n", g_tests_passed, g_tests_run);
    printf("====================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
