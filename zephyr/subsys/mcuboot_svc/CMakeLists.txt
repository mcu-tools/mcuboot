# CMakeLists.txt for building mcuboot as a Zephyr project
#
# Copyright (c) 2017 Open Source Foundries Limited
# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

# Path to "boot" subdirectory of repository root.
set(BOOT_DIR ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot)
# Path to top-level repository root directory.
set(MCUBOOT_DIR ${ZEPHYR_MCUBOOT_MODULE_DIR})
# Path to tinycrypt library source subdirectory of MCUBOOT_DIR.
set(TINYCRYPT_DIR "${ZEPHYR_MCUBOOT_MODULE_DIR}/ext/tinycrypt/lib")
assert_exists(TINYCRYPT_DIR)
set(TINYCRYPT_SHA512_DIR "${ZEPHYR_MCUBOOT_MODULE_DIR}/ext/tinycrypt-sha512/lib")
assert_exists(TINYCRYPT_SHA512_DIR)
# Path to crypto-fiat
set(FIAT_DIR "${ZEPHYR_MCUBOOT_MODULE_DIR}/ext/fiat")
assert_exists(FIAT_DIR)
# Path to mbed-tls' asn1 parser library.
set(MBEDTLS_ASN1_DIR "${ZEPHYR_MCUBOOT_MODULE_DIR}/ext/mbedtls-asn1")
assert_exists(MBEDTLS_ASN1_DIR)
set(NRF_DIR "${ZEPHYR_MCUBOOT_MODULE_DIR}/ext/nrf")

if(CONFIG_BOOT_USE_NRF_CC310_BL)
set(NRFXLIB_DIR ${ZEPHYR_NRFXLIB_MODULE_DIR})
if(NOT EXISTS ${NRFXLIB_DIR})
  message(FATAL_ERROR "
    ------------------------------------------------------------------------
    No such file or directory: ${NRFXLIB_DIR}
    The current configuration enables nRF CC310 crypto accelerator hardware
    with the `CONFIG_BOOT_USE_NRF_CC310_BL` option. Please follow
    `ext/nrf/README.md` guide to fix your setup or use tinycrypt instead of
    the HW accelerator.
    To use the tinycrypt set `CONFIG_BOOT_ECDSA_TINYCRYPT` to y.
    ------------------------------------------------------------------------")
endif()
# Don't include this if we are using west
 add_subdirectory(${NRFXLIB_DIR} ${PROJECT_BINARY_DIR}/nrfxlib)
endif()

zephyr_library_include_directories(
  ${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include
  targets
  )
if(EXISTS targets/${BOARD}.h)
  zephyr_library_compile_definitions(MCUBOOT_TARGET_CONFIG="${BOARD}.h")
endif()

# Zephyr port-specific sources.
zephyr_library_sources(
  main.c
  flash_map_extended.c
  os.c
  )

if(DEFINED CONFIG_ENABLE_MGMT_PERUSER)
  zephyr_library_sources(
    boot_serial_extensions.c
    )
endif()

if(NOT DEFINED CONFIG_FLASH_PAGE_LAYOUT)
  zephyr_library_sources(
    flash_map_legacy.c
    )
endif()

# Generic bootutil sources and includes.
zephyr_library_include_directories(${BOOT_DIR}/bootutil/include)
zephyr_library_sources(
  ${BOOT_DIR}/bootutil/src/image_validate.c
  ${BOOT_DIR}/bootutil/src/tlv.c
  ${BOOT_DIR}/bootutil/src/encrypted.c
  ${BOOT_DIR}/bootutil/src/image_rsa.c
  ${BOOT_DIR}/bootutil/src/image_ec256.c
  ${BOOT_DIR}/bootutil/src/image_ed25519.c
  ${BOOT_DIR}/bootutil/src/bootutil_misc.c
  ${BOOT_DIR}/bootutil/src/fault_injection_hardening.c
  )

if(CONFIG_BOOT_FIH_PROFILE_HIGH)
zephyr_library_sources(
  ${BOOT_DIR}/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c
  )
endif()

if(CONFIG_SINGLE_APPLICATION_SLOT)
zephyr_library_sources(
  single_loader.c
  )
zephyr_library_include_directories(${BOOT_DIR}/bootutil/src)
else()
zephyr_library_sources(
  ${BOOT_DIR}/bootutil/src/loader.c
  ${BOOT_DIR}/bootutil/src/swap_misc.c
  ${BOOT_DIR}/bootutil/src/swap_scratch.c
  ${BOOT_DIR}/bootutil/src/swap_move.c
  ${BOOT_DIR}/bootutil/src/caps.c
  )
endif()

if(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256 OR CONFIG_BOOT_ENCRYPT_EC256 OR CONFIG_BOOT_SERIAL_ENCRYPT_EC256)
  zephyr_library_include_directories(
    ${MBEDTLS_ASN1_DIR}/include
    )
  zephyr_library_sources(
    # Additionally pull in just the ASN.1 parser from mbedTLS.
    ${MBEDTLS_ASN1_DIR}/src/asn1parse.c
    ${MBEDTLS_ASN1_DIR}/src/platform_util.c
    )
  if(CONFIG_BOOT_USE_TINYCRYPT)
  # When using ECDSA signatures, pull in our copy of the tinycrypt library.
  zephyr_library_include_directories(
    ${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include
    ${TINYCRYPT_DIR}/include
    )
  zephyr_include_directories(${TINYCRYPT_DIR}/include)

  zephyr_library_sources(
    ${TINYCRYPT_DIR}/source/ecc.c
    ${TINYCRYPT_DIR}/source/ecc_dsa.c
    ${TINYCRYPT_DIR}/source/sha256.c
    ${TINYCRYPT_DIR}/source/utils.c
    )
  elseif(CONFIG_BOOT_USE_NRF_CC310_BL)
    zephyr_library_sources(${NRF_DIR}/cc310_glue.c)
    zephyr_library_include_directories(${NRF_DIR})
    zephyr_link_libraries(nrfxlib_crypto)
  endif()

  # Since here we are not using Zephyr's mbedTLS but rather our own, we need
  # to set MBEDTLS_CONFIG_FILE ourselves. When using Zephyr's copy, this
  # variable is set by its Kconfig in the Zephyr codebase.
  zephyr_library_compile_definitions(
    MBEDTLS_CONFIG_FILE="${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include/mcuboot-mbedtls-cfg.h"
    )
elseif(CONFIG_BOOT_SIGNATURE_TYPE_NONE)
  zephyr_library_include_directories(
    ${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include
    ${TINYCRYPT_DIR}/include
    )

  zephyr_library_sources(
    ${TINYCRYPT_DIR}/source/sha256.c
    ${TINYCRYPT_DIR}/source/utils.c
    )
elseif(CONFIG_BOOT_SIGNATURE_TYPE_RSA)
  # Use mbedTLS provided by Zephyr for RSA signatures. (Its config file
  # is set using Kconfig.)
  zephyr_include_directories(${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include)
  if(CONFIG_BOOT_ENCRYPT_RSA)
    set_source_files_properties(
      ${BOOT_DIR}/bootutil/src/encrypted.c
      PROPERTIES
      INCLUDE_DIRECTORIES ${ZEPHYR_MBEDTLS_MODULE_DIR}/library
      )
  endif()
elseif(CONFIG_BOOT_SIGNATURE_TYPE_ED25519 OR CONFIG_BOOT_ENCRYPT_X25519)
  if(CONFIG_BOOT_USE_TINYCRYPT)
    zephyr_library_include_directories(
      ${MBEDTLS_ASN1_DIR}/include
      ${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include
      ${TINYCRYPT_DIR}/include
      ${TINYCRYPT_SHA512_DIR}/include
      )
    zephyr_library_sources(
      ${TINYCRYPT_DIR}/source/sha256.c
      ${TINYCRYPT_DIR}/source/utils.c
      ${TINYCRYPT_SHA512_DIR}/source/sha512.c
      # Additionally pull in just the ASN.1 parser from mbedTLS.
      ${MBEDTLS_ASN1_DIR}/src/asn1parse.c
      ${MBEDTLS_ASN1_DIR}/src/platform_util.c
      )
    zephyr_library_compile_definitions(
      MBEDTLS_CONFIG_FILE="${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include/mcuboot-mbedtls-cfg.h"
      )
  else()
    zephyr_include_directories(${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include)
  endif()

  zephyr_library_include_directories(
    ${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include
    ${FIAT_DIR}/include/
  )

  zephyr_library_sources(
    ${FIAT_DIR}/src/curve25519.c
  )
endif()

if(CONFIG_BOOT_ENCRYPT_EC256 OR CONFIG_BOOT_ENCRYPT_X25519 OR CONFIG_BOOT_SERIAL_ENCRYPT_EC256)
  zephyr_library_sources(
    ${TINYCRYPT_DIR}/source/aes_encrypt.c
    ${TINYCRYPT_DIR}/source/aes_decrypt.c
    ${TINYCRYPT_DIR}/source/ctr_mode.c
    ${TINYCRYPT_DIR}/source/hmac.c
    ${TINYCRYPT_DIR}/source/ecc_dh.c
    )
endif()

if(CONFIG_BOOT_ENCRYPT_EC256)
  zephyr_library_sources(
    ${TINYCRYPT_DIR}/source/ecc_dh.c
    )
endif()

if(CONFIG_MCUBOOT_SERIAL)
  zephyr_sources(serial_adapter.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/boot_serial.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/serial_recovery_cbor.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/zcbor_decode.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/zcbor_encode.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/zcbor_common.c)

  zephyr_sources_ifdef(CONFIG_BOOT_MGMT_ECHO ${BOOT_DIR}/boot_serial/src/serial_recovery_echo.c)

  zephyr_include_directories(${BOOT_DIR}/bootutil/include)
  zephyr_include_directories(${BOOT_DIR}/boot_serial/include)
  zephyr_include_directories(${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include)

  zephyr_include_directories_ifdef(
    CONFIG_BOOT_ERASE_PROGRESSIVELY
    ${BOOT_DIR}/bootutil/src
    )
endif()

if(CONFIG_MCUBOOT_CLEANUP_ARM_CORE)
zephyr_library_sources(
  arm_cleanup.c
)
endif()
