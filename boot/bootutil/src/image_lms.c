/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Linaro LTD
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#ifdef MCUBOOT_SIGN_LMS

#include <mbedtls/lms.h>
#include <psa/crypto.h>

#include "bootutil_priv.h"
#include "bootutil/sign_key.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/image.h"
#include "bootutil/crypto/sha.h"

/*
 * LMS image-signature verification.
 *
 * mcuboot signs SHA-256(image + protected TLVs) rather than the image
 * bytes themselves — see TlvGen's LMS branch for the rationale (a
 * bootloader can't always load the full image into RAM, and the mbedtls
 * LMS verify API takes a single contiguous buffer). RFC 8554 is
 * agnostic about what bytes the application calls "the message"; both
 * sides agree on the digest, so the security argument is unchanged
 * (LMS's own internal hash is also SHA-256, so a SHA-256 break would
 * already break LMS).
 *
 * Key material lives in bootutil_keys[key_id] as the 56-byte serialized
 * public key per RFC 8554 §5.3 (lms_type | lmots_type | I | T[1]).
 */
fih_ret
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
                    uint8_t key_id)
{
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    mbedtls_lms_public_t ctx;
    psa_status_t psa_rc;
    int rc;

    BOOT_LOG_DBG("bootutil_verify_sig: LMS key_id %d", (int)key_id);

    if (hlen != IMAGE_HASH_SIZE) {
        BOOT_LOG_DBG("bootutil_verify_sig: expected hash len %d, got %u",
                     IMAGE_HASH_SIZE, (unsigned int)hlen);
        FIH_RET(fih_rc);
    }

    if (slen != MBEDTLS_LMS_SIG_LEN(MBEDTLS_LMS_SHA256_M32_H10,
                                    MBEDTLS_LMOTS_SHA256_N32_W8)) {
        BOOT_LOG_DBG("bootutil_verify_sig: expected slen %u, got %u",
                     (unsigned)MBEDTLS_LMS_SIG_LEN(
                         MBEDTLS_LMS_SHA256_M32_H10,
                         MBEDTLS_LMOTS_SHA256_N32_W8),
                     (unsigned)slen);
        FIH_RET(fih_rc);
    }

    psa_rc = psa_crypto_init();
    if (psa_rc != PSA_SUCCESS) {
        BOOT_LOG_ERR("PSA crypto init failed %d", (int)psa_rc);
        FIH_RET(fih_rc);
    }

    mbedtls_lms_public_init(&ctx);

    rc = mbedtls_lms_import_public_key(&ctx,
                                       bootutil_keys[key_id].key,
                                       *bootutil_keys[key_id].len);
    if (rc != 0) {
        BOOT_LOG_ERR("LMS import_public_key failed %d", rc);
        goto out;
    }

    rc = mbedtls_lms_verify(&ctx, hash, hlen, sig, slen);
    if (rc == 0) {
        FIH_SET(fih_rc, FIH_SUCCESS);
    } else {
        BOOT_LOG_DBG("LMS verify failed %d", rc);
    }

out:
    mbedtls_lms_public_free(&ctx);
    FIH_RET(fih_rc);
}

#endif /* MCUBOOT_SIGN_LMS */
