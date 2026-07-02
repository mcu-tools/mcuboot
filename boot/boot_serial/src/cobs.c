/*
 * Copyright (c) 2026 Intercreate, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cobs.h"

enum {
    /* Code byte for a maximal group: cobs_run_max literals, no implicit zero. */
    cobs_code_full = cobs_run_max + 1,
};

size_t cobs_encode(
    size_t src_size,
    uint8_t const src[static restrict src_size],
    uint8_t dst[static restrict COBS_ENCODED_MAX(src_size)]
) {
    size_t dst_idx = 1;
    size_t code_idx = 0;
    uint8_t code = 1;

    for (size_t src_idx = 0; src_idx < src_size; src_idx += 1) {
        if (src[src_idx] != 0) {
            dst[dst_idx] = src[src_idx];
            dst_idx += 1;
            code += 1;
            if (code != cobs_code_full) {
                continue;
            }
        }
        dst[code_idx] = code;
        code_idx = dst_idx;
        dst_idx += 1;
        code = 1;
    }
    dst[code_idx] = code;

    return dst_idx;
}

size_t cobs_decode(
    size_t src_size,
    uint8_t const src[static src_size],
    uint8_t dst[static src_size]
) {
    size_t dst_idx = 0;
    size_t src_idx = 0;

    while (src_idx < src_size) {
        uint8_t const code = src[src_idx];
        src_idx += 1;

        if (code == 0) {
            return SIZE_MAX;
        }

        for (uint8_t i = 1; i < code; i += 1) {
            if (src_idx >= src_size) {
                return SIZE_MAX;
            }
            dst[dst_idx] = src[src_idx];
            dst_idx += 1;
            src_idx += 1;
        }

        if (code != cobs_code_full && src_idx < src_size) {
            dst[dst_idx] = 0;
            dst_idx += 1;
        }
    }

    return dst_idx;
}
