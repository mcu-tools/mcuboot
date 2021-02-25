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

#include "mcuboot_config/mcuboot_config.h"

#ifdef MCUBOOT_X509

#include <stddef.h>
#include <inttypes.h>
#include <string.h>

#include <mbedtls/x509_crt.h>

#include <flash_map_backend/flash_map_backend.h>
#include "bootutil/bootutil_log.h"

#include "bootutil/image.h"
#include "bootutil/crypto/sha256.h"
#include "bootutil/root_cert.h"

#include "image_util.h"

MCUBOOT_LOG_MODULE_DECLARE(mcuboot);

/*
 * Current support is for EC256 signatures using SHA256 hashes.  We
 * allow some amount of data to hold the certificates.
 */
#define SIG_BUF_SIZE 512

int verify_callback(void *buf, mbedtls_x509_crt *crt, int depth,
                    uint32_t *flags)
{
    (void) buf;
    (void) crt;
    (void) depth;
    (void) flags;

    // BOOT_LOG_ERR("callback");

    return 0;
}

/*
 * Verify the integrity of the image.
 * Return non-zero if image coule not be validated/does not validate.
 */
int
bootutil_img_validate(struct enc_key_data *enc_state, int image_index,
                      struct image_header *hdr, const struct flash_area *fap,
                      uint8_t *tmp_buf, uint32_t tmp_buf_sz, uint8_t *seed,
                      int seed_len, uint8_t *out_hash)
{
    uint32_t off;
    uint16_t len;
    uint16_t type;
    uint8_t hash[32];
    bool sha256_valid = false;
    bool cert_valid = false;
    int rc;
    struct image_tlv_iter it;
    uint8_t buf[SIG_BUF_SIZE];
    mbedtls_x509_crt chain;
    mbedtls_x509_crt trust_ca;
    uint32_t flags;

    mbedtls_x509_crt_init(&chain);

    rc = bootutil_img_hash(enc_state, image_index, hdr, fap, tmp_buf,
                           tmp_buf_sz, hash, seed, seed_len);
    if (rc) {
        return rc;
    }

    if (out_hash) {
        memcpy(out_hash, hash, 32);
    }

    rc = bootutil_tlv_iter_begin(&it, hdr, fap, IMAGE_TLV_ANY, false);
    if (rc) {
        return rc;
    }

    while (true) {
        rc = bootutil_tlv_iter_next(&it, &off, &len, &type);
        if (rc < 0) {
            return -1;
        } else if (rc > 0) {
            break;
        }

        if (type == IMAGE_TLV_SHA256) {
            /*
             * Verify the SHA256 image hash.  This must always be
             * present.
             */
            if (len != sizeof(hash)) {
                return -1;
            }
            rc = flash_area_read(fap, off, buf, sizeof hash);
            if (rc) {
                return rc;
            }
            if (memcmp(hash, buf, sizeof(hash))) {
                BOOT_LOG_ERR("Corrupt hash");
                return -1;
            }

            sha256_valid = true;
        } else if (type == IMAGE_TLV_X509) {
            if (len > sizeof buf) {
                return -1;
            }
            rc = flash_area_read(fap, off, buf, len);
            if (rc) {
                return rc;
            }
            // BOOT_LOG_ERR("Cert: %d bytes", len);
            rc = mbedtls_x509_crt_parse_der(&chain, buf, len);
            if (rc) {
                BOOT_LOG_ERR("Parse error %d", rc);
                /* TODO After init, should cleanup */
                return -1;
            }
        } else if (type == IMAGE_TLV_ECDSA256) {
            /* Finish with the root certificate. */
            mbedtls_x509_crt_init(&trust_ca);
            rc = mbedtls_x509_crt_parse_der(&trust_ca, bootutil_root_cert,
                                            bootutil_root_cert_len);
            if (rc) {
                BOOT_LOG_ERR("Parser error: %d", rc);
                /* TODO: After init, should cleanup */
                return -1;
            }

            // BOOT_LOG_ERR("Verify chain");
            flags = 0;
            rc = mbedtls_x509_crt_verify(&chain, &trust_ca, NULL,
                                         NULL,
                                         // "MCUboot sample signing key",
                                         &flags,
                                         verify_callback, NULL);
            // BOOT_LOG_ERR("Verify chain: %d", rc);
            if (rc == 0) {
                    cert_valid = true;
            }
        } else {
            BOOT_LOG_ERR("TLV: %d", type);
        }
    }

    if (!sha256_valid) {
        return -1;
    }
    if (!cert_valid) {
        return -1;
    }

    return 0;
}
#endif /* MCUBOOT_X509 */
