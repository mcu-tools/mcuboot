# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Mbed-MCUboot Port

cmake_minimum_required(VERSION 3.19.0 FATAL_ERROR)

get_filename_component(BOOT_UTIL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bootutil REALPATH)

set(LIB_TARGET mbed-mcuboot)
set(LIB_BOOTUTIL bootutil)

add_library(${LIB_TARGET} STATIC)

target_include_directories(${LIB_TARGET}
    PUBLIC
        include
        ${BOOT_UTIL_DIR}/include
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
)

if("_RTE_" IN_LIST MBED_CONFIG_DEFINITIONS)
    target_link_libraries(${LIB_TARGET}
    PUBLIC
        mbed-os
    )   
else()
    target_link_libraries(${LIB_TARGET}
    PUBLIC
        mbed-baremetal
    )   
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
