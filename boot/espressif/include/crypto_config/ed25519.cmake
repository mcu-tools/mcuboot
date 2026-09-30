# Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

# Ed25519 verify is Fiat (Mbed TLS 3.6 has no Ed25519 API). Hashes come from
# Mbed TLS unless the deprecated CONFIG_ESP_USE_TINYCRYPT override is set
# on its own.
# https://github.com/mcu-tools/mcuboot/issues/2256
if(DEFINED CONFIG_ESP_USE_TINYCRYPT AND NOT DEFINED CONFIG_ESP_USE_MBEDTLS)
    message(WARNING
        "Tinycrypt is unmaintained "
        "(https://github.com/mcu-tools/mcuboot/issues/2256). "
        "Mbed TLS is the supported crypto backend for the Espressif port. "
        "CONFIG_ESP_USE_TINYCRYPT keeps the deprecated Tinycrypt sources for Ed25519. "
        "Ed25519 signature verification stays in Fiat; Mbed TLS 3.6 has no Ed25519 API.")

    set(MBEDTLS_ASN1_DIR "${MCUBOOT_ROOT_DIR}/ext/mbedtls-asn1")
    set(CRYPTO_INC
        ${MBEDTLS_ASN1_DIR}/include
        )
    set(crypto_srcs
        # Additionally pull in just the ASN.1 parser from Mbed TLS.
        ${MBEDTLS_ASN1_DIR}/src/asn1parse.c
        ${MBEDTLS_ASN1_DIR}/src/platform_util.c
        )

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
else()
    set(MBEDTLS_DIR ${MCUBOOT_ROOT_DIR}/ext/mbedtls-3.6.0)
    set(CRYPTO_INC
        ${MBEDTLS_DIR}/include
        )
    set(crypto_srcs
        ${ESPRESSIF_PORT_DIR}/keys.c
        ${MBEDTLS_DIR}/library/platform_util.c
        ${MBEDTLS_DIR}/library/sha256.c
        ${MBEDTLS_DIR}/library/sha512.c
        ${MBEDTLS_DIR}/library/asn1parse.c
        ${MCUBOOT_ROOT_DIR}/ext/fiat/src/curve25519.c
        )
    if(DEFINED MBEDTLS_CONFIG_FILE)
        add_definitions(-DMBEDTLS_CONFIG_FILE=\"${MBEDTLS_CONFIG_FILE}\")
    else()
        add_definitions(-DMBEDTLS_CONFIG_FILE=\"${ESPRESSIF_PORT_DIR}/include/crypto_config/mbedtls_config.h\")
    endif()
endif()
