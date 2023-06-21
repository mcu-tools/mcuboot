/*******************************************************************************
* \file cy_si_keystorage.h
* \version 1.10
*
* \brief
* Secure key storage header for the secure image.
*
********************************************************************************
* \copyright
* Copyright 2017-2018, Cypress Semiconductor Corporation. All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#ifndef CY_SI_KEYSTORAGE_H
#define CY_SI_KEYSTORAGE_H

#include <stdint.h>
#include <stddef.h>
#include "cy_syslib.h"

#if defined(__cplusplus)
extern "C" {
#endif

/***************************************
*               Macros
***************************************/

/** \addtogroup group_secure_image_macro
* \{
*/

/** \defgroup group_secure_image_pubkey_macros Public Key Macros
* Macros used to define the Public key.
* \{
*/    
#define CY_SI_PUBLIC_KEY_RSA_2048       (0UL)   /**< RSASSA-PKCS1-v1_5-2048 signature scheme */
#define CY_SI_PUBLIC_KEY_RSA_1024       (1UL)   /**< RSASSA-PKCS1-v1_5-1024 signature scheme */
#define CY_SI_PUBLIC_KEY_STRUCT_OFFSET  (8UL)   /**< Offset to public key struct in number of bytes */
#define CY_SI_PUBLIC_KEY_MODULOLENGTH   (256UL) /**< Modulus length of the RSA 2K key */
/* #define CY_SI_PUBLIC_KEY_MODULOLENGTH   (384UL) */ /**< Modulus length of the RSA 3K key */
/* #define CY_SI_PUBLIC_KEY_MODULOLENGTH   (512UL) */ /**< Modulus length of the RSA 4K key */
#define CY_SI_PUBLIC_KEY_EXPLENGTH      (32UL)  /**< Exponent length of the RSA key */
#define CY_SI_PUBLIC_KEY_SIZEOF_BYTE    (8UL)   /**< Size of Byte in number of bits */
/** \} group_secure_image_pubkey_macros */

/** \} group_secure_image_macro */


/***************************************
*               Structs
***************************************/

/**
* \addtogroup group_secure_image_data_structures
* \{
*/

/** Public key definition structure as expected by the Crypto driver */
typedef struct
{
    uint32_t moduloAddr;            /**< Address of the public key modulus */
    uint32_t moduloSize;            /**< Size (bits) of the modulus part of the public key */
    uint32_t expAddr;               /**< Address of the public key exponent */
    uint32_t expSize;               /**< Size (bits) of the exponent part of the public key */
    uint32_t barrettAddr;           /**< Address of the Barret coefficient */
    uint32_t inverseModuloAddr;     /**< Address of the binary inverse modulo */
    uint32_t rBarAddr;              /**< Address of the (2^moduloLength mod modulo) */
} cy_si_stc_crypto_public_key_t;

/** Public key structure */
typedef struct
{
    uint32_t objSize;                                           /**< Public key Object size */
    uint32_t signatureScheme;                                   /**< Signature scheme */
    cy_si_stc_crypto_public_key_t publicKeyStruct;              /**< Public key definition struct */
    uint8_t  moduloData[CY_SI_PUBLIC_KEY_MODULOLENGTH];         /**< Modulo data */
    uint8_t  expData[CY_SI_PUBLIC_KEY_EXPLENGTH];               /**< Exponent data */
    uint8_t  barrettData[CY_SI_PUBLIC_KEY_MODULOLENGTH + 4UL];  /**< Barret coefficient data */
    uint8_t  inverseModuloData[CY_SI_PUBLIC_KEY_MODULOLENGTH];  /**< Binary inverse modulo data */
    uint8_t  rBarData[CY_SI_PUBLIC_KEY_MODULOLENGTH];           /**< 2^moduloLength mod modulo data */
} cy_si_stc_public_key_t;

/** \} group_secure_image_data_structures */


/***************************************
*               Globals
***************************************/

/** Public key in SFlash */
extern const cy_si_stc_public_key_t cy_publicKey;

#if defined(__cplusplus)
}
#endif

#endif /* CY_SI_KEYSTORAGE_H */

/* [] END OF FILE */
