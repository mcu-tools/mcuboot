# Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

set(MBEDTLS_ASN1_DIR "${MCUBOOT_ROOT_DIR}/ext/mbedtls-asn1")
set(CRYPTO_INC
    ${MBEDTLS_ASN1_DIR}/include
    )
set(crypto_srcs
    # Additionally pull in just the ASN.1 parser from Mbed TLS.
    ${MBEDTLS_ASN1_DIR}/src/asn1parse.c
    ${MBEDTLS_ASN1_DIR}/src/platform_util.c
    )

if (DEFINED CONFIG_ESP_USE_MBEDTLS)
    message(FATAL_ERROR "ED25519 image signing using Mbed TLS lib is not supported")
elseif (DEFINED CONFIG_ESP_USE_TINYCRYPT)
    set(TINYCRYPT_DIR ${MCUBOOT_ROOT_DIR}/ext/tinycrypt/lib)
    set(TINYCRYPT512_DIR ${MCUBOOT_ROOT_DIR}/ext/tinycrypt-sha512/lib)
    list(APPEND CRYPTO_INC
        ${TINYCRYPT_DIR}/include
        ${TINYCRYPT512_DIR}/include
        )
    list(APPEND crypto_srcs
        ${ESPRESSIF_PORT_DIR}/keys.c
        ${TINYCRYPT_DIR}/source/utils.c
        ${TINYCRYPT_DIR}/source/sha256.c
        ${TINYCRYPT512_DIR}/source/sha512.c
        ${MCUBOOT_ROOT_DIR}/ext/fiat/src/curve25519.c
        )
endif()