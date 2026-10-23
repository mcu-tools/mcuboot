/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
 */

#ifndef MBEDTLS_USER_CONFIG_H_
#define MBEDTLS_USER_CONFIG_H_

/*
 * The ThreadX MCUboot port uses TinyCrypt for ECDSA P-256 arithmetic.
 * MPI/Bignum operations are not required from mbed TLS ASN.1 parser.
 */
#undef MBEDTLS_BIGNUM_C

#endif /* MBEDTLS_USER_CONFIG_H_ */
