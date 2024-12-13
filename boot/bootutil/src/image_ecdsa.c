/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2017 Linaro LTD
 * Copyright (C) 2021-2024 Arm Limited
 *
 * Original license:
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_SIGN_EC256) || defined(MCUBOOT_SIGN_EC384)

#include "bootutil_priv.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/crypto/ecdsa.h"

#if !defined(MCUBOOT_BUILTIN_KEY)
fih_ret
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
                    uint8_t key_id)
{
    int rc;
    bootutil_ecdsa_context ctx;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    uint8_t *pubkey;
    uint8_t *end;

    pubkey = (uint8_t *)bootutil_keys[key_id].key;
    end = pubkey + *bootutil_keys[key_id].len;
    bootutil_ecdsa_init(&ctx);

    rc = bootutil_ecdsa_parse_public_key(&ctx, &pubkey, end);
    if (rc) {
        goto out;
    }

    rc = bootutil_ecdsa_verify(&ctx, pubkey, end-pubkey, hash, hlen, sig, slen);
    fih_rc = fih_ret_encode_zero_equality(rc);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        FIH_SET(fih_rc, FIH_FAILURE);
    }

out:
    bootutil_ecdsa_drop(&ctx);

    FIH_RET(fih_rc);
}
#else /* !MCUBOOT_BUILTIN_KEY */
fih_ret
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
                    uint8_t key_id)
{
    int rc;
    bootutil_ecdsa_context ctx;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    /* Use builtin key for image verification, no key parsing is required. */
    ctx.key_id = key_id;
    bootutil_ecdsa_init(&ctx);

    /* The public key pointer and key size can be omitted. */
    rc = bootutil_ecdsa_verify(&ctx, NULL, 0, hash, hlen, sig, slen);
    fih_rc = fih_ret_encode_zero_equality(rc);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        FIH_SET(fih_rc, FIH_FAILURE);
    }

    bootutil_ecdsa_drop(&ctx);

    FIH_RET(fih_rc);
}
#endif /* MCUBOOT_BUILTIN_KEY */

#endif /* MCUBOOT_SIGN_EC256 || MCUBOOT_SIGN_EC384 */
