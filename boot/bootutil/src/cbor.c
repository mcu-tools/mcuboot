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

#include <stdio.h>  // TODO: Remove
#include <stdlib.h>  // TODO: Remove

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_SUIT

#include <string.h>
#include <inttypes.h>

#include <bootutil/bootutil_log.h>

MCUBOOT_LOG_MODULE_REGISTER(mcuboot);

#include "cbor.h"

/* Decode the initial byte, and the associated value.  Returns a
 * non-zero error upon error.  On 0 for success, 'major' will be
 * written with the major value (0-7), and minor will be written with
 * the minor value.  This will consider indefinite values, and 64-bit
 * values to be invalid.  On success the slice will then point to the
 * following item after the tag.  The slice is not modified on error.
 */
static int
get_cbor_tag(struct slice *data,
             uint8_t *major,
             uint32_t *minor)
{
    if (data->len < 1) {
        return -1;
    }

    uint8_t first = *data->base;

    uint8_t maj = first >> 5;
    uint8_t ext = first & 0x1f;

    unsigned want;
    if (ext < 24) {
        *major = maj;
        *minor = ext;
        data->base++;
        data->len--;
        return 0;
    } else if (ext == 24) {
        want = 1;
    } else if (ext == 25) {
        want = 2;
    } else if (ext == 26) {
        want = 4;
    } else {
        /* If we want uint64 support, add it above here. */
        return -1;
    }

    if (data->len + 1 < want) {
        return -1;
    }

    data->base++;
    data->len--;

    uint64_t tmp = 0;
    while (want--) {
        tmp <<= 8;
        tmp |= *data->base++;
        data->len--;
    }

    *major = maj;
    *minor = tmp;
    return 0;
}

int
cbor_template_decode(struct slice template,
                     struct slice data,
                     struct cbor_capture *captures,
                     unsigned capture_size)
{
    /* How many items are left to decode. */
    int todo = 1;
    int rc;
    uint8_t tmajor, dmajor;
    uint32_t tminor, dminor;

    while (todo) {
        rc = get_cbor_tag(&template, &tmajor, &tminor);
        if (rc) {
            return rc;
        }

        rc = get_cbor_tag(&data, &dmajor, &dminor);
        if (rc) {
            return rc;
        }

        BOOT_LOG_ERR("Template: %d,%d; data: %d,%d",
                     tmajor, tminor,
                     dmajor, dminor);

        todo--;

        /* If this is a capture, process that. */
        if (tmajor == CBOR_MAJOR_OTHER && tminor >= 32) {
            unsigned cap = tminor - 32;
            if (cap >= capture_size) {
                BOOT_LOG_ERR("Template error: out of bounds capture");
                return -1;
            }
            captures[cap].major = dmajor;
            captures[cap].minor = dminor;
            switch (dmajor) {
            case CBOR_MAJOR_UNSIGNED:
            case CBOR_MAJOR_NEGATIVE:
                /* No additional data for these. */
                captures[cap].data.base = NULL;
                captures[cap].data.len = 0;
                break;

            case CBOR_MAJOR_BSTR:
            case CBOR_MAJOR_TEXT:
                /* Capture and skip this data. */
                if (dminor > data.len) {
                    BOOT_LOG_ERR("Insufficient space in data");
                    return -1;
                }

                captures[cap].data.base = data.base;
                captures[cap].data.len = dminor;
                data.base += dminor;
                data.len -= dminor;
                break;

            case CBOR_MAJOR_ARRAY:
            case CBOR_MAJOR_MAP:
            case CBOR_MAJOR_TAG:
            case CBOR_MAJOR_OTHER:
                /* These are not supported by captures. */
                BOOT_LOG_ERR("Type unsupported by capture");
                return -1;
                break;

            }

            continue;
        }

        /* Otherwise, this only matches if they are exactly the same. */
        if (tmajor != dmajor || tminor != dminor) {
            BOOT_LOG_ERR("Template mismatch in cbor");
            return -1;
        }

        /* For some CBOR types, the minor value is a length of some
         * type.  They are either an indicator of additional cbor
         * items to process, or bytes of data, depending on the tag.
         */
        switch (tmajor) {
        case CBOR_MAJOR_UNSIGNED:
        case CBOR_MAJOR_NEGATIVE:
            /* No additional data for these. */
            break;

        case CBOR_MAJOR_BSTR:
        case CBOR_MAJOR_TEXT:
            /* These have additional data, that must match. */
            if (tminor > template.len || tminor > data.len) {
                BOOT_LOG_ERR("Short data in cbor");
                return -1;
            }
            if (memcmp(template.base, data.base, tminor) != 0) {
                BOOT_LOG_ERR("Data mismatch on cbor");
#if 0
                for (unsigned i = 0; i < tminor; i++) {
                    printf(" %02x", template.base[i]);
                }
                printf("\ndata: ");
                for (unsigned i = 0; i < tminor; i++) {
                    printf(" %02x", data.base[i]);
                }
                printf("\n");
#endif
                return -1;
            }

            template.base += tminor;
            template.len -= tminor;
            data.base += tminor;
            data.len -= tminor;
            break;

        case CBOR_MAJOR_ARRAY:
            /* Indicates additional items. */
            todo += tminor;
            break;

        case CBOR_MAJOR_MAP:
            /* 2x additional items. */
            todo += tminor + tminor;
            break;

        case CBOR_MAJOR_TAG:
            /* One thing follows a tag. */
            todo++;
            break;

        case CBOR_MAJOR_OTHER:
            /* Sub types have some data, but we don't support those
             * yet. */
            break;
        }
    }

    if (template.len != 0) {
        BOOT_LOG_ERR("Template error, extra data");
        return -1;
    }
    if (data.len != 0) {
        BOOT_LOG_ERR("Extraneous data after template");
        return -1;
    }

    return 0;
}

#endif
