/*
 * Copyright (c) 2025 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __GENERATE_KEY_PAIR_H__
#define __GENERATE_KEY_PAIR_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"



int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);
int gen_p256_keypair(mbedtls_pk_context *pk);
void dump_p256(const mbedtls_pk_context *pk);
void show_public_key_formatted(const mbedtls_pk_context *pk);
int export_privkey_der(mbedtls_pk_context *pk,
                       unsigned char **der_ptr,
                       size_t *der_len);
int export_pub_pem(mbedtls_pk_context *pk);
int dump_pkcs8_der_as_c_array(const mbedtls_pk_context *pk);

#ifdef __cplusplus
}
#endif

#endif /* __GENERATE_KEY_PAIR_H__ */
