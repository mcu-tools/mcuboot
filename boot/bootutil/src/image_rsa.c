/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>

#ifdef APP_mynewt
#include "mynewt/config.h"
#endif

#ifdef MCUBOOT_SIGN_RSA
#include "bootutil/sign_key.h"

#include "mbedtls/rsa.h"
#include "mbedtls/asn1.h"

#include "bootutil_priv.h"

static const uint8_t sha256_oid[] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x04, 0x20
};

/*
 * Parse the public key used for signing. Simple RSA format.
 */
static int
bootutil_parse_rsakey(mbedtls_rsa_context *ctx, uint8_t **p, uint8_t *end)
{
    int rc;
    size_t len;

    if ((rc = mbedtls_asn1_get_tag(p, end, &len,
          MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return -1;
    }

    if (*p + len != end) {
        return -2;
    }

    if ((rc = mbedtls_asn1_get_mpi(p, end, &ctx->N)) != 0 ||
      (rc = mbedtls_asn1_get_mpi(p, end, &ctx->E)) != 0) {
        return -3;
    }

    if (*p != end) {
        return -4;
    }

    if ((rc = mbedtls_rsa_check_pubkey(ctx)) != 0) {
        return -5;
    }

    ctx->len = mbedtls_mpi_size(&ctx->N);

    return 0;
}

/*
 * PKCS1.5 using RSA2048 computed over SHA256.
 */
static int
bootutil_cmp_rsasig(mbedtls_rsa_context *ctx, uint8_t *hash, uint32_t hlen,
  uint8_t *sig)
{
    uint8_t buf[MBEDTLS_MPI_MAX_SIZE];
    uint8_t *p;

    if (ctx->len != 256) {
        return -1;
    }

    if (mbedtls_rsa_public(ctx, sig, buf)) {
        return -1;
    }

    p = buf;

    if (*p++ != 0 || *p++ != MBEDTLS_RSA_SIGN) {
        return -1;
    }

    while (*p != 0) {
        if (p >= buf + ctx->len - 1 || *p != 0xFF) {
            return -1;
        }
        p++;
    }
    p++;

    if ((p - buf) + sizeof(sha256_oid) + hlen != ctx->len) {
        return -1;
    }

    if (memcmp(p, sha256_oid, sizeof(sha256_oid))) {
        return -1;
    }
    p += sizeof(sha256_oid);

    if (memcmp(p, hash, hlen)) {
        return -1;
    }

    return 0;
}

int
bootutil_verify_sig(uint8_t *hash, uint32_t hlen, uint8_t *sig, int slen,
  uint8_t key_id)
{
    mbedtls_rsa_context ctx;
    int rc;
    uint8_t *cp;
    uint8_t *end;

    mbedtls_rsa_init(&ctx, 0, 0);

    cp = (uint8_t *)bootutil_keys[key_id].key;
    end = cp + *bootutil_keys[key_id].len;

    rc = bootutil_parse_rsakey(&ctx, &cp, end);
    if (rc || slen != ctx.len) {
        mbedtls_rsa_free(&ctx);
        return rc;
    }
    rc = bootutil_cmp_rsasig(&ctx, hash, hlen, sig);
    mbedtls_rsa_free(&ctx);

    return rc;
}
#endif /* MCUBOOT_SIGN_RSA */
