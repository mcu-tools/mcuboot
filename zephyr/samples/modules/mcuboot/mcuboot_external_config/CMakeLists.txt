# Copyright (c) 2022 Legrand North America, LLC.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Add a common dts overlay necessary to ensure mcuboot is linked into,
# and fits inside, the boot partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
set(DTC_OVERLAY_FILE
  "${DTC_OVERLAY_FILE};${CMAKE_CURRENT_LIST_DIR}/dts.overlay"
  CACHE STRING "" FORCE
  )

# NOTE: ${ZEPHYR_MCUBOOT_MODULE_DIR} requires delayed evaluation,
#   as it has not yet been defined.  Relative paths must be used at this point.
include(${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/mcuboot_zephyr_runner_mass_erase.cmake)

# find_package(Zephyr) in order to load application boilerplate:
# http://docs.zephyrproject.org/application/application.html
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mcuboot_exernal_config)

target_sources(app PRIVATE
  keys.c
  )

include(${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/cmake/mcuboot_signature_key_file_generation.cmake)
