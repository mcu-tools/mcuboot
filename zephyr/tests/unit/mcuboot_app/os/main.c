/*
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <zephyr/ztest.h>
#include <zephyr/fff.h>


#ifdef CONFIG_BOOT_USE_MBEDTLS
#error GAS
#include "os.c"

ZTEST(mcuboot_app, test_crypto_heap_mempool_size)
{
    zassert_equal(ARRAY_SIZE(mempool), CRYPTO_HEAP_SIZE, NULL);
    zassert_equal(sizeof(mempool[0]), sizeof(uint8_t), NULL);

#if (CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN == 2048) && !defined(CONFIG_BOOT_ENCRYPT_RSA)
    zassert_equal(CRYPTO_HEAP_SIZE, 6144, NULL);
#elif !defined(MBEDTLS_RSA_NO_CRT)
    zassert_equal(CRYPTO_HEAP_SIZE, 12032, NULL);
#else
    zassert_equal(CRYPTO_HEAP_SIZE, 16384, NULL);
#endif
}

ZTEST(mcuboot_app, test_os_heap_init)
{
    ztest_test_skip();
}

#endif

ZTEST_SUITE(mcuboot_app, NULL, NULL, NULL, NULL, NULL);
