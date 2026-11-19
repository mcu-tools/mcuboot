/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"
#include "bootutil/bootutil_public.h"

/*
 * Sample Eclipse ThreadX application demonstrating MCUboot integration:
 * - Booting from primary slot (offset 0x200 header reserved)
 * - Self-confirmation via boot_set_confirmed_multi()
 * - Staging upgrade image into secondary slot
 * - Triggering update via boot_set_pending_multi()
 */

#define APP_VERSION "1.0.0"

/*
 * Application self-confirmation:
 * Inspects whether the running image was started in test mode;
 * if so, marks it confirmed so MCUboot will not revert on next reboot.
 */
int app_confirm_running_image(void)
{
    struct boot_swap_state swap_state;
    int rc;

    rc = boot_read_swap_state_by_id(FLASH_AREA_PRIMARY_ID, &swap_state);
    if (rc != 0) {
        printf("[APP] Failed to read swap state: %d\n", rc);
        return rc;
    }

    printf("[APP] Primary slot swap state: type=%u, image_ok=%u, copy_done=%u\n",
           swap_state.swap_type, swap_state.image_ok, swap_state.copy_done);

    if (swap_state.image_ok != BOOT_FLAG_SET) {
        printf("[APP] Unconfirmed test image detected, confirming...\n");
        rc = boot_set_confirmed_multi(0);
        if (rc != 0) {
            printf("[APP] Failed to confirm image: %d\n", rc);
            return rc;
        }
        printf("[APP] Image successfully confirmed as stable.\n");
    } else {
        printf("[APP] Image already confirmed.\n");
    }

    return 0;
}

/*
 * Firmware update staging:
 * Writes a newly received signed image into the secondary slot
 * and marks it pending for upgrade on the next reboot.
 */
int app_stage_firmware_update(const uint8_t *signed_image, uint32_t image_size, bool permanent)
{
    const struct flash_area *fa = NULL;
    int rc;

    if (signed_image == NULL || image_size == 0) {
        return -1;
    }

    rc = flash_area_open(FLASH_AREA_SECONDARY_ID, &fa);
    if (rc != 0 || fa == NULL) {
        printf("[APP] Failed to open secondary slot: %d\n", rc);
        return rc;
    }

    printf("[APP] Erasing secondary slot for size 0x%x...\n", (unsigned int)image_size);
    /* Sector align erase size */
    uint32_t align_mask = 128 * 1024 - 1;
    uint32_t erase_size = (image_size + align_mask) & ~align_mask;
    if (erase_size > fa->fa_size) {
        erase_size = fa->fa_size;
    }

    rc = flash_area_erase(fa, 0, erase_size);
    if (rc != 0) {
        printf("[APP] Erase failed: %d\n", rc);
        flash_area_close(fa);
        return rc;
    }

    printf("[APP] Writing signed image to secondary slot...\n");
    rc = flash_area_write(fa, 0, signed_image, image_size);
    if (rc != 0) {
        printf("[APP] Flash write failed: %d\n", rc);
        flash_area_close(fa);
        return rc;
    }

    flash_area_close(fa);

    printf("[APP] Marking secondary slot image as pending (permanent=%d)...\n", permanent ? 1 : 0);
    rc = boot_set_pending_multi(0, permanent ? 1 : 0);
    if (rc != 0) {
        printf("[APP] Failed to set pending flag: %d\n", rc);
        return rc;
    }

    printf("[APP] Update staged successfully! Reboot to initiate swap.\n");
    return 0;
}

int main(void)
{
    printf("[APP] Eclipse ThreadX Application v%s running under MCUboot\n", APP_VERSION);
    app_confirm_running_image();
    return 0;
}
