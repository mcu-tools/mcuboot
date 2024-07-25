/*
 * Copyright (c) 2020 Embedded Planet
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#include <bootutil/sign_key.h>
#include <mcuboot_config/mcuboot_config.h>

#if defined(MCUBOOT_SIGN_RSA)
#define HAVE_KEYS
extern const unsigned char rsa_pub_key[];
extern unsigned int rsa_pub_key_len;
#elif defined(MCUBOOT_SIGN_EC256)
#define HAVE_KEYS
extern const unsigned char ecdsa_pub_key[];
extern unsigned int ecdsa_pub_key_len;
#elif defined(MCUBOOT_SIGN_ED25519)
#define HAVE_KEYS
extern const unsigned char ed25519_pub_key[];
extern unsigned int ed25519_pub_key_len;
#endif

/*
 * Note: Keys for both signing and encryption must be provided by the application.
 * mcuboot's imgtool utility can be used to generate these keys and convert them into compatible C code.
 * See imgtool's documentation, specifically the section: "Incorporating the public key into the code" which can be found here:
 * https://github.com/JuulLabs-OSS/mcuboot/blob/master/docs/imgtool.md#incorporating-the-public-key-into-the-code
 */
#if defined(HAVE_KEYS)
const struct bootutil_key bootutil_keys[] = {
    {
#if defined(MCUBOOT_SIGN_RSA)
        .key = rsa_pub_key,
        .len = &rsa_pub_key_len,
#elif defined(MCUBOOT_SIGN_EC256)
        .key = ecdsa_pub_key,
        .len = &ecdsa_pub_key_len,
#elif defined(MCUBOOT_SIGN_ED25519)
        .key = ed25519_pub_key,
        .len = &ed25519_pub_key_len,
#endif
    },
};
const int bootutil_key_cnt = 1;

#if defined(MCUBOOT_ENCRYPT_RSA)

extern const unsigned char enc_priv_key[];
extern const unsigned int enc_priv_key_len;

const struct bootutil_key bootutil_enc_key = {
    .key = enc_priv_key,
    .len = &enc_priv_key_len,
};
#elif defined(MCUBOOT_ENCRYPT_KW)
#error "Encrypted images with AES-KW is not implemented yet."
#endif

#endif
