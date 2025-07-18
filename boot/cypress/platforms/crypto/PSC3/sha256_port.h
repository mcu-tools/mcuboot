/*******************************************************************************
 * \file sha256_port.h
 * \version 1.0.0
 * Provides port layer for SHA256 MCUBoot functions
 ********************************************************************************
 * \copyright
 * (c) 2025, Cypress Semiconductor Corporation (an Infineon company) or an
 * affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 * This software, associated documentation and materials ("Software") is owned
 * by Cypress Semiconductor Corporation or one of its affiliates ("Cypress") and
 * is protected by and subject to worldwide patent protection (United States and
 * foreign), United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you obtained this
 * Software ("EULA"). If no EULA applies, then any reproduction, modification,
 * translation, compilation, or representation of this Software is prohibited
 * without the express written permission of Cypress.
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * Cypress reserves the right to make changes to the Software without notice.
 * Cypress does not assume any liability arising out of the application or use
 * of the Software or any product or circuit described in the Software. Cypress
 * does not authorize its products for use in any products where a malfunction
 * or failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product").
 * By including Cypress's product in a High Risk Product, the manufacturer of
 * such system or application assumes all risk of such use and in doing so
 * agrees to indemnify Cypress against all liability.
 *******************************************************************************/

#pragma once

#include "cyboot_crypto_list.h"
#include "cy_cryptolite_common.h"

#include "cy_cryptolite_sha256.h"

#define BOOTUTIL_CRYPTO_SHA256_BLOCK_SIZE (64)
#define BOOTUTIL_CRYPTO_SHA256_DIGEST_SIZE (32)

typedef cyboot_sha256_context_t bootutil_sha256_context;

static inline void bootutil_sha256_init(bootutil_sha256_context *ctx)
{
    BOOTROM_CRYPTO_API->sha256_init(ctx);
}

static inline void bootutil_sha256_drop(bootutil_sha256_context *ctx)
{
    (void)ctx;
}

static inline int bootutil_sha256_update(bootutil_sha256_context *ctx,
                                         const void *data,
                                         uint32_t data_len)
{
    int ret = -1;
    uint32_t result;

    uint32_t bytes_left = data_len;

    uint8_t* data_bytes = (uint8_t*)data;
    uint8_t tmp_buf[CY_CRYPTOLITE_SHA256_BLOCK_SIZE] = {0U};

    while(bytes_left > CY_CRYPTOLITE_SHA256_BLOCK_SIZE)
    {
        memcpy(tmp_buf, data_bytes, CY_CRYPTOLITE_SHA256_BLOCK_SIZE);

        result = BOOTROM_CRYPTO_API->sha256_update(ctx, CY_REMAP_ADDRESS_CRYPTOLITE(tmp_buf), CY_CRYPTOLITE_SHA256_BLOCK_SIZE);

        data_bytes += CY_CRYPTOLITE_SHA256_BLOCK_SIZE;

        bytes_left -= CY_CRYPTOLITE_SHA256_BLOCK_SIZE;

        if (result != CYBOOT_CRYPTO_SUCCESS)
        {
            return -1;
        }
    }

    memcpy(tmp_buf, data_bytes, bytes_left);

    result = BOOTROM_CRYPTO_API->sha256_update(ctx, CY_REMAP_ADDRESS_CRYPTOLITE(tmp_buf), bytes_left);

    if (result == CYBOOT_CRYPTO_SUCCESS) {
        ret = 0;
    }

    return ret;
}

static inline int bootutil_sha256_finish(bootutil_sha256_context *ctx,
                                         uint8_t *output)
{
    int ret = -1;

    uint32_t result = BOOTROM_CRYPTO_API->sha256_finish(ctx, (uint32_t*)output);

    if (result == CYBOOT_CRYPTO_SUCCESS) {
        ret = 0;
    }

    return ret;
}