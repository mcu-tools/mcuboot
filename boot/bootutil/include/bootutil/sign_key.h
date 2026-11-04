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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MCUBOOT_HW_KEY
struct bootutil_key {
    const uint8_t *key;
    const unsigned int *len;
};

extern const struct bootutil_key bootutil_keys[];
#else
struct bootutil_key {
    uint8_t *key;
    unsigned int *len;
};

extern struct bootutil_key bootutil_keys[];

/**
 * Special return code for boot_retrieve_public_key_hash().
 *
 * Returned when the requested key index is out of range (no more keys).
 */
#define BOOTUTIL_HW_KEY_NO_MORE 1

/**
 * Retrieve a HW key hash by index (multi-key support).
 *
 * @param[in]      image_index      Index of the image to be authenticated.
 * @param[in]      key_index        Index of the HW key hash to retrieve.
 * @param[out]     public_key_hash  Buffer to store the key-hash in.
 * @param[in,out]  key_hash_size    As input the size of the buffer. As output
 *                                  the actual key-hash length.
 *
 * @return                          0 on success; BOOTUTIL_HW_KEY_NO_MORE when
 *                                  key_index is out of range; other nonzero on
 *                                  failure.
 */
int boot_retrieve_public_key_hash(uint8_t image_index,
                                  uint8_t key_index,
                                  uint8_t *public_key_hash,
                                  size_t *key_hash_size);
#endif /* !MCUBOOT_HW_KEY */

extern const int bootutil_key_cnt;

#ifdef __cplusplus
}
#endif

#endif /* __BOOTUTIL_SIGN_KEY_H_ */
