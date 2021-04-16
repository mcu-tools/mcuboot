/*
 * Mbed-OS Microcontroller Library
 * Copyright (c) 2020 Embedded Planet
 * Copyright (c) 2020 ARM Limited
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

#if MCUBOOT_DIRECT_XIP

#include "flash_map_backend/secondary_bd.h"
#include "platform/mbed_toolchain.h"
#include "FlashIAPBlockDevice.h"

/**
 * For an XIP build, the secondary BD is provided by mcuboot by default.
 *
 * This is a weak symbol so the user can override it.
 */
MBED_WEAK mbed::BlockDevice* get_secondary_bd(void) {
    static FlashIAPBlockDevice secondary_bd(MBED_CONF_MCUBOOT_XIP_SECONDARY_SLOT_ADDRESS,
            MCUBOOT_SLOT_SIZE);

    return &secondary_bd;
}

#endif


