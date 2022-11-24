/*
 * Copyright (c) 2020 Arm Limited.
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

#include "bootutil/security_cnt.h"
#include <stdint.h>

fih_ret
boot_nv_security_counter_init(void)
{
    /* Do nothing. */
    return FIH_SUCCESS;
}

fih_ret
boot_nv_security_counter_get(uint32_t image_id, fih_int *security_cnt)
{
    (void)image_id;
    *security_cnt = 30;

    return FIH_SUCCESS;
}

int32_t
boot_nv_security_counter_update(uint32_t image_id, uint32_t img_security_cnt)
{
    (void)image_id;
    (void)img_security_cnt;

    /* Do nothing. */
    return 0;
}
