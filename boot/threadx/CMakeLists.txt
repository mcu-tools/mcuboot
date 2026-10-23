#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
#

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(mcuboot_threadx C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MCUBOOT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(BOOTUTIL_DIR ${MCUBOOT_ROOT}/boot/bootutil)
set(TINYCRYPT_DIR ${MCUBOOT_ROOT}/ext/tinycrypt/lib)
set(MBEDTLS_ASN1_DIR ${MCUBOOT_ROOT}/ext/mbedtls-asn1)

# Options
option(MCUBOOT_SIGN_EC256 "Enable ECDSA P-256 signatures" ON)
option(MCUBOOT_USE_TINYCRYPT "Use TinyCrypt crypto backend" ON)
option(MCUBOOT_SWAP_USING_SCRATCH "Use swap with scratch area" ON)
option(MCUBOOT_BUILD_TESTS "Build automated test suite" ON)

# Crypto backend sources (TinyCrypt + ASN.1 parser)
set(CRYPTO_SOURCES
    ${TINYCRYPT_DIR}/source/sha256.c
    ${TINYCRYPT_DIR}/source/ecc.c
    ${TINYCRYPT_DIR}/source/ecc_dsa.c
    ${TINYCRYPT_DIR}/source/utils.c
    ${MBEDTLS_ASN1_DIR}/src/asn1parse.c
    ${MBEDTLS_ASN1_DIR}/src/platform_util.c
)

# MCUboot core bootutil sources
set(BOOTUTIL_SOURCES
    ${BOOTUTIL_DIR}/src/boot_record.c
    ${BOOTUTIL_DIR}/src/bootutil_find_key.c
    ${BOOTUTIL_DIR}/src/bootutil_img_hash.c
    ${BOOTUTIL_DIR}/src/bootutil_img_security_cnt.c
    ${BOOTUTIL_DIR}/src/bootutil_misc.c
    ${BOOTUTIL_DIR}/src/bootutil_area.c
    ${BOOTUTIL_DIR}/src/bootutil_loader.c
    ${BOOTUTIL_DIR}/src/bootutil_public.c
    ${BOOTUTIL_DIR}/src/caps.c
    ${BOOTUTIL_DIR}/src/encrypted.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening.c
    ${BOOTUTIL_DIR}/src/image_ecdsa.c
    ${BOOTUTIL_DIR}/src/image_ed25519.c
    ${BOOTUTIL_DIR}/src/image_rsa.c
    ${BOOTUTIL_DIR}/src/image_validate.c
    ${BOOTUTIL_DIR}/src/loader.c
    ${BOOTUTIL_DIR}/src/swap_misc.c
    ${BOOTUTIL_DIR}/src/swap_move.c
    ${BOOTUTIL_DIR}/src/swap_offset.c
    ${BOOTUTIL_DIR}/src/swap_scratch.c
    ${BOOTUTIL_DIR}/src/ram_load.c
    ${BOOTUTIL_DIR}/src/tlv.c
)

# ThreadX port static library
add_library(bootutil_threadx STATIC
    ${BOOTUTIL_SOURCES}
    ${CRYPTO_SOURCES}
    src/flash_map_backend.c
)

target_include_directories(bootutil_threadx
    PUBLIC
        include
        ${BOOTUTIL_DIR}/include
    PRIVATE
        ${BOOTUTIL_DIR}/src
        ${TINYCRYPT_DIR}/include
        ${MBEDTLS_ASN1_DIR}/include
)

target_compile_definitions(bootutil_threadx
    PUBLIC
        MCUBOOT_SIGN_EC256=1
        MCUBOOT_USE_TINYCRYPT=1
        MCUBOOT_SWAP_USING_SCRATCH=1
        MCUBOOT_MAX_IMG_SECTORS=128
        MCUBOOT_BOOT_MAX_ALIGN=8
        MCUBOOT_IMAGE_NUMBER=1
        MCUBOOT_VALIDATE_PRIMARY_SLOT=1
        MCUBOOT_HAVE_LOGGING=1
        MCUBOOT_HAVE_ASSERT_H=1
        MBEDTLS_USER_CONFIG_FILE="mbedtls_user_config.h"
)

# Standalone Bootloader Executable
add_executable(mcuboot_threadx
    src/main.c
    src/keys.c
)

target_link_libraries(mcuboot_threadx PRIVATE bootutil_threadx)

# Automated Test Suite
if(MCUBOOT_BUILD_TESTS)
    enable_testing()

    add_executable(mcuboot_threadx_test
        tests/test_mcuboot_threadx.c
        src/keys.c
    )

    target_include_directories(mcuboot_threadx_test
        PRIVATE
            tests
    )

    target_link_libraries(mcuboot_threadx_test PRIVATE bootutil_threadx)

    add_test(NAME mcuboot_threadx_verification COMMAND mcuboot_threadx_test)
endif()

add_subdirectory(sample_app)
