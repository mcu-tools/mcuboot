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

#ifndef NRF_CC310_GLUE_H__
#define NRF_CC310_GLUE_H__

#include <nrf.h>
#include <nrf_cc310_bl_init.h>
#include <nrf_cc310_bl_hash_sha256.h>
#include <nrf_cc310_bl_ecdsa_verify_secp256r1.h>
#include <devicetree.h>
#include <string.h>

/*
 * Name translation for peripherals with only one type of access available.
 */
#if !defined(NRF_TRUSTZONE_NONSECURE) && defined(CONFIG_ARM_TRUSTZONE_M)
#define NRF_CRYPTOCELL   NRF_CRYPTOCELL_S
#endif

typedef nrf_cc310_bl_hash_context_sha256_t bootutil_sha_context;

int cc310_ecdsa_verify_secp256r1(uint8_t *hash,
                                 uint8_t *public_key,
                                 uint8_t *signature,
                                 size_t hash_len);


int cc310_init(void);

static inline void cc310_sha256_init(nrf_cc310_bl_hash_context_sha256_t *ctx);

void cc310_sha256_update(nrf_cc310_bl_hash_context_sha256_t *ctx,
                         const void *data,
                         uint32_t data_len);

static inline void nrf_cc310_enable(void)
{
    NRF_CRYPTOCELL->ENABLE=1;
}

static inline void nrf_cc310_disable(void)
{
    NRF_CRYPTOCELL->ENABLE=0;
}

/* Enable and disable cc310 to reduce power consumption */
static inline void cc310_sha256_init(nrf_cc310_bl_hash_context_sha256_t * ctx)
{
    cc310_init();
    nrf_cc310_enable();
    nrf_cc310_bl_hash_sha256_init(ctx);
}

static inline void cc310_sha256_finalize(nrf_cc310_bl_hash_context_sha256_t *ctx,
                                          uint8_t *output)
{
    nrf_cc310_bl_hash_sha256_finalize(ctx,
                                      (nrf_cc310_bl_hash_digest_sha256_t *)output);
    nrf_cc310_disable();
}

#endif /* NRF_CC310_GLUE_H__ */
