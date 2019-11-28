/*
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
#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

#include <syscfg/syscfg.h>

#if MYNEWT_VAL(BOOTUTIL_IMAGE_NUMBER)
#define MCUBOOT_IMAGE_NUMBER MYNEWT_VAL(BOOTUTIL_IMAGE_NUMBER)
#else
#define MCUBOOT_IMAGE_NUMBER 1
#endif
#if MYNEWT_VAL(BOOT_SERIAL)
#define MCUBOOT_SERIAL 1
#endif
#if MYNEWT_VAL(BOOTUTIL_VALIDATE_SLOT0)
#define MCUBOOT_VALIDATE_PRIMARY_SLOT 1
#endif
#if MYNEWT_VAL(BOOTUTIL_USE_MBED_TLS)
#define MCUBOOT_USE_MBED_TLS 1
#endif
#if MYNEWT_VAL(BOOTUTIL_USE_TINYCRYPT)
#define MCUBOOT_USE_TINYCRYPT 1
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_EC256)
#define MCUBOOT_SIGN_EC256 1
  #ifndef MCUBOOT_USE_TINYCRYPT
  #error "EC256 requires the use of tinycrypt."
  #endif
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_RSA)
#define MCUBOOT_SIGN_RSA 1
#define MCUBOOT_SIGN_RSA_LEN MYNEWT_VAL(BOOTUTIL_SIGN_RSA_LEN)
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_ED25519)
#define MCUBOOT_SIGN_ED25519 1
#endif
#if MYNEWT_VAL(BOOTUTIL_SIGN_EC)
#define MCUBOOT_SIGN_EC 1
#endif
#if MYNEWT_VAL(BOOTUTIL_ENCRYPT_RSA)
#define MCUBOOT_ENCRYPT_RSA 1
#endif
#if MYNEWT_VAL(BOOTUTIL_ENCRYPT_KW)
#define MCUBOOT_ENCRYPT_KW 1
#endif
#if MYNEWT_VAL(BOOTUTIL_ENCRYPT_EC256)
#define MCUBOOT_ENCRYPT_EC256 1
#endif
#if MYNEWT_VAL(BOOTUTIL_ENCRYPT_RSA) || MYNEWT_VAL(BOOTUTIL_ENCRYPT_KW) || \
    MYNEWT_VAL(BOOTUTIL_ENCRYPT_EC256)
#define MCUBOOT_ENC_IMAGES 1
#endif
#if MYNEWT_VAL(BOOTUTIL_SWAP_USING_MOVE)
#define MCUBOOT_SWAP_USING_MOVE 1
#endif
#if MYNEWT_VAL(BOOTUTIL_OVERWRITE_ONLY)
#define MCUBOOT_OVERWRITE_ONLY 1
#endif
#if MYNEWT_VAL(BOOTUTIL_OVERWRITE_ONLY_FAST)
#define MCUBOOT_OVERWRITE_ONLY_FAST 1
#endif
#if MYNEWT_VAL(BOOTUTIL_HAVE_LOGGING)
#define MCUBOOT_HAVE_LOGGING 1
#endif
#if MYNEWT_VAL(BOOTUTIL_BOOTSTRAP)
#define MCUBOOT_BOOTSTRAP 1
#endif

#define MCUBOOT_MAX_IMG_SECTORS       MYNEWT_VAL(BOOTUTIL_MAX_IMG_SECTORS)

#if MYNEWT_VAL(BOOTUTIL_FEED_WATCHDOG) && MYNEWT_VAL(WATCHDOG_INTERVAL)
#include <hal/hal_watchdog.h>
#define MCUBOOT_WATCHDOG_FEED()    \
    do {                           \
        hal_watchdog_tickle();     \
    } while (0)
#else
#define MCUBOOT_WATCHDOG_FEED()    do {} while (0)
#endif

#endif /* __MCUBOOT_CONFIG_H__ */
