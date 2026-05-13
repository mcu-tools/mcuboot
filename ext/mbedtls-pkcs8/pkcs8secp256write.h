/*
 * Copyright (c) 2026 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __PKCS8SECP256WRITE_H__
#define __PKCS8SECP256WRITE_H__

#include <stddef.h>

typedef struct mbedtls_pk_context mbedtls_pk_context;

/**
 * @brief Write a private key in PKCS#8 DER format
 * 
 * @param key   Pointer to mbedtls_pk_context containing the private key.
 * @param buf   Output buffer where the DER structure will be written.
 * @param size  Size of the output buffer in bytes.
 * 
 * @return      Length of the DER-encoded key.
 * @return      MBEDTLS_ERR_ASN1_BUF_TOO_SMALL if the buffer is to small
 * @return      MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE if the key type is not supported
 */
int mbedtls_pk_write_keypkcs8_der(const mbedtls_pk_context *key, unsigned char *buf, size_t size);

#endif /*__PKCS8SECP256WRITE_H__*/