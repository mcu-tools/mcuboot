/*
 * Simulator stub for MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG.
 *
 * Mbed TLS 4.1 (TF-PSA-Crypto 1.1) requires the caller to supply
 * `mbedtls_psa_external_get_random()` when MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
 * is set. Upstream ships a test implementation in
 * `framework/tests/src/fake_external_rng_for_test.c`, but that file
 * drags in a large chunk of the test framework (test_common.h,
 * <test/build_info.h>, etc.) as transitive headers, so we use this
 * self-contained stub instead.
 *
 * ECDSA signature *verification* — the only PSA operation mcuboot's
 * sig-ecdsa-psa path drives — does not consume randomness, but the
 * symbol must still resolve at link time. Fill from libc rand() so
 * that, if the PSA core ever does sample entropy for other code paths
 * in this simulator build, it gets bytes rather than a failure.
 *
 * NOT FOR PRODUCTION USE. libc rand() is not cryptographically secure.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <psa/crypto.h>
#include <stdlib.h>
#include <stdint.h>

psa_status_t mbedtls_psa_external_get_random(
    mbedtls_psa_external_random_context_t *context,
    uint8_t *output, size_t output_size, size_t *output_length)
{
    (void)context;
    for (size_t i = 0; i < output_size; ++i) {
        output[i] = (uint8_t)rand();
    }
    *output_length = output_size;
    return PSA_SUCCESS;
}

/*
 * The simulator's Rust side calls these to flip the test RNG on/off
 * (see sim/mcuboot-sys/src/c.rs). Our implementation is always "on",
 * so the toggles are no-ops but must still link.
 */
void mbedtls_test_enable_insecure_external_rng(void) { }
void mbedtls_test_disable_insecure_external_rng(void) { }
