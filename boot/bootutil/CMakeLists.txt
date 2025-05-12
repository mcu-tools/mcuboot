#------------------------------------------------------------------------------
# Copyright (c) 2020-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#------------------------------------------------------------------------------

add_library(bootutil STATIC)

target_include_directories(bootutil
    PUBLIC
        include
    PRIVATE
        src
)

set(BOOTUTIL_SOURCES
    src/boot_record.c
    src/bootutil_misc.c
    src/bootutil_public.c
    src/caps.c
    src/encrypted.c
    src/fault_injection_hardening.c
    src/fault_injection_hardening_delay_rng_mbedtls.c
    src/image_ecdsa.c
    src/image_ed25519.c
    src/image_rsa.c
    src/loader.c
    src/swap_misc.c
    src/swap_move.c
    src/swap_scratch.c
    src/tlv.c
)

if(CONFIG_BOOT_RAM_LOAD)
    list(APPEND BOOTUTIL_SOURCES src/ram_load.c)
endif()

if(MCUBOOT_IMAGE_MULTI_SIG_SUPPORT)
    list(APPEND BOOTUTIL_SOURCES src/image_multi_sig.c)
else()
    list(APPEND BOOTUTIL_SOURCES src/image_validate.c)
endif()

target_sources(bootutil PRIVATE ${BOOTUTIL_SOURCES})
