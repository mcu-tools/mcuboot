/*
 * Copyright (c) 2026 Intercreate, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <string.h>

#include "cobs.h"

enum {
    test_payload_max = 600,
};

static uint8_t payload[test_payload_max];
static uint8_t encoded[COBS_ENCODED_MAX(test_payload_max)];
static uint8_t decoded[test_payload_max];

/* Deterministic xorshift32 so any failure reproduces from the fixed seed. */
static uint32_t rng_state = 0x1234567u;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* Encode then decode payload[0..len); assert the encoded stream is zero-free
 * and within the worst-case bound, and that decode inverts encode both
 * out-of-place and in place (dst aliasing src). */
static void check_roundtrip(size_t len)
{
    size_t enc_len = cobs_encode(len, payload, encoded);

    zassert_true(enc_len <= COBS_ENCODED_MAX(len),
                 "len %zu: encoded %zu exceeds bound", len, enc_len);
    for (size_t i = 0; i < enc_len; i++) {
        zassert_not_equal(encoded[i], 0,
                          "len %zu: 0x00 in encoded stream at %zu", len, i);
    }

    zassert_equal(cobs_decode(enc_len, encoded, decoded), len,
                  "len %zu: decoded length mismatch", len);
    zassert_mem_equal(decoded, payload, len, "len %zu: payload mismatch", len);

    zassert_equal(cobs_decode(enc_len, encoded, encoded), len,
                  "len %zu: in-place length mismatch", len);
    zassert_mem_equal(encoded, payload, len, "len %zu: in-place mismatch", len);
}

ZTEST_SUITE(cobs, NULL, NULL, NULL, NULL, NULL);

ZTEST(cobs, test_roundtrip_fuzz)
{
    for (int iter = 0; iter < 20000; iter++) {
        size_t len = rng() % (test_payload_max + 1);
        unsigned int mode = rng() % 4;

        for (size_t i = 0; i < len; i++) {
            switch (mode) {
            case 0: payload[i] = rng(); break;                   /* random */
            case 1: payload[i] = 0; break;                       /* all zero */
            case 2: payload[i] = 0xFF; break;                    /* >254 non-zero runs */
            case 3: payload[i] = (rng() % 4) ? rng() : 0; break; /* sparse zeros */
            }
        }
        check_roundtrip(len);
    }
}

ZTEST(cobs, test_empty)
{
    check_roundtrip(0);
}

ZTEST(cobs, test_run_boundaries)
{
    /* The classic COBS off-by-one lives at a run of exactly 254 non-zero bytes. */
    memset(payload, 0xAB, sizeof(payload));
    check_roundtrip(253);
    check_roundtrip(254);
    check_roundtrip(255);
    check_roundtrip(508);   /* two maximal runs back to back */
}

ZTEST(cobs, test_overhead_within_bound)
{
    /* All-non-zero is the worst case: no zeros for COBS to absorb. */
    memset(payload, 0xAB, sizeof(payload));
    for (size_t len = 0; len <= test_payload_max; len++) {
        size_t enc_len = cobs_encode(len, payload, encoded);
        zassert_true(enc_len <= COBS_ENCODED_MAX(len),
                     "len %zu encoded to %zu, exceeds bound", len, enc_len);
    }
}

ZTEST(cobs, test_malformed_returns_sizemax)
{
    uint8_t out[8];

    /* A 0x00 code byte cannot occur in a valid COBS frame. */
    uint8_t zero_code[] = { 0x00 };
    zassert_equal(cobs_decode(sizeof(zero_code), zero_code, out), SIZE_MAX,
                  "zero code byte not rejected");

    /* Truncated group: the code promises 4 more bytes, only 1 is present. */
    uint8_t truncated[] = { 0x05, 0x11 };
    zassert_equal(cobs_decode(sizeof(truncated), truncated, out), SIZE_MAX,
                  "truncated frame not rejected");
}
