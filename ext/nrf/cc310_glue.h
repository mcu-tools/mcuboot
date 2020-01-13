/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef NRF_CC310_GLUE_H__
#define NRF_CC310_GLUE_H__

#include <nrf.h>
#include <nrf_cc310_bl_init.h>
#include <nrf_cc310_bl_hash_sha256.h>
#include <nrf_cc310_bl_ecdsa_verify_secp256r1.h>
#include <devicetree.h>
#include <string.h>

typedef nrf_cc310_bl_hash_context_sha256_t bootutil_sha256_context;

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

static inline void cc310_sha256_finalize(bootutil_sha256_context *ctx,
                                          uint8_t *output)
{
    nrf_cc310_bl_hash_sha256_finalize(ctx,
                                      (nrf_cc310_bl_hash_digest_sha256_t *)output);
    nrf_cc310_disable();
}

#endif /* NRF_CC310_GLUE_H__ */
