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
#include <zephyr.h>
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include "flash_map_backend/flash_map_backend.h"

/* @retval 0: header was read/populated
 *         FIH_FAILURE: image is invalid,
 *         1 if hook not implemented for the image-slot,
 *         othervise an error-code value.
 */
int boot_read_image_header_hook(int img_index, int slot, struct image_header *img_hed)
{
    if (img_index == 1 && slot == 0) {
            img_hed->ih_magic = IMAGE_MAGIC;
            return 0;
    }

    return 1;
}

/* @retval FIH_SUCCESS: image is valid,
 *         FIH_FAILURE: image is invalid,
 *         fih encoded 1 if hook not implemented for the image-slot.
 */
fih_int boot_image_check_hook(int img_index)
{
    if (img_index == 1) {
        FIH_RET(FIH_SUCCESS);
    }

    FIH_RET(fih_int_encode(1));
}

int boot_perform_update_hook(int img_index, struct image_header *img_head,
                             const struct flash_area *area)
{
    if (img_index == 1) {
        return 0;
    }

    return 1;
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
    }

    return 1;
}
