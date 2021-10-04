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

#if MCUBOOT_DATA_SHARING && !MCUBOOT_BOOTLOADER_BUILD

#include "DataShare.h"
#include "boot_status.h"

#include <cstring>

#define PTR_TO_UINT8(x) ((volatile uint8_t *) x)
#define PTR_TO_UINT16(x) ((volatile uint16_t *) x)
#define PTR_TO_UINT32(x) ((volatile uint32_t *) x)

#define SHARED_DATA_ENTRY_BASE MCUBOOT_SHARED_DATA_BASE+SHARED_DATA_HEADER_SIZE

DataShare::DataShare(uint8_t *shared_base) : _shared_base(shared_base) {
    volatile uint16_t *ptr = PTR_TO_UINT16(MCUBOOT_SHARED_DATA_BASE);

    /* Validate magic word */
    if(*ptr++ == SHARED_DATA_TLV_INFO_MAGIC) {
        _is_valid = true;
    }

    _total_size = *ptr;

}

bool DataShare::is_valid() {
    return _is_valid;
}

uint16_t DataShare::get_total_size() {
    if(!is_valid()) {
        return 0;
    }

    return _total_size;
}

int DataShare::get_next(uint16_t *type, mbed::Span<uint8_t> buf, uint16_t *actual_size) {

    if(!is_valid()) {
        return DATA_SHARE_ERROR_CORRUPT;
    }

    if(_current_offset >= (_total_size - SHARED_DATA_HEADER_SIZE)) {
        return DATA_SHARE_ERROR_EOF;
    }

    uint16_t working_offset = _current_offset;

    /* Get the type and the length */
    *type = *PTR_TO_UINT16((SHARED_DATA_ENTRY_BASE + working_offset));
    working_offset += sizeof(uint16_t);
    *actual_size = *PTR_TO_UINT16((SHARED_DATA_ENTRY_BASE + working_offset));
    working_offset += sizeof(uint16_t);

    /* Check if the output buffer is large enough */
    if((size_t)buf.size() < *actual_size) {
        return DATA_SHARE_ERROR_OUT_OF_MEM;
    }

    /* Copy data of TLV entry */
    memcpy(buf.data(), (const uint8_t*)PTR_TO_UINT8((SHARED_DATA_ENTRY_BASE + working_offset)), *actual_size);

    working_offset += *actual_size;

    /* Update state */
    _current_offset = working_offset;

    return DATA_SHARE_OK;

}

void DataShare::rewind() {
    _current_offset = 0;
}

#endif /* MCUBOOT_DATA_SHARING && !MCUBOOT_BOOTLOADER_BUILD */
