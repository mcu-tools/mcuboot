/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2018-2019 JUUL Labs
 * Copyright (c) 2019-2021 Arm Limited
 * Copyright (c) 2021 Nordic Semiconductor ASA
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

#ifndef BOOTUTIL_ENC_KEY_PUBLIC_H
#define BOOTUTIL_ENC_KEY_PUBLIC_H
#include <mcuboot_config/mcuboot_config.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef MCUBOOT_AES_256
#define BOOT_ENC_KEY_SIZE       32
#else
#define BOOT_ENC_KEY_SIZE       16
#endif

#define BOOT_ENC_KEY_ALIGN_SIZE \
    ((((BOOT_ENC_KEY_SIZE - 1) / BOOT_MAX_ALIGN) + 1) * BOOT_MAX_ALIGN)


#define TLV_ENC_RSA_SZ    256
#define TLV_ENC_KW_SZ     BOOT_ENC_KEY_SIZE + 8
#define TLV_ENC_EC256_SZ  (65 + 32 + BOOT_ENC_KEY_SIZE)
#define TLV_ENC_X25519_SZ (32 + 32 + BOOT_ENC_KEY_SIZE)

#if defined(MCUBOOT_ENCRYPT_RSA)
#define BOOT_ENC_TLV_SIZE TLV_ENC_RSA_SZ
#elif defined(MCUBOOT_ENCRYPT_EC256)
#define BOOT_ENC_TLV_SIZE TLV_ENC_EC256_SZ
#elif defined(MCUBOOT_ENCRYPT_X25519)
#define BOOT_ENC_TLV_SIZE TLV_ENC_X25519_SZ
#else
#define BOOT_ENC_TLV_SIZE TLV_ENC_KW_SZ
#endif

#ifdef __cplusplus
}
#endif

#endif /* BOOTUTIL_ENC_KEY_PUBLIC_H */
