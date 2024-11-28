/*******************************************************************************
* \file cyboot_crypto_list.h
* \version 1.0.0
* Provides header file for crypto API.
********************************************************************************
* \copyright
* (c) 2023, Cypress Semiconductor Corporation (an Infineon company) or an
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

#ifndef CY_FB_CRYPTO_API_H
#define CY_FB_CRYPTO_API_H

#include <stdint.h>

/** \cond INTERNAL */
/** A size in words of a SHA-256 result */
#define CYBOOT_HASH_RESULT_SIZE_IN_WORDS (8U)
#define CYBOOT_CONTEXT_SIZE_IN_WORDS (100U)

typedef uint32_t cyboot_hash_result_t[CYBOOT_HASH_RESULT_SIZE_IN_WORDS];
typedef uint32_t cyboot_sha256_context_t[CYBOOT_CONTEXT_SIZE_IN_WORDS];

/********************************************************************************
*  This function does a SHA-256 checksum calculation.
* \param data   Input data for hash calculating
* \param size   Syze in bytes of input data
* \param hash   Calculated hash
*
* \return
* \ref CYBOOT_CRYPTO_SUCCESS if successful
*******************************************************************************/
typedef uint32_t (*cyboot_ecdsa_p256_signature_validate_t)(uint8_t *pub_key, uint32_t pub_key_len,
                                                                  uint8_t *hash, uint32_t hash_len,
                                                                  uint8_t *sign, uint32_t sign_len);

/********************************************************************************
*  This function does a SHA-256 checksum calculation.
* \param data   Input data for hash calculating
* \param size   Syze in bytes of input data
* \param hash   Calculated hash
*
* \return
* \ref CYBOOT_CRYPTO_SUCCESS if successful
*******************************************************************************/
typedef uint32_t (*cyboot_sha256_ret_t)(const uint8_t *data, uint32_t size, cyboot_hash_result_t hash);

/********************************************************************************
* This function finishes the SHA-256 operation, and writes the result to the
* output buffer.
*
* \param ctx The SHA-256 context.
*
* \param output The SHA-256 checksum result.
*
* \return
* \ref CYBOOT_CRYPTO_SUCCESS if successful
*******************************************************************************/
typedef uint32_t (*cyboot_sha256_finish_t)(cyboot_sha256_context_t *ctx, cyboot_hash_result_t output);

/********************************************************************************
* This function feeds an input buffer into an ongoing SHA-256 checksum
* calculation.
*
* \param ctx
* The SHA-256 context. This must be initialized and have a hash operation started.
*
* \param data
* The buffer holding the data. This must be a readable buffer of length
* \p data_len in bytes
*
* \param data_len
* The length of the input data in bytes.
*
* \return
* \ref CYBOOT_CRYPTO_SUCCESS if successful
*******************************************************************************/
typedef uint32_t (*cyboot_sha256_update_t)(cyboot_sha256_context_t *ctx, const uint8_t *data, uint32_t data_len);

/********************************************************************************
*  This function starts a SHA-256 checksum calculation.
* \param ctx The SHA-256 context to initialize.
*
* \return
* \ref CYBOOT_CRYPTO_SUCCESS if successful
*******************************************************************************/
typedef uint32_t (*cyboot_sha256_init_t)(cyboot_sha256_context_t *ctx);

typedef struct
{
    cyboot_sha256_init_t sha256_init;
    cyboot_sha256_update_t sha256_update;
    cyboot_sha256_finish_t sha256_finish;
    cyboot_sha256_ret_t sha256_ret;
    cyboot_ecdsa_p256_signature_validate_t ecdsa_p256_signature_validate;
} cy_boot_crypto_api_t;

#define BOOTROM_CRYPTO_API ((cy_boot_crypto_api_t *)0x1080FFB8)
#define CYBOOT_CRYPTO_SUCCESS 0x0D50B002

/** \endcond */
#endif
/* [] END OF FILE */
