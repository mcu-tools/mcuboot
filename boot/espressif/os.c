/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Bignum (RSA and EC256) allocates from this heap. Ed25519 and hash-only
 * builds do not. */
#if defined(CONFIG_ESP_USE_MBEDTLS) && \
    (defined(CONFIG_ESP_SIGN_RSA) || defined(CONFIG_ESP_SIGN_EC256))

#include <mbedtls/platform.h>
#include <mbedtls/memory_buffer_alloc.h>

#define CRYPTO_HEAP_SIZE 8192

static unsigned char memory_buf[CRYPTO_HEAP_SIZE];

/*
 * Initialize Mbed TLS to be able to use the local heap.
 */
void os_heap_init(void)
{
    mbedtls_memory_buffer_alloc_init(memory_buf, sizeof(memory_buf));
}
#else

void os_heap_init(void)
{
}

#endif
