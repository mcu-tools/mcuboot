/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
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

#include <assert.h>
#include <zephyr/kernel.h>
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include "flash_map_backend/flash_map_backend.h"

/* @retval 0: header was read/populated
 *         FIH_FAILURE: image is invalid,
 *         BOOT_HOOK_REGULAR if hook not implemented for the image-slot,
 *         othervise an error-code value.
 */
int boot_read_image_header_hook(int img_index, int slot,
                                struct image_header *img_hed)
{
    if (img_index == 1 && slot == 0) {
            img_hed->ih_magic = IMAGE_MAGIC;
            return 0;
    }

    return BOOT_HOOK_REGULAR;
}

/* @retval FIH_SUCCESS: image is valid,
 *         FIH_FAILURE: image is invalid,
 *         fih encoded BOOT_HOOK_REGULAR if hook not implemented for
 *         the image-slot.
 */
fih_ret boot_image_check_hook(int img_index, int slot)
{
    if (img_index == 1 && slot == 0) {
        FIH_RET(FIH_SUCCESS);
    }

    FIH_RET(FIH_BOOT_HOOK_REGULAR);
}

int boot_perform_update_hook(int img_index, struct image_header *img_head,
                             const struct flash_area *area)
{
    if (img_index == 1) {
        return 0;
    }

    return BOOT_HOOK_REGULAR;
}

int boot_read_swap_state_primary_slot_hook(int image_index,
                                           struct boot_swap_state *state)
{
    if (image_index == 1) {
        state->magic = BOOT_MAGIC_UNSET;
        state->swap_type = BOOT_SWAP_TYPE_NONE;
        state->image_num = image_index ; // ?
        state->copy_done = BOOT_FLAG_UNSET;
        state->image_ok = BOOT_FLAG_UNSET;

        return 0;
    }

    return BOOT_HOOK_REGULAR;
}

int boot_copy_region_post_hook(int img_index, const struct flash_area *area,
                               size_t size)
{
    return 0;
}

int boot_serial_uploaded_hook(int img_index, const struct flash_area *area,
                               size_t size)
{
    return 0;
}

int boot_img_install_stat_hook(int image_index, int slot, int *img_install_stat)
{
    return BOOT_HOOK_REGULAR;
}
