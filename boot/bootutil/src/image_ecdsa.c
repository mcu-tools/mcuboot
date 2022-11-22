/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2023 Arm Limited
 */

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SIGN_ECDSA
#include "bootutil_priv.h"
#include "bootutil/sign_key.h"
#include "bootutil/fault_injection_hardening.h"

#include "bootutil/crypto/ecdsa.h"

static fih_ret
bootutil_cmp_ecdsa_sig(bootutil_ecdsa_context *ctx, uint8_t *hash, uint32_t hlen,
  uint8_t *sig, size_t slen)
{
    int rc = -1;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    /* PSA Crypto APIs allow the verification in a single call */
    rc = bootutil_ecdsa_verify(ctx, hash, hlen, sig, slen);

    fih_rc = fih_ret_encode_zero_equality(rc);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        FIH_SET(fih_rc, FIH_FAILURE);
    }

    FIH_RET(fih_rc);
}

fih_ret
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
  uint8_t key_id)
{
    int rc = -1;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    uint8_t *cp;
    uint8_t *end;
    bootutil_ecdsa_context ctx;

    bootutil_ecdsa_init(&ctx);

    cp = (uint8_t *)bootutil_keys[key_id].key;
    end = cp + *bootutil_keys[key_id].len;

    /* The key used for signature verification is a public ECDSA key */
    rc = bootutil_ecdsa_parse_public_key(&ctx, &cp, end);
    if (rc) {
        goto out;
    }

    FIH_CALL(bootutil_cmp_ecdsa_sig, fih_rc, &ctx, hash, hlen, sig, slen);

out:
    bootutil_ecdsa_drop(&ctx);

    FIH_RET(fih_rc);
}
#endif /* MCUBOOT_SIGN_ECDSA */
