#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
#

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(mcuboot_threadx C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(mcuboot_root ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(bootutil_dir ${mcuboot_root}/boot/bootutil)

# Options
option(MCUBOOT_SIGN_EC256 "Enable ECDSA P-256 signatures" ON)
option(MCUBOOT_SIGN_RSA "Enable RSA signatures" OFF)
option(MCUBOOT_SIGN_ED25519 "Enable ED25519 signatures" OFF)
option(MCUBOOT_USE_MBED_TLS "Use mbedTLS crypto backend" ON)
option(MCUBOOT_USE_TINYCRYPT "Use TinyCrypt crypto backend (deprecated)" OFF)
option(MCUBOOT_SWAP_USING_MOVE "Use swap move upgrade mode" ON)
option(MCUBOOT_OVERWRITE_ONLY "Use overwrite-only upgrade mode" OFF)
option(MCUBOOT_DIRECT_XIP "Use direct-xip upgrade mode" OFF)
option(MCUBOOT_VALIDATE_PRIMARY_SLOT "Always validate primary slot on boot" ON)
option(MCUBOOT_HAVE_LOGGING "Enable logging" ON)

# Verification signing key configuration and automatic generation
if(NOT DEFINED mcuboot_sign_key_file)
  if(MCUBOOT_SIGN_RSA)
    set(default_key "${mcuboot_root}/root-rsa-2048.pem")
  elseif(MCUBOOT_SIGN_ED25519)
    set(default_key "${mcuboot_root}/root-ed25519.pem")
  else()
    set(default_key "${mcuboot_root}/root-ec-p256.pem")
  endif()
  set(mcuboot_sign_key_file "${default_key}" CACHE FILEPATH "Path to signature verification key file")
endif()

if(NOT DEFINED PYTHON_EXECUTABLE)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
  else()
    find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)
  endif()
endif()

set(generated_pubkey "${CMAKE_CURRENT_BINARY_DIR}/autogen-pubkey.c")
add_custom_command(
  OUTPUT "${generated_pubkey}"
  COMMAND ${PYTHON_EXECUTABLE} "${mcuboot_root}/scripts/imgtool.py" getpub -k "${mcuboot_sign_key_file}" -o "${generated_pubkey}"
  DEPENDS "${mcuboot_sign_key_file}"
  COMMENT "Generating public key from ${mcuboot_sign_key_file}"
)

# Crypto backend sources and configuration
if(MCUBOOT_USE_MBED_TLS)
  set(mbedtls_dir ${mcuboot_root}/ext/mbedtls-3.6.0)
  set(crypto_sources
    ${mbedtls_dir}/library/sha256.c
    ${mbedtls_dir}/library/asn1parse.c
    ${mbedtls_dir}/library/asn1write.c
    ${mbedtls_dir}/library/bignum.c
    ${mbedtls_dir}/library/bignum_core.c
    ${mbedtls_dir}/library/constant_time.c
    ${mbedtls_dir}/library/ecdsa.c
    ${mbedtls_dir}/library/ecp.c
    ${mbedtls_dir}/library/ecp_curves.c
    ${mbedtls_dir}/library/platform.c
    ${mbedtls_dir}/library/platform_util.c
    ${mbedtls_dir}/library/md.c
  )
  if(MCUBOOT_SIGN_RSA)
    list(APPEND crypto_sources
      ${mbedtls_dir}/library/rsa.c
      ${mbedtls_dir}/library/rsa_alt_helpers.c
    )
  endif()
  set(crypto_include_dirs
    ${mbedtls_dir}/include
    ${mbedtls_dir}/library
  )
  set(crypto_compile_defs
    MCUBOOT_USE_MBED_TLS=1
    MBEDTLS_CONFIG_FILE="mcuboot_mbedtls_config.h"
  )
elseif(MCUBOOT_USE_TINYCRYPT)
  set(tinycrypt_dir ${mcuboot_root}/ext/tinycrypt/lib)
  set(mbedtls_asn1_dir ${mcuboot_root}/ext/mbedtls-asn1)
  set(crypto_sources
    ${tinycrypt_dir}/source/sha256.c
    ${tinycrypt_dir}/source/ecc.c
    ${tinycrypt_dir}/source/ecc_dsa.c
    ${tinycrypt_dir}/source/utils.c
    ${mbedtls_asn1_dir}/src/asn1parse.c
    ${mbedtls_asn1_dir}/src/platform_util.c
  )
  set(crypto_include_dirs
    ${tinycrypt_dir}/include
    ${mbedtls_asn1_dir}/include
  )
  set(crypto_compile_defs
    MCUBOOT_USE_TINYCRYPT=1
  )
else()
  message(FATAL_ERROR "No crypto backend selected. Enable MCUBOOT_USE_MBED_TLS or MCUBOOT_USE_TINYCRYPT.")
endif()

# MCUboot core bootutil sources
set(bootutil_sources
  ${bootutil_dir}/src/boot_record.c
  ${bootutil_dir}/src/bootutil_find_key.c
  ${bootutil_dir}/src/bootutil_img_hash.c
  ${bootutil_dir}/src/bootutil_img_security_cnt.c
  ${bootutil_dir}/src/bootutil_misc.c
  ${bootutil_dir}/src/bootutil_area.c
  ${bootutil_dir}/src/bootutil_loader.c
  ${bootutil_dir}/src/bootutil_public.c
  ${bootutil_dir}/src/caps.c
  ${bootutil_dir}/src/encrypted.c
  ${bootutil_dir}/src/fault_injection_hardening.c
  ${bootutil_dir}/src/image_ecdsa.c
  ${bootutil_dir}/src/image_ed25519.c
  ${bootutil_dir}/src/image_rsa.c
  ${bootutil_dir}/src/image_validate.c
  ${bootutil_dir}/src/loader.c
  ${bootutil_dir}/src/swap_misc.c
  ${bootutil_dir}/src/swap_move.c
  ${bootutil_dir}/src/swap_offset.c
  ${bootutil_dir}/src/swap_scratch.c
  ${bootutil_dir}/src/ram_load.c
  ${bootutil_dir}/src/tlv.c
)

# ThreadX port static library
add_library(bootutil_threadx STATIC
  ${bootutil_sources}
  ${crypto_sources}
  src/flash_map_backend.c
  src/keys.c
  ${generated_pubkey}
)

target_include_directories(bootutil_threadx
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${bootutil_dir}/include
  PRIVATE
    ${bootutil_dir}/src
    ${crypto_include_dirs}
)

target_compile_definitions(bootutil_threadx
  PUBLIC
    MCUBOOT_MAX_IMG_SECTORS=128
    MCUBOOT_BOOT_MAX_ALIGN=8
    MCUBOOT_IMAGE_NUMBER=1
    MCUBOOT_HAVE_ASSERT_H=1
    ${crypto_compile_defs}
)

if(MCUBOOT_SIGN_EC256)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_SIGN_EC256=1)
endif()
if(MCUBOOT_SIGN_RSA)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_SIGN_RSA=1)
endif()
if(MCUBOOT_SIGN_ED25519)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_SIGN_ED25519=1)
endif()

if(MCUBOOT_SWAP_USING_MOVE)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_SWAP_USING_MOVE=1)
endif()
if(MCUBOOT_OVERWRITE_ONLY)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_OVERWRITE_ONLY=1)
endif()
if(MCUBOOT_DIRECT_XIP)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_DIRECT_XIP=1)
endif()

if(MCUBOOT_VALIDATE_PRIMARY_SLOT)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_VALIDATE_PRIMARY_SLOT=1)
endif()
if(MCUBOOT_HAVE_LOGGING)
  target_compile_definitions(bootutil_threadx PUBLIC MCUBOOT_HAVE_LOGGING=1)
endif()

# Standalone Bootloader Executable
add_executable(mcuboot_threadx
  src/main.c
)

target_link_libraries(mcuboot_threadx PRIVATE bootutil_threadx)
