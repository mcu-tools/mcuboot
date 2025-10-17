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

#include "bootutil/bootutil_log.h"
#include "bootutil/crypto/sha.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/image.h"
#include "bootutil/sign_key.h"
#include "bootutil_priv.h"
#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_IMAGE_MULTI_SIG_SUPPORT
#define NUM_OF_KEYS MCUBOOT_ROTPK_MAX_KEYS_PER_IMAGE
#else
#define NUM_OF_KEYS 1
#endif /* MCUBOOT_IMAGE_MULTI_SIG_SUPPORT */

#if defined(MCUBOOT_BUILTIN_KEY)
int bootutil_find_key(uint8_t image_index, uint8_t *key_id_buf, uint8_t key_id_buf_len)
{
    uint32_t key_id;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    BOOT_LOG_DBG("bootutil_find_key: image_index %d", image_index);
    /* Key id is passed */
    assert(key_id_buf_len == sizeof(uint32_t));
    memcpy(&key_id, key_id_buf, sizeof(key_id));

    /* Check if key id is associated with the image */
    FIH_CALL(boot_verify_key_id_for_image, fih_rc, image_index, key_id);
    if (FIH_EQ(fih_rc, FIH_SUCCESS)) {
        return (int32_t)key_id;
    }

    return -1;
}

#elif defined(MCUBOOT_HW_KEY)
extern unsigned int pub_key_len;
static int last_hw_key_index = -1;
int bootutil_get_last_hw_key_index(void)
{
    return last_hw_key_index;
}
int bootutil_find_key(uint8_t image_index, uint8_t *key, uint16_t key_len)
{
    bootutil_sha_context sha_ctx;
    uint8_t hash[IMAGE_HASH_SIZE];
    uint8_t key_hash[IMAGE_HASH_SIZE];
    size_t key_hash_size = sizeof(key_hash);
    int rc;
    uint8_t key_index;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    bootutil_sha_init(&sha_ctx);
    bootutil_sha_update(&sha_ctx, key, key_len);
    bootutil_sha_finish(&sha_ctx, hash);
    bootutil_sha_drop(&sha_ctx);

    BOOT_LOG_DBG("bootutil_find_key: image_index %d", image_index);
    for(key_index = 0; key_index < NUM_OF_KEYS; key_index++) {
        last_hw_key_index = -1;
        rc = boot_retrieve_public_key_hash(image_index, key_index, key_hash, &key_hash_size);
        if (rc) {
            return -1;
        }

        /* Adding hardening to avoid this potential attack:
         *  - Image is signed with an arbitrary key and the corresponding public
         *    key is added as a TLV field.
         * - During public key validation (comparing against key-hash read from
         *   HW) a fault is injected to accept the public key as valid one.
         */
        FIH_CALL(boot_fih_memequal, fih_rc, hash, key_hash, key_hash_size);
        if (FIH_EQ(fih_rc, FIH_SUCCESS)) {
            BOOT_LOG_INF("Key hash matched for image %u at slot %u", image_index, key_index);
            bootutil_keys[0].key = key;
            pub_key_len = key_len;
            last_hw_key_index = key_index;
            return 0;
        }
    }
    BOOT_LOG_ERR("Key hash NOT found for image %d!", image_index);
    last_hw_key_index = -1;

    return -1;
}

#else /* !defined MCUBOOT_BUILTIN_KEY && !defined MCUBOOT_HW_KEY */
int bootutil_find_key(uint8_t image_index, uint8_t *keyhash, uint8_t keyhash_len)
{
    bootutil_sha_context sha_ctx;
    int i;
    uint8_t hash[IMAGE_HASH_SIZE];
    const struct bootutil_key *key;
    (void)image_index;

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
#endif
