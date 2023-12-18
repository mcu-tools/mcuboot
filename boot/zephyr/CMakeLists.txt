# CMakeLists.txt for building mcuboot as a Zephyr project
#
# Copyright (c) 2017 Open Source Foundries Limited
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

# find_package(Zephyr) in order to load application boilerplate:
# http://docs.zephyrproject.org/application/application.html
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

# Path to "boot" subdirectory of repository root.
get_filename_component(BOOT_DIR ${APPLICATION_SOURCE_DIR} DIRECTORY)
# Path to top-level repository root directory.
get_filename_component(MCUBOOT_DIR ${BOOT_DIR} DIRECTORY)
# Path to tinycrypt library source subdirectory of MCUBOOT_DIR.
set(TINYCRYPT_DIR "${MCUBOOT_DIR}/ext/tinycrypt/lib")
assert_exists(TINYCRYPT_DIR)
set(TINYCRYPT_SHA512_DIR "${MCUBOOT_DIR}/ext/tinycrypt-sha512/lib")
assert_exists(TINYCRYPT_SHA512_DIR)
# Path to crypto-fiat
set(FIAT_DIR "${MCUBOOT_DIR}/ext/fiat")
assert_exists(FIAT_DIR)
# Path to mbed-tls' asn1 parser library.
set(MBEDTLS_ASN1_DIR "${MCUBOOT_DIR}/ext/mbedtls-asn1")
assert_exists(MBEDTLS_ASN1_DIR)
set(NRF_DIR "${MCUBOOT_DIR}/ext/nrf")

if(CONFIG_BOOT_USE_NRF_CC310_BL)
set(NRFXLIB_DIR ${ZEPHYR_BASE}/../nrfxlib)
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
  include
  targets
  )
if(EXISTS targets/${BOARD}.h)
  zephyr_library_compile_definitions(MCUBOOT_TARGET_CONFIG="${BOARD}.h")
endif()

# Zephyr port-specific sources.
zephyr_library_sources(
  main.c
  io.c
  flash_map_extended.c
  os.c
  keys.c
  )

if(DEFINED CONFIG_ENABLE_MGMT_PERUSER)
  zephyr_library_sources(
    boot_serial_extensions.c
    )

  zephyr_linker_sources_ifdef(
    CONFIG_ENABLE_MGMT_PERUSER
    SECTIONS include/boot_serial/boot_serial.ld
  )

  if(DEFINED CONFIG_BOOT_MGMT_CUSTOM_STORAGE_ERASE OR DEFINED CONFIG_BOOT_MGMT_CUSTOM_IMG_LIST)
    zephyr_library_sources(
      boot_serial_extension_zephyr_basic.c
      )
  endif()
endif()

if(NOT DEFINED CONFIG_FLASH_PAGE_LAYOUT)
  zephyr_library_sources(
    flash_map_legacy.c
    )
endif()

if(DEFINED CONFIG_BOOT_SHARE_BACKEND_RETENTION)
  zephyr_library_sources(
    shared_data.c
    )
endif()

# Generic bootutil sources and includes.
zephyr_library_include_directories(${BOOT_DIR}/bootutil/include)
zephyr_library_sources(
  ${BOOT_DIR}/bootutil/src/image_validate.c
  ${BOOT_DIR}/bootutil/src/tlv.c
  ${BOOT_DIR}/bootutil/src/encrypted.c
  ${BOOT_DIR}/bootutil/src/image_rsa.c
  ${BOOT_DIR}/bootutil/src/image_ecdsa.c
  ${BOOT_DIR}/bootutil/src/image_ed25519.c
  ${BOOT_DIR}/bootutil/src/bootutil_misc.c
  ${BOOT_DIR}/bootutil/src/fault_injection_hardening.c
  )

if(DEFINED CONFIG_MEASURED_BOOT OR DEFINED CONFIG_BOOT_SHARE_DATA)
  zephyr_library_sources(
    ${BOOT_DIR}/bootutil/src/boot_record.c
    )

  # Set a define for this file which will allow inclusion of the Zephyr version
  # include file
  set_source_files_properties(
    ${BOOT_DIR}/bootutil/src/boot_record.c
    PROPERTIES COMPILE_FLAGS -DZEPHYR_VER_INCLUDE=1
    )
endif()

# library which might be common source code for MCUBoot and an application
zephyr_link_libraries(MCUBOOT_BOOTUTIL)

if(CONFIG_BOOT_FIH_PROFILE_HIGH)
zephyr_library_sources(
  ${BOOT_DIR}/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c
  )
endif()

if(CONFIG_SINGLE_APPLICATION_SLOT)
zephyr_library_sources(
  ${BOOT_DIR}/zephyr/single_loader.c
  )
zephyr_library_include_directories(${BOOT_DIR}/bootutil/src)
elseif(CONFIG_BOOT_FIRMWARE_LOADER)
zephyr_library_sources(
  ${BOOT_DIR}/zephyr/firmware_loader.c
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

if(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256 OR CONFIG_BOOT_ENCRYPT_EC256)
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
    ${BOOT_DIR}/zephyr/include
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
    MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/include/mcuboot-mbedtls-cfg.h"
    )
elseif(CONFIG_BOOT_SIGNATURE_TYPE_NONE)
  zephyr_library_include_directories(
    ${BOOT_DIR}/zephyr/include
    ${TINYCRYPT_DIR}/include
    )

  zephyr_library_sources(
    ${TINYCRYPT_DIR}/source/sha256.c
    ${TINYCRYPT_DIR}/source/utils.c
    )
elseif(CONFIG_BOOT_SIGNATURE_TYPE_RSA)
  # Use mbedTLS provided by Zephyr for RSA signatures. (Its config file
  # is set using Kconfig.)
  zephyr_include_directories(include)
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
      ${BOOT_DIR}/zephyr/include
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
      MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/include/mcuboot-mbedtls-cfg.h"
      )
  else()
    zephyr_include_directories(include)
  endif()

  zephyr_library_include_directories(
    ${BOOT_DIR}/zephyr/include
    ${FIAT_DIR}/include/
  )

  zephyr_library_sources(
    ${FIAT_DIR}/src/curve25519.c
  )
endif()

if(CONFIG_BOOT_ENCRYPT_EC256 OR CONFIG_BOOT_ENCRYPT_X25519)
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
  zephyr_sources(${BOOT_DIR}/zephyr/serial_adapter.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/boot_serial.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/zcbor_bulk.c)

  zephyr_include_directories(${BOOT_DIR}/bootutil/include)
  zephyr_include_directories(${BOOT_DIR}/boot_serial/include)
  zephyr_include_directories(include)

  zephyr_include_directories_ifdef(
    CONFIG_BOOT_ERASE_PROGRESSIVELY
    ${BOOT_DIR}/bootutil/src
    )

  if(CONFIG_BOOT_ENCRYPT_IMAGE)
    zephyr_library_sources(
      ${BOOT_DIR}/boot_serial/src/boot_serial_encryption.c
    )
  endif()
endif()

if(NOT CONFIG_BOOT_SIGNATURE_KEY_FILE STREQUAL "")
  # CONF_FILE points to the KConfig configuration files of the bootloader.
  foreach (filepath ${CONF_FILE})
    file(READ ${filepath} temp_text)
    string(FIND "${temp_text}" ${CONFIG_BOOT_SIGNATURE_KEY_FILE} match)
    if (${match} GREATER_EQUAL 0)
      if (NOT DEFINED CONF_DIR)
        get_filename_component(CONF_DIR ${filepath} DIRECTORY)
      else()
        message(FATAL_ERROR "Signature key file defined in multiple conf files")
      endif()
    endif()
  endforeach()

  if(IS_ABSOLUTE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
    set(KEY_FILE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  elseif((DEFINED CONF_DIR) AND
	 (EXISTS ${CONF_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE}))
    set(KEY_FILE ${CONF_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  else()
    set(KEY_FILE ${MCUBOOT_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  endif()
  message("MCUBoot bootloader key file: ${KEY_FILE}")

  set(GENERATED_PUBKEY ${ZEPHYR_BINARY_DIR}/autogen-pubkey.c)
  add_custom_command(
    OUTPUT ${GENERATED_PUBKEY}
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${MCUBOOT_DIR}/scripts/imgtool.py
    getpub
    -k
    ${KEY_FILE}
    > ${GENERATED_PUBKEY}
    DEPENDS ${KEY_FILE}
    )
  zephyr_library_sources(${GENERATED_PUBKEY})
endif()

if(CONFIG_BOOT_ENCRYPTION_KEY_FILE AND NOT CONFIG_BOOT_ENCRYPTION_KEY_FILE STREQUAL "")
  # CONF_FILE points to the KConfig configuration files of the bootloader.
  unset(CONF_DIR)
  foreach(filepath ${CONF_FILE})
    file(READ ${filepath} temp_text)
    string(FIND "${temp_text}" ${CONFIG_BOOT_ENCRYPTION_KEY_FILE} match)
    if(${match} GREATER_EQUAL 0)
      if(NOT DEFINED CONF_DIR)
        get_filename_component(CONF_DIR ${filepath} DIRECTORY)
      else()
        message(FATAL_ERROR "Encryption key file defined in multiple conf files")
      endif()
    endif()
  endforeach()

  if(IS_ABSOLUTE ${CONFIG_BOOT_ENCRYPTION_KEY_FILE})
    set(KEY_FILE ${CONFIG_BOOT_ENCRYPTION_KEY_FILE})
  elseif((DEFINED CONF_DIR) AND
	 (EXISTS ${CONF_DIR}/${CONFIG_BOOT_ENCRYPTION_KEY_FILE}))
    set(KEY_FILE ${CONF_DIR}/${CONFIG_BOOT_ENCRYPTION_KEY_FILE})
  else()
    set(KEY_FILE ${MCUBOOT_DIR}/${CONFIG_BOOT_ENCRYPTION_KEY_FILE})
  endif()
  message("MCUBoot bootloader encryption key file: ${KEY_FILE}")

  set(GENERATED_ENCKEY ${ZEPHYR_BINARY_DIR}/autogen-enckey.c)
  add_custom_command(
    OUTPUT ${GENERATED_ENCKEY}
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${MCUBOOT_DIR}/scripts/imgtool.py
    getpriv
    -k
    ${KEY_FILE}
    > ${GENERATED_ENCKEY}
    DEPENDS ${KEY_FILE}
    )
  zephyr_library_sources(${GENERATED_ENCKEY})
endif()

if(CONFIG_MCUBOOT_CLEANUP_ARM_CORE)
zephyr_library_sources(
  ${BOOT_DIR}/zephyr/arm_cleanup.c
)
endif()

if(SYSBUILD)
  function(align_up num align result)
    math(EXPR out "(((${num}) + ((${align}) - 1)) & ~((${align}) - 1))")
    set(${result} "${out}" PARENT_SCOPE)
  endfunction()

  function(dt_get_parent node)
    string(FIND "${${node}}" "/" pos REVERSE)

    if(pos EQUAL -1)
      message(ERROR "Unable to get parent of node: ${${node}}")
    endif()

    string(SUBSTRING "${${node}}" 0 ${pos} ${node})
    set(${node} "${${node}}" PARENT_SCOPE)
  endfunction()

  if(CONFIG_SINGLE_APPLICATION_SLOT OR CONFIG_BOOT_FIRMWARE_LOADER OR CONFIG_BOOT_SWAP_USING_SCRATCH OR CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_UPGRADE_ONLY OR CONFIG_BOOT_DIRECT_XIP OR CONFIG_BOOT_RAM_LOAD)
    # TODO: RAM LOAD support
    dt_nodelabel(slot0_flash NODELABEL "slot0_partition")
    dt_get_parent(slot0_flash)
    dt_get_parent(slot0_flash)

    if(NOT CONFIG_SINGLE_APPLICATION_SLOT)
      dt_nodelabel(slot1_flash NODELABEL "slot1_partition")
      dt_get_parent(slot1_flash)
      dt_get_parent(slot1_flash)

      if(NOT "${slot0_flash}" STREQUAL "${slot1_flash}")
        # Check both slots for the one with the largest write/erase block size
        dt_prop(erase_size_slot0 PATH "${slot0_flash}" PROPERTY "erase-block-size")
        dt_prop(write_size_slot0 PATH "${slot0_flash}" PROPERTY "write-block-size")
        dt_prop(erase_size_slot1 PATH "${slot1_flash}" PROPERTY "erase-block-size")
        dt_prop(write_size_slot1 PATH "${slot1_flash}" PROPERTY "write-block-size")

        if(DEFINED erase_size_slot0 AND DEFINED erase_size_slot1)
          if(${erase_size_slot0} GREATER ${erase_size_slot1})
            set(erase_size ${erase_size_slot0})
          else()
            set(erase_size ${erase_size_slot1})
          endif()
        elseif(DEFINED erase_size_slot0)
          set(erase_size ${erase_size_slot0})
        elseif(DEFINED erase_size_slot1)
          set(erase_size ${erase_size_slot1})
        endif()

        if(DEFINED write_size_slot0 AND DEFINED write_size_slot1)
          if(${write_size_slot0} GREATER ${write_size_slot1})
            set(write_size ${write_size_slot0})
          else()
            set(write_size ${write_size_slot1})
          endif()
        elseif(DEFINED write_size_slot0)
          set(write_size ${write_size_slot0})
        elseif(DEFINED write_size_slot1)
          set(write_size ${write_size_slot1})
        endif()
      else()
        dt_prop(erase_size PATH "${slot0_flash}" PROPERTY "erase-block-size")
        dt_prop(write_size PATH "${slot0_flash}" PROPERTY "write-block-size")
      endif()
    else()
      dt_prop(erase_size PATH "${slot0_flash}" PROPERTY "erase-block-size")
      dt_prop(write_size PATH "${slot0_flash}" PROPERTY "write-block-size")
    endif()

    if(NOT DEFINED erase_size)
      message(WARNING "Unable to determine erase size of slot0 or slot1 partition, setting to 1 (this is probably wrong)")
      set(erase_size 1)
    endif()

    if(NOT DEFINED write_size)
      message(WARNING "Unable to determine write size of slot0 or slot1 partition, setting to 8 (this is probably wrong)")
      set(write_size 8)
    elseif(write_size LESS 8)
      set(write_size 8)
    endif()

    set(key_size 0)

    # Boot trailer magic size
    set(boot_magic_size 16)

    # Estimates for trailer TLV data size, this was taken from hello world builds for nrf52840dk
    if(CONFIG_BOOT_SIGNATURE_TYPE_RSA)
      if(CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN EQUAL 3072)
        set(boot_tlv_estimate 464)
      else()
        set(boot_tlv_estimate 336)
      endif()
    elseif(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256)
      set(boot_tlv_estimate 150)
    elseif(CONFIG_BOOT_SIGNATURE_TYPE_ED25519)
      set(boot_tlv_estimate 144)
    else()
      set(boot_tlv_estimate 40)
    endif()

    if(CONFIG_BOOT_ENCRYPT_RSA OR CONFIG_BOOT_ENCRYPT_EC256 OR CONFIG_BOOT_ENCRYPT_X25519)
      # 128-bit AES key size
      set(boot_enc_key_size 16)

      if(CONFIG_BOOT_SWAP_SAVE_ENCTLV)
        if(CONFIG_BOOT_ENCRYPT_RSA)
          set(key_size 256)
        elseif(CONFIG_BOOT_ENCRYPT_EC256)
          math(EXPR key_size "65 + 32 + ${boot_enc_key_size}")
        elseif(CONFIG_BOOT_ENCRYPT_X25519)
          math(EXPR key_size "32 + 32 + ${boot_enc_key_size}")
        endif()
      else()
        set(key_size "${boot_enc_key_size}")
      endif()

      align_up(${key_size} ${write_size} key_size)
      math(EXPR key_size "${key_size} * 2")
    endif()

    align_up(${boot_magic_size} ${write_size} boot_magic_size)

    if(CONFIG_SINGLE_APPLICATION_SLOT OR CONFIG_BOOT_FIRMWARE_LOADER)
      set(boot_swap_data_size 0)
    else()
      math(EXPR boot_swap_data_size "${write_size} * 4")
    endif()

    if(CONFIG_BOOT_SWAP_USING_SCRATCH OR CONFIG_BOOT_SWAP_USING_MOVE)
      math(EXPR boot_status_data_size "${CONFIG_BOOT_MAX_IMG_SECTORS} * (3 * ${write_size})")
    else()
      set(boot_status_data_size 0)
    endif()

    math(EXPR required_size "${key_size} + ${boot_magic_size} + ${boot_swap_data_size} + ${boot_status_data_size} + ${boot_tlv_estimate}")

    align_up(${required_size} ${erase_size} required_size)

    if(CONFIG_BOOT_SWAP_USING_MOVE)
      math(EXPR required_size "${required_size} + ${erase_size}")
    endif()
  else()
    set(required_size 0)
  endif()

  set(mcuboot_image_footer_size ${required_size} CACHE INTERNAL "Estimated MCUboot image trailer size" FORCE)
endif()
