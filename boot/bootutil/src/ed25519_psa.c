/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <mcuboot_config/mcuboot_config.h>
#include "bootutil/bootutil_log.h"

#include <psa/crypto.h>
#include <psa/crypto_types.h>

BOOT_LOG_MODULE_REGISTER(ed25519_psa);

#define SHA512_DIGEST_LENGTH    64
#define EDDSA_KEY_LENGTH        32
#define EDDSA_SIGNAGURE_LENGTH  64

int ED25519_verify(const uint8_t *message, size_t message_len,
                   const uint8_t signature[EDDSA_SIGNAGURE_LENGTH],
                   const uint8_t public_key[EDDSA_KEY_LENGTH])
{
    /* Set to any error */
    psa_status_t status = PSA_ERROR_BAD_STATE;
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t kid;
    int ret = 0;        /* Fail by default */

    BOOT_LOG_DBG("ED25519_verify: PSA implementation");

    /* Initialize PSA Crypto */
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        BOOT_LOG_ERR("PSA crypto init failed %d\n", status);
        return 0;
    }

    status = PSA_ERROR_BAD_STATE;

    psa_set_key_type(&key_attr,
                     PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_TWISTED_EDWARDS));
    psa_set_key_usage_flags(&key_attr, PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&key_attr, PSA_ALG_PURE_EDDSA);

    status = psa_import_key(&key_attr, public_key, EDDSA_KEY_LENGTH, &kid);
    if (status != PSA_SUCCESS) {
        BOOT_LOG_ERR("ED25519 key import failed %d", status);
        return 0;
    }

    status = psa_verify_message(kid, PSA_ALG_PURE_EDDSA, message, message_len,
                                signature, EDDSA_SIGNAGURE_LENGTH);
    if (status != PSA_SUCCESS) {
        BOOT_LOG_ERR("ED25519 signature verification failed %d", status);
        ret = 0;
        /* Pass through to destroy key */
    } else {
	ret = 1;
        /* Pass through to destroy key */
    }

    status = psa_destroy_key(kid);

    if (status != PSA_SUCCESS) {
        /* Just for logging */
        BOOT_LOG_WRN("Failed to destroy key %d", status);
    }

    return ret;
}
