/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Linaro LTD
 */

/*
 * Simulator-only memo of verified signatures.
 *
 * The power-fail tests boot the same few images many thousands of
 * times, and each boot repeats the same public-key signature checks.
 * When MCUBOOT_SIM_SIG_CACHE is defined, bootutil_img_validate() asks
 * this cache before calling bootutil_verify_sig(), and records each
 * signature that verifies.  The full check still runs the first time
 * a given (key, hash, signature) triple is seen, so every distinct
 * input, including every invalid one, goes through the real code.
 *
 * The cache lives in the Rust side of the simulator (sig_cache.rs).
 */

#ifndef H_SIM_SIG_CACHE_
#define H_SIM_SIG_CACHE_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Return true if this exact (key, hash, signature) triple has already
 * verified successfully.
 */
bool sim_sig_cache_lookup(const uint8_t *key, size_t key_len,
                          const uint8_t *hash, size_t hash_len,
                          const uint8_t *sig, size_t sig_len);

/*
 * Record a (key, hash, signature) triple that has just verified
 * successfully.  Must only be called after a real verification.
 */
void sim_sig_cache_insert(const uint8_t *key, size_t key_len,
                          const uint8_t *hash, size_t hash_len,
                          const uint8_t *sig, size_t sig_len);

#endif /* H_SIM_SIG_CACHE_ */
