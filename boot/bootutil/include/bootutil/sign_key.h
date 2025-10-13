/*
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef __BOOTUTIL_SIGN_KEY_H_
#define __BOOTUTIL_SIGN_KEY_H_

#include <stddef.h>
#include <stdint.h>

/* mcuboot_config.h is needed for MCUBOOT_HW_KEY to work */
#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_IMAGE_MULTI_SIG_SUPPORT
#include <stdbool.h>
#endif /* MCUBOOT_IMAGE_MULTI_SIG_SUPPORT */
#ifdef MCUBOOT_BUILTIN_KEY
#include "bootutil/fault_injection_hardening.h"
#endif /* MCUBOOT_BUILTIN_KEY */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MCUBOOT_HW_KEY
struct bootutil_key {
    const uint8_t *key;
    const unsigned int *len;
};

extern const struct bootutil_key bootutil_keys[];
#ifdef MCUBOOT_BUILTIN_KEY
/**
 * Verify that the specified key ID is valid for authenticating the given image.
 *
 * @param[in]  image_index   Index of the image to be verified.
 * @param[in]  key_id        Identifier of the key to be verified against the image.
 *
 * @return                   0 if the key ID is valid for the image; nonzero on failure.
 */
fih_ret boot_verify_key_id_for_image(uint8_t image_index, uint32_t key_id);
#endif /* MCUBOOT_BUILTIN_KEY */
#else
struct bootutil_key {
    uint8_t *key;
    unsigned int *len;
};

extern struct bootutil_key bootutil_keys[];

/**
 * Retrieve the hash of the corresponding public key for image authentication.
 *
 * @param[in]      image_index      Index of the image to be authenticated.
 * @param[in]      key_index        Index of the key to be used.
 * @param[out]     public_key_hash  Buffer to store the key-hash in.
 * @param[in,out]  key_hash_size    As input the size of the buffer. As output
 *                                  the actual key-hash length.
 *
 * @return                          0 on success; nonzero on failure.
 */
int boot_retrieve_public_key_hash(uint8_t image_index,
                                  uint8_t key_index,
                                  uint8_t *public_key_hash,
                                  size_t *key_hash_size);
#endif /* !MCUBOOT_HW_KEY */

#ifdef MCUBOOT_IMAGE_MULTI_SIG_SUPPORT
/**
 * @brief Checks the key policy for signature verification.
 *
 * Determines whether a given key might or must be used to sign an image,
 * based on the validity of the signature and the key index. Updates the
 * provided output parameters to reflect the policy.
 *
 * @param valid_sig            Indicates if the signature is valid.
 * @param key                  The key index to check.
 * @param[out] key_might_sign  Set to true if the key might be used to sign.
 * @param[out] key_must_sign   Set to true if the key must be used to sign.
 * @param[out] key_must_sign_count  Set to the number of keys that must sign.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int boot_plat_check_key_policy(bool valid_sig, uint32_t key,
                               bool *key_might_sign, bool *key_must_sign,
                               uint8_t *key_must_sign_count);
#endif /* MCUBOOT_IMAGE_MULTI_SIG_SUPPORT */

extern const int bootutil_key_cnt;

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_SIGN_KEY_H_ */
