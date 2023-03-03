/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2017 Linaro LTD
 * Copyright (C) 2021-2023 Arm Limited
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

#ifdef MCUBOOT_SIGN_EC256

#include "bootutil_priv.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/crypto/ecdsa_p256.h"

fih_int
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, size_t slen,
                    uint8_t key_id)
{
    int rc;
    bootutil_ecdsa_p256_context ctx;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    uint8_t *pubkey;
    uint8_t *end;

    pubkey = (uint8_t *)bootutil_keys[key_id].key;
    end = pubkey + *bootutil_keys[key_id].len;
    bootutil_ecdsa_p256_init(&ctx);

    rc = bootutil_ecdsa_p256_parse_public_key(&ctx, &pubkey, end);
    if (rc) {
        FIH_SET(fih_rc, FIH_FAILURE);
        FIH_RET(fih_rc);
    }

    FIH_CALL(bootutil_ecdsa_p256_verify, fih_rc, &ctx, pubkey, end-pubkey, hash, hlen, sig, slen);
    bootutil_ecdsa_p256_drop(&ctx);

    FIH_RET(fih_rc);
}

#endif /* MCUBOOT_SIGN_EC256 */
