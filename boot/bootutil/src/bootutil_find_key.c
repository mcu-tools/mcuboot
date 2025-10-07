/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2019 Linaro LTD
 * Copyright (c) 2016-2019 JUUL Labs
 * Copyright (c) 2019-2025 Arm Limited
 * Copyright (c) 2025 Nordic Semiconductor ASA
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

#include <stdint.h>

#include "bootutil/crypto/sha.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/image.h"
#include "bootutil/sign_key.h"
#include "bootutil_priv.h"
#include "mcuboot_config/mcuboot_config.h"
#include "bootutil/bootutil_log.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#if defined(MCUBOOT_SIGN_RSA)       || \
    defined(MCUBOOT_SIGN_EC256)     || \
    defined(MCUBOOT_SIGN_EC384)     || \
    defined(MCUBOOT_SIGN_EC)        || \
    defined(MCUBOOT_SIGN_ED25519)
#define EXPECTED_SIG_TLV
#else
    /* no signing, sha256 digest only */
#endif

#ifdef EXPECTED_SIG_TLV
#if !defined(MCUBOOT_BYPASS_KEY_MATCH)
/* Find functions are only needed when key is checked first */
#if !defined(MCUBOOT_BUILTIN_KEY)
#if !defined(MCUBOOT_HW_KEY)
int bootutil_find_key(uint8_t *keyhash, uint8_t keyhash_len)
{
    bootutil_sha_context sha_ctx;
    int i;
    const struct bootutil_key *key;
    uint8_t hash[IMAGE_HASH_SIZE];

    BOOT_LOG_DBG("bootutil_find_key");

    if (keyhash_len > IMAGE_HASH_SIZE) {
        return -1;
    }

    for (i = 0; i < bootutil_key_cnt; i++) {
        key = &bootutil_keys[i];
        bootutil_sha_init(&sha_ctx);
        bootutil_sha_update(&sha_ctx, key->key, *key->len);
        bootutil_sha_finish(&sha_ctx, hash);
        bootutil_sha_drop(&sha_ctx);
        if (!memcmp(hash, keyhash, keyhash_len)) {
            return i;
        }
    }
    return -1;
}
#else /* !MCUBOOT_HW_KEY */
extern unsigned int pub_key_len;
int bootutil_find_key(uint8_t image_index, uint8_t *key, uint16_t key_len)
{
    bootutil_sha_context sha_ctx;
    uint8_t hash[IMAGE_HASH_SIZE];
    uint8_t key_hash[IMAGE_HASH_SIZE];
    size_t key_hash_size = sizeof(key_hash);
    int rc;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    BOOT_LOG_DBG("bootutil_find_key: image_index %d", image_index);

    bootutil_sha_init(&sha_ctx);
    bootutil_sha_update(&sha_ctx, key, key_len);
    bootutil_sha_finish(&sha_ctx, hash);
    bootutil_sha_drop(&sha_ctx);

    rc = boot_retrieve_public_key_hash(image_index, key_hash, &key_hash_size);
    if (rc) {
        return -1;
    }

    /* Adding hardening to avoid this potential attack:
     * - Image is signed with an arbitrary key and the corresponding public
     *   key is added as a TLV field.
     * - During public key validation (comparing against key-hash read from
     *   HW) a fault is injected to accept the public key as valid one.
     */
    FIH_CALL(boot_fih_memequal, fih_rc, hash, key_hash, key_hash_size);
    if (FIH_EQ(fih_rc, FIH_SUCCESS)) {
        bootutil_keys[0].key = key;
        pub_key_len = key_len;
        return 0;
    }

    return -1;
}
#endif /* !MCUBOOT_HW_KEY */
#endif /* !MCUBOOT_BUILTIN_KEY */

#else /* !MCUBOOT_BYPASS_KEY_MATCH */
int bootutil_find_key(uint8_t image_index, uint8_t *key, uint16_t key_len)
{
    (void)image_index;
    (void)key;
    (void)key_len;

    /* There is only one key, so it always matches */
    return 0;
}
#endif /* !MCUBOOT_BYPASS_KEY_MATCH */
#endif /* EXPECTED_SIG_TLV */
