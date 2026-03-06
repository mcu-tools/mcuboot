/*
 * This module provides a thin abstraction over some of the crypto
 * primitives to make it easier to swap out the used crypto library.
 *
 * At this point, there are four choices: MCUBOOT_USE_MBED_TLS,
 * MCUBOOT_USE_TINYCRYPT, MCUBOOT_USE_PSA_CRYPTO, or
 * MCUBOOT_USE_CUSTOM_CRYPTO.  It is a compile error if there is not
 * exactly one of these defined.
 */

#ifndef __BOOTUTIL_CRYPTO_AES_CTR_H_
#define __BOOTUTIL_CRYPTO_AES_CTR_H_

#include "mcuboot_config/mcuboot_config.h"

#if (defined(MCUBOOT_USE_MBED_TLS) + \
     defined(MCUBOOT_USE_TINYCRYPT) + \
     defined(MCUBOOT_USE_PSA_CRYPTO) + \
     defined(MCUBOOT_USE_CUSTOM_CRYPTO)) != 1
    #error "One crypto backend must be defined: either MBED_TLS or TINYCRYPT or PSA or CUSTOM_CRYPTO"
#endif

#if defined(MCUBOOT_USE_MBED_TLS)
    #include "bootutil/crypto/aes_ctr_mbedtls.h"
#endif /* MCUBOOT_USE_MBED_TLS */

#if defined(MCUBOOT_USE_TINYCRYPT)
    #include "bootutil/crypto/aes_ctr_tinycrypt.h"
#endif /* MCUBOOT_USE_TINYCRYPT */

#if defined(MCUBOOT_USE_PSA_CRYPTO)
    #include "bootutil/crypto/aes_ctr_psa.h"
#endif /* MCUBOOT_USE_PSA_CRYPTO */

#endif /* __BOOTUTIL_CRYPTO_AES_CTR_H_ */
