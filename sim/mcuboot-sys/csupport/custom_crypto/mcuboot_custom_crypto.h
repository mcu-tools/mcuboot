/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG
 *
 * Sim-only mcuboot_custom_crypto.h — satisfies the MCUBOOT_USE_CUSTOM_CRYPTO
 * contract for the simulator.
 *
 * This header must be included before any bootutil/crypto headers. It includes
 * sim_custom_*.h headers which provide typedef and static inline implementations
 * of the bootutil crypto API.
 */

#ifndef MCUBOOT_CUSTOM_CRYPTO_H
#define MCUBOOT_CUSTOM_CRYPTO_H

#ifndef __BOOTSIM__
#  error "csupport/custom_crypto/mcuboot_custom_crypto.h is a sim-only header. " \
         "Embedded builds must use a different include path."
#endif

/* Tell image_ecdsa.c to pass the DER signature unchanged to verify(). */
#define MCUBOOT_ECDSA_NEED_ASN1_SIG

/*
 * Include all sim custom crypto headers.
 * Each defines bootutil_*_context and provides static inline
 * implementations of the bootutil_* functions.
 */
#include "sim_custom_sha.h"
#include "sim_custom_ecdsa.h"
#if defined(MCUBOOT_ENC_IMAGES)
#include "sim_custom_aes_ctr.h"
#include "sim_custom_ecdh_p256.h"
#include "sim_custom_hmac_sha256.h"
#endif /* MCUBOOT_ENC_IMAGES */

#endif /* MCUBOOT_CUSTOM_CRYPTO_H */
