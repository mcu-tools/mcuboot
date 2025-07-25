/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2025 Nordic Semiconductor ASA
 */
#include <../../bootutil/src/bootutil_priv.h>
#ifdef MCUBOOT_BOOT_REQUEST
#include <bootutil/boot_request.h>
#endif /* MCUBOOT_BOOT_REQUEST */

/**
 * Finds the preferred slot containing the image based on bootloader requests.
 */
int boot_find_next_slot_hook(struct boot_loader_state *state, uint8_t image, uint32_t *active_slot)
{
#ifdef MCUBOOT_BOOT_REQUEST
    uint32_t slot = BOOT_REQUEST_NO_PREFERRED_SLOT;

    if (active_slot == NULL) {
        return BOOT_HOOK_REGULAR;
    }

    slot = boot_request_get_preferred_slot(image);
    if (slot != BOOT_REQUEST_NO_PREFERRED_SLOT) {
        if (state->slot_usage[image].slot_available[slot]) {
            *active_slot = slot;
            return 0;
        }
    }
#endif /* MCUBOOT_BOOT_REQUEST */

    return BOOT_HOOK_REGULAR;
}
