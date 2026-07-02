/*
 * Copyright (c) 2026 Intercreate, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_COBS_
#define H_COBS_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* COBS appends one code byte per run of this many non-zero input bytes. */
    cobs_run_max = 254,
};

/*
 * Worst-case COBS-encoded length for "len" decoded bytes, excluding any frame
 * delimiter. A macro rather than a function so it can size a buffer at compile
 * time; "len" is evaluated more than once.
 */
#define COBS_ENCODED_MAX(len) ((len) + (len) / cobs_run_max + 1)

/*
 * Encode "src_size" bytes of "src" into "dst", replacing every 0x00 so the
 * encoded output may be terminated with a lone 0x00 delimiter. The array-bound
 * syntax states the contract: "dst" must hold COBS_ENCODED_MAX(src_size) bytes,
 * and "restrict" means "dst" and "src" must not alias. Returns the number of
 * bytes written to "dst".
 */
size_t cobs_encode(
    size_t src_size,
    uint8_t const src[static restrict src_size],
    uint8_t dst[static restrict COBS_ENCODED_MAX(src_size)]
);

/*
 * Decode "src_size" COBS bytes of "src" (with the trailing delimiter already
 * removed) into "dst", which may alias "src" for in-place decoding (hence no
 * "restrict"); the decoded length never exceeds "src_size". Returns the decoded
 * length, or SIZE_MAX if "src" is not a well-formed COBS frame.
 */
size_t cobs_decode(
    size_t src_size,
    uint8_t const src[static src_size],
    uint8_t dst[static src_size]
);

#ifdef __cplusplus
}
#endif

#endif /* H_COBS_ */
