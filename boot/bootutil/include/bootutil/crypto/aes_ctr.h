/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, the choices are: MCUBOOT_USE_MBED_TLS, MCUBOOT_USE_TINYCRYPT
 * or MCUBOOT_USE_PSA_CRYPTO. Since MCUBOOT_USE_PSA_CRYPTO does not yet
 * support all the same abstraction as MCUBOOT_USE_MBED_TLS, the support for
 * PSA Crypto is built on top of mbed TLS, i.e. they must be both defined.
 */

#ifndef __BOOTUTIL_CRYPTO_AES_CTR_H_
#define __BOOTUTIL_CRYPTO_AES_CTR_H_

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_USE_PSA_CRYPTO) || defined(MCUBOOT_USE_MBED_TLS)
#define MCUBOOT_USE_PSA_OR_MBED_TLS
#endif /* MCUBOOT_USE_PSA_CRYPTO || MCUBOOT_USE_MBED_TLS */

#if (defined(MCUBOOT_USE_PSA_OR_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT) ) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT or PSA"
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
    #include "bootutil/crypto/aes_ctr_mbedtls.h"
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
    #include "bootutil/crypto/aes_ctr_tinycrypt.h"
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_PSA_CRYPTO)
    #include "bootutil/crypto/aes_ctr_psa.h"
#endif

#endif /* __BOOTUTIL_CRYPTO_AES_CTR_H_ */
