# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Mbed-MCUboot Port

cmake_minimum_required(VERSION 3.19.0 FATAL_ERROR)

set(LIB_TARGET mbed-mcuboot)
set(LIB_BOOTUTIL bootutil)

add_library(${LIB_TARGET} STATIC)

# TODO: It's good practice to only put the top-level path in the include path.
#       The header path would then be relative to the include directory. 
#       For example, "os/os_malloc.h" instead of "os_malloc.h"
target_include_directories(${LIB_TARGET}
    PUBLIC
        include
        include/flash_map_backend/
        include/mcuboot_config
        include/os
        include/sysflash
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
        mbed-baremetal
        mbed-storage-flashiap
        mbed-storage-blockdevice
)

# TODO: Add comment here about bootutil
target_include_directories(${LIB_BOOTUTIL}
    PUBLIC
        include
)

target_link_libraries(${LIB_BOOTUTIL}
    PUBLIC
        mbed-mcuboot
        mbed-baremetal
        mbed-mbedtls
)

