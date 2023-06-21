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

#include "bootutil/fault_injection_hardening.h"

#define MAX_SEC_COUNTER_VAL      (32U)

/**
 * Reads the security counter data from chip's EFUSEs and converts it to the actual value of 
 * security counter for each image.
 * 
 * @param image_id          The image number for which you want to get a security counter,
 *                          saved in EFUSE (from 0)
 * @param security_cnt      Pointer to a variable, where security conter value would be stored
 *
 * @return                  FIH_SUCESS on success; FIH_FAILURE on failure.
 */
fih_int platform_security_counter_get(uint32_t image_id, fih_uint *security_cnt);


/**
 * Updates the stored value of a given image's security counter with a new
 * security counter value if the new one is greater. 
 * Only one security counter is available in system. Maximum value is 32.
 * Since more than one image can be used, 32 bits of NV counter are divided into 
 * number of images (it's on a user decision how many bits for each image).
 *
 * @param image_id          The image number for which you want to get a security counter,
 *                          saved in EFUSE (from 0)
 * @param img_security_cnt  Security counter value of image: appropriated bit array inside of 32bits
 * 
 * @param reprov_packet     Pointer to a reprovisioning packet containing NV counter.
 * 
 * @return                  0 on success; nonzero on failure.
 */

int32_t platform_security_counter_update(uint32_t image_id, fih_uint img_security_cnt, uint8_t * reprov_packet);


/**
 * Converts security counter for the desired image from full NV-counter 
 * to a decimal integer value with validation of all bits in 'nv_counter'.
 * If NV-counter has bit(s) in position for another 'image_id', it returns an error.
 * For example, the folowing case updates counter for image 0 - it returns an error
 * because the counter for image 0 contains bits in position for the image 1:
 * *************************************************
 *  bits for image 1  | 5 bits are reserved for image 0
 * ***************************
 *                   1111111 *
 * ***************************
 * 
 * Efuse stores nv counter value as a consequent bits. This means
 * NV counter set to 5 in policy would be written as 0x1F.
 * Only one security counter is available in system. Maximum value is 32.
 * Since more than one image can be used, 32 bits of NV counter are divided into
 * number of images (it's on a user decision how many bits for each image).
 *
 * @param image_id          Index of the image (from 0)
 *
 * @param nv_counter        Image security counter read out from TLV packet of image_id.
 * 
 * @param extracted_img_cnt Pointer to a variable, where extracted counter for the 'image_id'
 *                          would be stored
 * @return                  FIH_FAILURE on failure, otherwise FIH_SUCCESS.
 * 
 * @warning                 Don't use this function inside of 'platform_security_counter_get' or
 *                          'platform_security_counter_update' functions.
 *                          For this purpose please use the function 'counter_extract'.
 */

fih_int platform_security_counter_check_extract(uint32_t image_id, fih_uint nv_counter, fih_uint *extracted_img_cnt);

#endif /* CY_SECURITY_CNT_PLATFORM_H */
