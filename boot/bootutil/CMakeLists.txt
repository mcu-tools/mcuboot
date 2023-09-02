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

target_sources(bootutil
    PRIVATE
        src/boot_record.c
        src/bootutil_misc.c
        src/bootutil_public.c
        src/caps.c
        src/encrypt_ec256.c
        src/encrypt_kw.c
        src/encrypt_misc.c
        src/encrypt_rsa.c
        src/encrypt_x25519.c
        src/encrypted.c
        src/fault_injection_hardening.c
        src/fault_injection_hardening_delay_rng_mbedtls.c
        src/image_ecdsa.c
        src/image_ed25519.c
        src/image_rsa.c
        src/image_validate.c
        src/loader.c
        src/swap_misc.c
        src/swap_move.c
        src/swap_scratch.c
        src/tlv.c
)
