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
#include <stdint.h>

#include "sysflash/sysflash.h"
#include "cy_efuse.h"

#include "cy_security_cnt_platform.h"
#include "cy_service_app.h"

#if defined MCUBootApp && defined MCUBOOT_HW_ROLLBACK_PROT

#define TEST_BIT(var, pos) (0U != ((var) & (1UL << (pos))))

#define NV_COUNTER_EFUSE_OFFSET  0x60

/**
 * Efuse stores nv counter value as a consequent bits. This means
 * NV counter set to 5 in policy would be written as 0x1F. This function
 * converts efuse value to integer value.
 *
 * @param val     Value of security counter from which came from efuse
 *                which needs to converted in a number
 *
 * @return        Security counter value in number form encoded in complex type on success;
 *                FIH_FAILURE on failure.
 */
static fih_uint convert_efuse_val(fih_uint val)
{
    uint32_t i = 0U;
    uint32_t j = MAX_SEC_COUNTER_VAL - 1U;

    while (TEST_BIT(fih_uint_decode(val), i++)) {
        j--;
    }

    if ((MAX_SEC_COUNTER_VAL - j) == i) {
        return fih_uint_encode(i - 1U);
    }
    else {
        return (fih_uint)FIH_FAILURE;
    }
}

/**
 * Reads a data corresponding to security counter which is stored in
 * efuses of chip and converts it actual value of security counter
 *
 * @param security_cnt     Pointer to a variable, where security counter value would be stored
 *
 * @return                 FIH_SUCESS on success; FIH_FAILURE on failure.
 */
fih_int platform_security_counter_get(fih_uint *security_cnt) {

    fih_int fih_ret = FIH_FAILURE;
    cy_en_efuse_status_t efuse_stat = CY_EFUSE_ERR_UNC;
    uint32_t nv_counter = 0;
    fih_uint nv_counter_secure = (fih_uint)FIH_FAILURE;

    /* Init also enables Efuse block */
    efuse_stat = Cy_EFUSE_Init(EFUSE);

    if (efuse_stat == CY_EFUSE_SUCCESS) {

        efuse_stat = Cy_EFUSE_ReadWord(EFUSE, &nv_counter, NV_COUNTER_EFUSE_OFFSET);

        if (efuse_stat == CY_EFUSE_SUCCESS){
            /* Read value of counter from efuse twice to ensure value is not compromised */
            nv_counter_secure = fih_uint_encode(nv_counter);
            nv_counter = 0U;
            efuse_stat = Cy_EFUSE_ReadWord(EFUSE, &nv_counter, NV_COUNTER_EFUSE_OFFSET);
        }
        if (efuse_stat == CY_EFUSE_SUCCESS){

            if (fih_uint_eq(nv_counter_secure, fih_uint_encode(nv_counter))) {

                *security_cnt = convert_efuse_val(nv_counter);
                fih_ret = FIH_SUCCESS;

            }
        }

        Cy_EFUSE_Disable(EFUSE);
        Cy_EFUSE_DeInit(EFUSE);
    }

    FIH_RET(fih_ret);
}

/**
 * Updates the stored value of a given image's security counter with a new
 * security counter value if the new one is greater.
 *
 * @param reprov_packet     Pointer to a reprovisioning packet containing NV counter.
 * @param packet_len        Length of a packet
 * @param img_security_cnt  Security counter value of image
 *
 * @return                  0 on success; nonzero on failure.
 */
int32_t platform_security_counter_update(uint32_t img_security_cnt, uint8_t * reprov_packet)
{
    int32_t rc = -1;
    fih_uint security_cnt = (fih_uint) FIH_FAILURE;
    fih_int fih_rc = FIH_FAILURE;

    /* Read value of security counter stored in chips efuses.
     * Only one security counter is available in system. Maximum value is 32.
    */
    FIH_CALL(platform_security_counter_get, fih_rc, &security_cnt);

    if (true == fih_eq(fih_rc, FIH_SUCCESS)) {

        /* Compare the new image's security counter value against the
        * stored security counter value.
        */
        if ( (img_security_cnt > fih_uint_decode(security_cnt)) &&
             (img_security_cnt <= MAX_SEC_COUNTER_VAL) ) {
            
            /* Attention: This function initiates system reset */
            call_service_app(reprov_packet);
            /* Runtime should never get here. Panic statement added to secure
             * sutiation when hacker initiates skip of call_service_app function. 
            */
            FIH_PANIC;
        }
        else {
            rc = 0;
        }
    }

return rc;
}

#endif /* defined MCUBootApp && defined MCUBOOT_HW_ROLLBACK_PROT */
