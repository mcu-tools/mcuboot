/*
 *  mbed Microcontroller Library
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Copyright (c) 2021 Infineon Technologies AG
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

#if defined(MBEDTLS_CONFIG_FILE)
#include MBEDTLS_CONFIG_FILE
#else
#include "config.h"
#endif

#if defined(MBEDTLS_SHA256_C)

#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_SHA256_ALT)

/* Uncomment for distinct error codes */
/* #define MAP_SPECIFIC_ERROR_CODES */

/* Cy_Cryptolite_Sha256_Update() fails with CY_CRYPTOLITE_BUS_ERROR for data
 * from Flash, if length is 64 bytes or more.
 */
#define CYW20829_SHA256_FLASH_WORKAROUND

/* Parameter validation macros based on platform_util.h */
#define SHA256_VALIDATE_RET(cond) \
    MBEDTLS_INTERNAL_VALIDATE_RET((cond), MBEDTLS_ERR_SHA256_BAD_INPUT_DATA)

#define SHA256_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

#ifdef CYW20829_SHA256_FLASH_WORKAROUND

#ifndef CYW20829
#error Workaround is only for CYW20829!
#endif /* CYW20829 */

#include "cyw20829_partition.h"

/* Largest safe length for Cy_Cryptolite_Sha256_Update() */
#define CYW20829_SHA256_SAFE_CHUNK_SIZE 63u

#endif /* CYW20829_SHA256_FLASH_WORKAROUND */

/**
 * \brief          Map Cryptolite status to mbed TLS error code.
 *
 * \param status   The \c CY_CRYPTOLITE_??? function status.
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
static
#ifndef MAP_SPECIFIC_ERROR_CODES
inline __attribute__((always_inline))
#endif /* MAP_SPECIFIC_ERROR_CODES */
int cryptolite_to_mbedtls(cy_en_cryptolite_status_t status)
{
    int rc = -1;

    switch (status) {
    case CY_CRYPTOLITE_SUCCESS:
        rc = 0;
        break;

#ifdef MAP_SPECIFIC_ERROR_CODES
    case CY_CRYPTOLITE_BAD_PARAMS:
        rc = MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
        break;

    case CY_CRYPTOLITE_HW_BUSY:
    case CY_CRYPTOLITE_BUS_ERROR:
        rc = MBEDTLS_ERR_SHA256_HW_ACCEL_FAILED;
        break;
#endif /* MAP_SPECIFIC_ERROR_CODES */

    default:
        break;
    }

    return rc;
}

/**
 * \brief          Zeroize memory block. There is no Cy_Crypto_Core_MemSet() in
 *                 Cryptolite, and no memset_s() in newlib-nano.
 *
 * \param buf      The buffer to zeroize.
 * \param len      The length of the buffer in Bytes.
 */
static inline __attribute__((always_inline))
void zeroize(void *buf, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)buf;

    while (len > 0u) {
        *p++ = 0u;
        len--;
    }
}

/**
 * \brief          This function initializes a SHA-256 context.
 *
 * \param ctx      The SHA-256 context to initialize. This must not be \c NULL.
 */
void mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
    cy_en_cryptolite_status_t status;

    SHA256_VALIDATE(ctx != NULL);
    zeroize(ctx, sizeof(*ctx));

    /* There is some chance crypto HW might be busy here */
    do {
        status = Cy_Cryptolite_Sha256_Init(CRYPTO, ctx);
    } while (CY_CRYPTOLITE_HW_BUSY == status);

    SHA256_VALIDATE(CY_CRYPTOLITE_SUCCESS == status);
}

/**
 * \brief          This function clears a SHA-256 context.
 *
 * \param ctx      The SHA-256 context to clear. This may be \c NULL, in which
 *                 case this function returns immediately. If it is not \c NULL,
 *                 it must point to an initialized SHA-256 context.
 */
void mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
    if (ctx != NULL) {
        (void)Cy_Cryptolite_Sha256_Free(CRYPTO, ctx);
        zeroize(ctx, sizeof(*ctx));
    }
}

/**
 * \brief          This function clones the state of a SHA-256 context.
 *
 * \param dst      The destination context. This must be initialized.
 * \param src      The context to clone. This must be initialized.
 */
void mbedtls_sha256_clone(mbedtls_sha256_context *dst,
                          const mbedtls_sha256_context *src)
{
    SHA256_VALIDATE(dst != NULL);
    SHA256_VALIDATE(src != NULL);

    *dst = *src;
}

/**
 * \brief          This function starts a SHA-224 or SHA-256 checksum
 *                 calculation.
 *                 WARNING: SHA-224 is NOT supported by Cryptolite!
 *
 * \param ctx      The context to use. This must be initialized.
 * \param is224    This determines which function to use. This must be
 *                 either \c 0 for SHA-256, or \c 1 for SHA-224.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_starts_ret(mbedtls_sha256_context *ctx, int is224)
{
    SHA256_VALIDATE_RET(ctx != NULL);
    SHA256_VALIDATE_RET(0u == is224);

    (void)is224;

    return cryptolite_to_mbedtls(
        Cy_Cryptolite_Sha256_Start(CRYPTO, ctx));
}

/**
 * \brief          This function feeds an input buffer into an ongoing
 *                 SHA-256 checksum calculation.
 *
 * \param ctx      The SHA-256 context. This must be initialized
 *                 and have a hash operation started.
 * \param input    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_update_ret(mbedtls_sha256_context *ctx,
                              const unsigned char *input,
                              size_t ilen)
{
    size_t offs = 0;
    SHA256_VALIDATE_RET(ctx != NULL);
    SHA256_VALIDATE_RET(0u == ilen || input != NULL);

#ifdef CYW20829_SHA256_FLASH_WORKAROUND
    /* Apply workaround only for data from Flash */
    if ((uintptr_t)input >= XIP_NS_CBUS &&
        (uintptr_t)(input + ilen) <= XIP_NS_CBUS + XIP_SIZE) {

        while (ilen > CYW20829_SHA256_SAFE_CHUNK_SIZE) {
            cy_en_cryptolite_status_t status =
                Cy_Cryptolite_Sha256_Update(CRYPTO,
                                            (uint8_t const *)input + offs,
                                            CYW20829_SHA256_SAFE_CHUNK_SIZE,
                                            ctx);

            if (CY_CRYPTOLITE_SUCCESS != status) {
                return cryptolite_to_mbedtls(status);
            }

            offs += CYW20829_SHA256_SAFE_CHUNK_SIZE;
            ilen -= CYW20829_SHA256_SAFE_CHUNK_SIZE;
        }
    }
#endif /* CYW20829_SHA256_FLASH_WORKAROUND */

    return cryptolite_to_mbedtls(
        Cy_Cryptolite_Sha256_Update(CRYPTO, (uint8_t const *)input + offs,
                                    (uint32_t)ilen, ctx));
}

/**
 * \brief          This function finishes the SHA-256 operation, and writes
 *                 the result to the output buffer.
 *
 * \param ctx      The SHA-256 context. This must be initialized
 *                 and have a hash operation started.
 * \param output   The SHA-224 or SHA-256 checksum result.
 *                 This must be a writable buffer of length \c 32 Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_finish_ret(mbedtls_sha256_context *ctx,
                              unsigned char output[32])
{
    SHA256_VALIDATE_RET(ctx != NULL);
    SHA256_VALIDATE_RET((unsigned char *)output != NULL);

    return cryptolite_to_mbedtls(
        Cy_Cryptolite_Sha256_Finish(CRYPTO, (uint8_t *)output, ctx));
}

/**
 * \brief          This function processes a single data block within
 *                 the ongoing SHA-256 computation. This function is for
 *                 internal use only.
 *
 * \param ctx      The SHA-256 context. This must be initialized.
 * \param data     The buffer holding one block of data. This must
 *                 be a readable buffer of length \c 64 Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_internal_sha256_process(mbedtls_sha256_context *ctx,
                                    const unsigned char data[64])
{
    return mbedtls_sha256_update_ret(ctx,
                                     data,
                                     CY_CRYPTOLITE_SHA256_BLOCK_SIZE);
}

#endif /* MBEDTLS_SHA256_ALT */

#endif /* MBEDTLS_SHA256_C */
