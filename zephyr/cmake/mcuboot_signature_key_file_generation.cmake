# Copyright (c) 2022 Legrand North America, LLC.
# SPDX-License-Identifier: Apache-2.0

#cmake_minimum_required(VERSION 3.20.0)

# This is a MCUBoot-specific fragment for top-level CMakeLists.txt files.
# Sourcing this file brings in reusable logic to generate the public key file.

if(NOT CONFIG_BOOT_SIGNATURE_KEY_FILE STREQUAL "")
  # CONF_FILE points to the KConfig configuration files of the bootloader.
  foreach (mcuboot_filepath ${CONF_FILE})
    string(CONFIGURE "${mcuboot_filepath}" mcuboot_filepath_expanded)
    file(READ ${mcuboot_filepath_expanded} temp_text)
    string(FIND "${temp_text}" ${CONFIG_BOOT_SIGNATURE_KEY_FILE} match)
    if (${match} GREATER_EQUAL 0)
      if (NOT DEFINED mcuboot_signature_file_dir)
        get_filename_component(mcuboot_signature_file_dir ${mcuboot_filepath_expanded} DIRECTORY)
      else()
        message(FATAL_ERROR "Signature key file defined in multiple conf files")
      endif()
    endif()
  endforeach()

  if(IS_ABSOLUTE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
    set(KEY_FILE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  elseif((DEFINED mcuboot_signature_file_dir) AND
	 (EXISTS ${mcuboot_signature_file_dir}/${CONFIG_BOOT_SIGNATURE_KEY_FILE}))
    set(KEY_FILE ${mcuboot_signature_file_dir}/${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  else()
    set(KEY_FILE ${ZEPHYR_MCUBOOT_MODULE_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  endif()
  message("MCUBoot bootloader key file: ${KEY_FILE}")

  set(GENERATED_PUBKEY ${ZEPHYR_BINARY_DIR}/autogen-pubkey.c)
  add_custom_command(
    OUTPUT ${GENERATED_PUBKEY}
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_MCUBOOT_MODULE_DIR}/scripts/imgtool.py
    getpub
    -k
    ${KEY_FILE}
    > ${GENERATED_PUBKEY}
    DEPENDS ${KEY_FILE}
    )
  zephyr_library_sources(${GENERATED_PUBKEY})
endif()
