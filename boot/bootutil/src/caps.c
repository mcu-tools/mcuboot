/*
 * Copyright (c) 2017 Linaro Limited
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
 * limitations under the License.
 */

#include <bootutil/caps.h>
#include "mcuboot_config/mcuboot_config.h"

uint32_t bootutil_get_caps(void)
{
        uint32_t res = 0;

#if defined(MCUBOOT_SIGN_RSA)
        res |= BOOTUTIL_CAP_RSA2048;
#endif
#if defined(MCUBOOT_SIGN_EC)
        res |= BOOTUTIL_CAP_ECDSA_P224;
#endif
#if defined(MCUBOOT_SIGN_EC256)
        res |= BOOTUTIL_CAP_ECDSA_P256;
#endif
#if defined(MCUBOOT_OVERWRITE_ONLY)
        res |= BOOTUTIL_CAP_OVERWRITE_UPGRADE;
#else
        res |= BOOTUTIL_CAP_SWAP_UPGRADE;
#endif
#if defined(MCUBOOT_ENCRYPT_RSA)
	res |= BOOTUTIL_CAP_ENC_RSA;
#endif
#if defined(MCUBOOT_ENCRYPT_KW)
	res |= BOOTUTIL_CAP_ENC_KW;
#endif
#if defined(MCUBOOT_VALIDATE_SLOT0)
	res |= BOOTUTIL_CAP_VALIDATE_SLOT0;
#endif

        return res;
}
