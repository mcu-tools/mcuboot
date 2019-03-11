# CMakeLists.txt for building mcuboot as a Zephyr project
#
# Copyright (c) 2017 Open Source Foundries Limited
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.8.2)

# Default to qemu_x86 if no board has been specified.
set(BOARD qemu_x86)

# Add a common dts overlay necessary to ensure mcuboot is linked into,
# and fits inside, the boot partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
if(DTC_OVERLAY_FILE)
  set(DTC_OVERLAY_FILE
      "${DTC_OVERLAY_FILE} ${CMAKE_CURRENT_LIST_DIR}/dts.overlay")
else()
  set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dts.overlay)
endif()

if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.overlay)
  set(DTC_OVERLAY_FILE
      "${DTC_OVERLAY_FILE} ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}.overlay")
endif()

# Enable Zephyr runner options which request mass erase if so
# configured.
#
# Note that this also disables the default "leave" option when
# targeting STM32 DfuSe devices with dfu-util, making the chip stay in
# the bootloader after flashing.
#
# That's the right thing, because mcuboot has nothing to do since the
# chip was just erased. The next thing the user is going to want to do
# is flash the application. (Developers can reset DfuSE devices
# manually to test mcuboot behavior on an otherwise erased flash
# device.)
macro(app_set_runner_args)
  if(CONFIG_ZEPHYR_TRY_MASS_ERASE)
    board_runner_args(dfu-util "--dfuse-modifiers=force:mass-erase")
    board_runner_args(pyocd "--flash-opt=-e=chip")
    board_runner_args(nrfjprog "--erase")
  endif()
endmacro()

# Standard Zephyr application boilerplate:
# http://docs.zephyrproject.org/application/application.html
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

# Path to "boot" subdirectory of repository root.
get_filename_component(BOOT_DIR ${APPLICATION_SOURCE_DIR} DIRECTORY)
# Path to top-level repository root directory.
get_filename_component(MCUBOOT_DIR ${BOOT_DIR} DIRECTORY)
# Path to tinycrypt library source subdirectory of MCUBOOT_DIR.
set(TINYCRYPT_DIR "${MCUBOOT_DIR}/ext/tinycrypt/lib")
# Path to mbed-tls' asn1 parser library.
set(MBEDTLS_ASN1_DIR "${MCUBOOT_DIR}/ext/mbedtls")

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
  flash_map_extended.c
  os.c
  keys.c
  )

if(NOT DEFINED CONFIG_FLASH_PAGE_LAYOUT)
  zephyr_library_sources(
	flash_map_legacy.c
	)
endif()

# Generic bootutil sources and includes.
zephyr_library_include_directories(${BOOT_DIR}/bootutil/include)
zephyr_library_sources(
  ${BOOT_DIR}/bootutil/src/loader.c
  ${BOOT_DIR}/bootutil/src/bootutil_misc.c
  ${BOOT_DIR}/bootutil/src/image_validate.c
  ${BOOT_DIR}/bootutil/src/encrypted.c
  ${BOOT_DIR}/bootutil/src/image_rsa.c
  ${BOOT_DIR}/bootutil/src/image_ec256.c
  ${BOOT_DIR}/bootutil/src/caps.c
  )

if(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256)
  # When using ECDSA signatures, pull in our copy of the tinycrypt library.
  zephyr_library_include_directories(
	 ${BOOT_DIR}/zephyr/include
	   ${TINYCRYPT_DIR}/include
	${MBEDTLS_ASN1_DIR}/include
	)

  zephyr_library_sources(
	${TINYCRYPT_DIR}/source/ecc.c
	${TINYCRYPT_DIR}/source/ecc_dsa.c
	${TINYCRYPT_DIR}/source/sha256.c
	${TINYCRYPT_DIR}/source/utils.c

	# Additionally pull in just the ASN.1 parser from mbedTLS.
	${MBEDTLS_ASN1_DIR}/src/asn1parse.c
	${MBEDTLS_ASN1_DIR}/src/platform_util.c
	)

  # Since here we are not using Zephyr's mbedTLS but rather our own, we need
  # to set MBEDTLS_CONFIG_FILE ourselves. When using Zephyr's copy, this
  # variable is set by its Kconfig in the Zephyr codebase.
  zephyr_library_compile_definitions(
	MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/include/mcuboot-mbedtls-cfg.h"
	)
elseif(CONFIG_BOOT_SIGNATURE_TYPE_RSA)
  # Use mbedTLS provided by Zephyr for RSA signatures. (Its config file
  # is set using Kconfig.)
  zephyr_include_directories(include)
endif()

if(CONFIG_MCUBOOT_SERIAL)
  zephyr_sources(${BOOT_DIR}/zephyr/serial_adapter.c)
  zephyr_sources(${BOOT_DIR}/boot_serial/src/boot_serial.c)

  zephyr_include_directories(${BOOT_DIR}/bootutil/include)
  zephyr_include_directories(${BOOT_DIR}/boot_serial/include)
  zephyr_include_directories(include)

  zephyr_link_libraries_ifdef(
	CONFIG_TINYCBOR
	TINYCBOR
	)

  zephyr_include_directories_ifdef(
	CONFIG_BOOT_ERASE_PROGRESSIVELY
	${BOOT_DIR}/bootutil/src
	)
endif()

if(NOT CONFIG_BOOT_SIGNATURE_KEY_FILE STREQUAL "")
  if(IS_ABSOLUTE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
    set(KEY_FILE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  else()
    set(KEY_FILE ${MCUBOOT_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE})
  endif()
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
