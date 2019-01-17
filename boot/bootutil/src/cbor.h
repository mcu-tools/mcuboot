/*
 * Copyright (c) 2019 Linaro Limited
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

#ifndef BOOTUTIL_CBOR__
#define BOOTUTIL_CBOR__

#include <inttypes.h>

/* Macros to help construct CBOR data.  These all expand to comma
 * terminate lists of byte values. */

/* A simple CBOR item, with a length less than 24. */
#define CBOR_ITEM_SIMPLE(major, minor) (((major)<<5)|(minor))
#define CBOR_ITEM_1(major, minor)      (((major)<<5)|24),(minor)
#define CBOR_ITEM_2(major, minor)      (((major)<<5)|25), \
        ((minor)>>8), ((minor) & 0xFF)
#define CBOR_ITEM_4(major, minor)      (((major)<<5)|26), \
        ((minor)>>24), (((minor)>>16)&0xFF), (((minor)>>8)&0xFF), \
        ((minor) & 0xFF)

#define CBOR_MAJOR_UNSIGNED 0
#define CBOR_MAJOR_NEGATIVE 1
#define CBOR_MAJOR_BSTR     2
#define CBOR_MAJOR_TEXT     3
#define CBOR_MAJOR_ARRAY    4
#define CBOR_MAJOR_MAP      5
#define CBOR_MAJOR_TAG      6
#define CBOR_MAJOR_OTHER    7

#define CBOR_OTHER_FALSE    20
#define CBOR_OTHER_TRUE     21
#define CBOR_OTHER_NULL     22
#define CBOR_OTHER_UNDEFINED 23
#define CBOR_OTHER_CAPTURE(x)  ((x)+32)

/* To assist in always performing bounds checking on data, this
 * 'slice' type contains a base and a length.  This is a fat pointer
 * type. */
struct slice {
    const uint8_t *base;
    uint32_t len;
};

/* A single piece of captured data.  This will record the major/minor
 * value, and when appropriate, also fill in a slice representing the
 * associated block of data. */
struct cbor_capture {
    uint8_t  major;
    uint32_t minor;
    struct slice data;
};

/* Walk through a given piece of CBOR data, using a template.  Fields
 * marked with CBOR_OTHER_CAPTURE(x) will be stored in the numbered
 * element of the capture array.  Returns 0 on success, or a non-zero
 * value indicating a failure. */
int
cbor_template_decode(struct slice template,
                     struct slice data,
                     struct cbor_capture *captures,
                     unsigned capture_size);

#endif /* not BOOTUTIL_CBOR__ */
