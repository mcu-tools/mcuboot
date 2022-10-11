# CMakeLists.txt for building mcuboot as a Zephyr project
#
# Copyright (c) 2017 Open Source Foundries Limited
# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

# Add a common dts overlay necessary to ensure mcuboot is linked into,
# and fits inside, the boot partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
if(DTC_OVERLAY_FILE)
  set(DTC_OVERLAY_FILE
    "${DTC_OVERLAY_FILE} ${CMAKE_CURRENT_LIST_DIR}/dts.overlay"
    CACHE STRING "" FORCE
    )
else()
  set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dts.overlay)
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

# find_package(Zephyr) in order to load application boilerplate:
# http://docs.zephyrproject.org/application/application.html
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

target_sources(app PRIVATE
  keys.c
  )

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
