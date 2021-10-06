/**
 *
 * Built with ARM Mbed-OS
 *
 * Copyright (c) 2019-2021 Embedded Planet, Inc.
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
 * limitations under the License.
 */


#ifndef DATASHARE_H_
#define DATASHARE_H_

/**
 * This is an utility intended to be used by the booted application rather
 * than by the bootloader
 */
#if MCUBOOT_DATA_SHARING && !MCUBOOT_BOOTLOADER_BUILD

#include <stdint.h>
#include "platform/Span.h"

/* Error codes */
#define DATA_SHARE_OK               0
#define DATA_SHARE_ERROR_EOF        1   /* No more TLV entries available */
#define DATA_SHARE_ERROR_OUT_OF_MEM 2   /* The output buffer was not large enough to hold the contents of the TLV entry */
#define DATA_SHARE_ERROR_CORRUPT    3   /* Data corruption has been detected */

/**
 * Class enabling iterator-style access to the TLV-encoded data shared
 * by an mcuboot-based bootloader.
 */
class DataShare
{
public:

    /**
     * Initializes a DataShare iterator at the given base address
     * @note the configured MCUBOOT_SHARED_DATA_BASE address is used by default
     */
    DataShare(uint8_t * shared_base = ((uint8_t *) MCUBOOT_SHARED_DATA_BASE));

    /**
     * Validates the magic number of the shared data section
     * @return true if magic number is found, false otherwise
     */
    bool is_valid();

    /**
     * Gets the total size of the shared data region
     * @return 0 if shared data region is not valid, otherwise the size of the shared data region
     */
    uint16_t get_total_size();

    /**
     * Attempts to get the next TLV entry in the shared data memory
     * @param[put] type Type code of the data entry
     * @param[out] out Output buffer span
     * @param[out] actual_size Size of entry copied to output buffer in bytes
     * @return err Zero if output buffer contents is valid (successful), non-zero on failure
     */
    int get_next(uint16_t *type, mbed::Span<uint8_t> buf, uint16_t *actual_size);

    /**
     * Resets the iterator-like pointer to the first TLV element in the shared
     * data region
     */
    void rewind();

protected:

    uint8_t *_shared_base;
    bool _is_valid = false;
    uint16_t _total_size = 0;
    uint16_t _current_offset = 0;

};

#endif /* MCUBOOT_DATA_SHARING && !MCUBOOT_BOOTLOADER_BUILD */

#endif /* DATASHARE_H_ */
