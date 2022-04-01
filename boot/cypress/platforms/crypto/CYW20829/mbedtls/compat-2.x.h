/********************************************************************************
* Copyright 2022 Infineon Technologies AG
* SPDX-License-Identifier: Apache-2.0
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
********************************************************************************/

#if !defined(MBEDTLS_COMPAT2X_H)
#define MBEDTLS_COMPAT2X_H

#include <string.h>

#define mbedtls_sha256_starts_ret mbedtls_sha256_starts
#define mbedtls_sha256_finish_ret mbedtls_sha256_finish

static inline int mbedtls_sha256_update_ret(struct mbedtls_sha256_context *ctx,
                                            const unsigned char           *input,
                                            size_t                        ilen)
{
    /* Cryptolite accelerator does not work on CBUS! */
    if (input >= (const unsigned char *)CY_XIP_REMAP_OFFSET &&
        input < (const unsigned char *)(CY_XIP_REMAP_OFFSET + CY_XIP_SIZE)) {

        if (input + ilen > (const unsigned char *)(CY_XIP_REMAP_OFFSET + CY_XIP_SIZE)) {
            return -MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
        }
        else {
#ifdef MCUBOOT_ENC_IMAGES_XIP
            /* Process chunks copied to SRAM */
            uint8_t tmp_buf[0x400];
            size_t offs = 0;
            int rc = 0;

            while (rc == 0 && offs < ilen) {
                size_t len = ilen - offs;

                if (len > sizeof(tmp_buf)) {
                    len = sizeof(tmp_buf);
                }

                (void)memcpy(tmp_buf, input + offs, len);
                rc = mbedtls_sha256_update(ctx, tmp_buf, len);
                offs += len;
            }

            (void)memset(tmp_buf, 0, sizeof(tmp_buf));
            return rc;
#else
            /* Remap to SAHB */
            input += CY_XIP_BASE - CY_XIP_SIZE;
#endif /* MCUBOOT_ENC_IMAGES_XIP */
        }
    }

    return mbedtls_sha256_update(ctx, input, ilen);
}

#endif /* MBEDTLS_COMPAT2X_H */
