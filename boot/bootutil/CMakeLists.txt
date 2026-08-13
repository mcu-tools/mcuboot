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
        src/bootutil_find_key.c
        src/bootutil_img_hash.c
        src/bootutil_img_security_cnt.c
        src/bootutil_misc.c
        src/bootutil_area.c
        src/bootutil_loader.c
        src/bootutil_public.c
        src/caps.c
        src/encrypted.c
        src/fault_injection_hardening.c
        src/image_ecdsa.c
        src/image_ed25519.c
        src/image_rsa.c
        src/image_validate.c
        src/loader.c
        src/swap_misc.c
        src/swap_move.c
        src/swap_offset.c
        src/swap_scratch.c
        src/ram_load.c
        src/tlv.c
)

# Select the FIH delay RNG implementation.
# When the bootloader uses PSA Crypto (MCUBOOT_USE_PSA_CRYPTO), use
# psa_generate_random() instead of the legacy mbedTLS entropy + CTR-DRBG
# APIs that were removed in tf-psa-crypto 1.0.
if(MCUBOOT_USE_PSA_CRYPTO)
  target_sources(bootutil PRIVATE src/fault_injection_hardening_delay_rng_psa.c)
else()
  target_sources(bootutil PRIVATE src/fault_injection_hardening_delay_rng_mbedtls.c)
endif()
