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

#if defined MCUBOOT_HW_ROLLBACK_PROT

#include <stdint.h>
#include <string.h>
#include "bootutil/security_cnt.h"
#include "cy_security_cnt_platform.h"

fih_int
boot_nv_security_counter_init(void)
{
    /* Do nothing. */
    return 0;
}

fih_int
boot_nv_security_counter_get(uint32_t image_id, fih_uint *security_cnt)
{
    (void)image_id;
    fih_int fih_ret = FIH_FAILURE;

    if (NULL != security_cnt) { 
        FIH_CALL(platform_security_counter_get, fih_ret, security_cnt);
    }

    FIH_RET(fih_ret);
}

int32_t
boot_nv_security_counter_update(uint32_t image_id, uint32_t img_security_cnt, void * custom_data)
{
    (void)image_id;

    int32_t rc = platform_security_counter_update(img_security_cnt, (uint8_t *)custom_data);

    /* Do nothing. */
    return rc;
}

#endif /* defined MCUBOOT_HW_ROLLBACK_PROT */
