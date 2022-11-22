# Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

if (DEFINED CONFIG_ESP_USE_MBEDTLS)
    set(MBEDTLS_DIR ${MCUBOOT_ROOT_DIR}/ext/mbedtls)
    set(CRYPTO_INC
        ${MBEDTLS_DIR}/include
        )
    set(crypto_srcs
        ${ESPRESSIF_PORT_DIR}/keys.c
        ${MBEDTLS_DIR}/library/platform.c
        ${MBEDTLS_DIR}/library/platform_util.c
        ${MBEDTLS_DIR}/library/sha256.c
        ${MBEDTLS_DIR}/library/rsa.c
        ${MBEDTLS_DIR}/library/bignum.c
        ${MBEDTLS_DIR}/library/asn1parse.c
        ${MBEDTLS_DIR}/library/md.c
        ${MBEDTLS_DIR}/library/memory_buffer_alloc.c
        )
    if (DEFINED MBEDTLS_CONFIG_FILE)
        add_definitions(-DMBEDTLS_CONFIG_FILE=\"${MBEDTLS_CONFIG_FILE}\")
    else()
        add_definitions(-DMBEDTLS_CONFIG_FILE=\"${ESPRESSIF_PORT_DIR}/include/crypto_config/mbedtls_custom_config.h\")
    endif()
elseif (DEFINED CONFIG_ESP_USE_TINYCRYPT)
    message(FATAL_ERROR "RSA signature verification using Tinycrypt lib is not supported")
endif()