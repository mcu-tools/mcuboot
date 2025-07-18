/*
 * Copyright (c) 2025 Infineon Technologies AG
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

#ifndef CYW_PLATFORMS_UTILS_H
#define CYW_PLATFORMS_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include "sysflash/sysflash.h"

#include "bootutil/image.h"
#include "bootutil/enc_key.h"
#include "bootutil/fault_injection_hardening.h"

__NO_RETURN void platform_RunNextApp(fih_uint toc2_addr, uint32_t *key, uint32_t *iv);

#endif /* CYW_PLATFORMS_UTILS_H */

