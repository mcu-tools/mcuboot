# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Pull in functions for working with imgtool
include(mcuboot_imgtool.cmake)

# Mbed-MCUboot Port
cmake_minimum_required(VERSION 3.19.0 FATAL_ERROR)
get_filename_component(BOOT_UTIL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bootutil REALPATH)

set(LIB_TARGET mbed-mcuboot)
set(LIB_BOOTUTIL bootutil)

add_library(${LIB_TARGET} STATIC)

target_include_directories(${LIB_TARGET}
  PUBLIC
    include
    ${BOOT_UTIL_DIR}/src
)

target_sources(${LIB_TARGET}
  PRIVATE
    mcuboot_main.cpp
    app_enc_keys.c
    src/flash_map_backend.cpp
    src/secondary_bd.cpp
)

target_link_libraries(${LIB_TARGET}
  PUBLIC
    bootutil  # Cross-dependency
    mbed-mbedtls
    mbed-storage-flashiap
    mbed-storage-blockdevice
    mbed-core-flags
)

# Add signing key generated source file
mcuboot_generate_signing_keys_file(${CMAKE_CURRENT_BINARY_DIR}/signing_keys.c)
target_sources(${LIB_TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/signing_keys.c)

if("MCUBOOT_ENCRYPT_RSA=1" IN_LIST MBED_CONFIG_DEFINITIONS)
  mcuboot_generate_encryption_key_file(${CMAKE_CURRENT_BINARY_DIR}/enc_keys.c)
  target_sources(${LIB_TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/enc_keys.c)
endif()

# The cross-dependency requires that bootutil have access to the mbed port's
# include directory and is linked with the appropriate mbed-specific libraries.
target_include_directories(${LIB_BOOTUTIL}
  PUBLIC
    include
)

target_link_libraries(${LIB_BOOTUTIL}
  PUBLIC
    mbed-mcuboot
    mbed-mbedtls
)
