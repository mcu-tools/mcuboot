/*
 * Copyright (c) 2020 Arm Limited.
 * Copyright (c) 2021 Infineon Technologies AG
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

#ifndef CY_SECURITY_CNT_PLATFORM_H
#define CY_SECURITY_CNT_PLATFORM_H

#if defined MCUBOOT_HW_ROLLBACK_PROT

#include "bootutil/fault_injection_hardening.h"

/**
 * Reads a data corresponding to security counter which is stored in
 * efuses of chip and converts it actual value of security conter
 *
 * @param image_id         Image ID of counter to get value
 * @param security_cnt     Pointer to a variable, where security conter value would be stored
 *
 * @return                  FIH_SUCCESS on success; FIH_FAILURE on failure.
 */
fih_int platform_security_counter_get(uint32_t image_id, fih_uint *security_cnt);

/**
 * Updates the stored value of a given image's security counter with a new
 * security counter value if the new one is greater.
 *
 * @param image_id          Image ID of counter to update value
 * @param img_security_cnt  Security conter value of image
 * @param custom_data       Supplemental data
 *
 * @return                  0 on success; nonzero on failure.
 */
int32_t platform_security_counter_update(uint32_t image_id, fih_uint img_security_cnt, uint8_t *custom_data);

#endif /* MCUBOOT_HW_ROLLBACK_PROT */

#endif /* CY_SECURITY_CNT_PLATFORM_H */
