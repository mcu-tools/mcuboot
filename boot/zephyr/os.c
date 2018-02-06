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

#include <zephyr.h>
#include <string.h>

#include "os/os_heap.h"

#if !defined(CONFIG_MBEDTLS) && defined(MCUBOOT_USE_MBED_TLS)

#define MBEDTLS_CONFIG_FILE MCUBOOT_MBEDTLS_CFG_FILE
#include <mbedtls/platform.h>
#include <mbedtls/memory_buffer_alloc.h>

/*
 * This is the heap for mbed TLS.  The value needed depends on the key
 * size and algorithm used.  For RSA-2048, 6144 bytes seems to be
 * enough.
 */
#define CRYPTO_HEAP_SIZE 6144

static unsigned char mempool[CRYPTO_HEAP_SIZE];

/*
 * Initialize mbedtls to be able to use the local heap.
 */
void os_heap_init(void)
{
    mbedtls_memory_buffer_alloc_init(mempool, sizeof(mempool));
}
#else
void os_heap_init(void)
{
}
#endif
