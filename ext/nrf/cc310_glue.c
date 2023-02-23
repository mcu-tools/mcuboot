/*
 *  Copyright Nordic Semiconductor ASA
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "cc310_glue.h"

int cc310_init(void)
{
    /* Only initialize once */
    static bool initialized;

    if (!initialized) {
        nrf_cc310_enable();
        if (nrf_cc310_bl_init() != 0) {
            return -1;
        }
        initialized = true;
        nrf_cc310_disable();
    }

    return 0;
}

void cc310_sha256_update(nrf_cc310_bl_hash_context_sha256_t *ctx,
                         const void *data,
                         uint32_t data_len)
{
    /*
     * NRF Cryptocell can only read from RAM this allocates a buffer on the stack
     * if the data provided is not located in RAM.
     */

    if ((uint32_t) data < CONFIG_SRAM_BASE_ADDRESS) {
        uint8_t stack_buffer[data_len];
        uint32_t block_len = data_len;
        memcpy(stack_buffer, data, block_len);
        nrf_cc310_bl_hash_sha256_update(ctx, stack_buffer, block_len);
    } else {
        nrf_cc310_bl_hash_sha256_update(ctx, data, data_len);
    }
};

int cc310_ecdsa_verify_secp256r1(uint8_t *hash,
                                 uint8_t *public_key,
                                 uint8_t *signature,
                                 size_t hash_len)
{
        int rc;
        nrf_cc310_bl_ecdsa_verify_context_secp256r1_t ctx;
        cc310_init();
        nrf_cc310_enable();
        rc = nrf_cc310_bl_ecdsa_verify_secp256r1(&ctx,
                                                 (nrf_cc310_bl_ecc_public_key_secp256r1_t *) public_key,
                                                 (nrf_cc310_bl_ecc_signature_secp256r1_t  *) signature,
                                                 hash,
                                                 hash_len);
        nrf_cc310_disable();
        return rc;
}

