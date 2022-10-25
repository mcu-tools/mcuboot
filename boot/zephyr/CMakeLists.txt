# CMakeLists.txt for building mcuboot as a Zephyr project
#
# Copyright (c) 2017 Open Source Foundries Limited
# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

# NOTE: ${ZEPHYR_MCUBOOT_MODULE_DIR} requires delayed evaluation,
#   as it has not yet been defined.  Relative paths must be used at this point.
include(${CMAKE_CURRENT_LIST_DIR}/../../zephyr/cmake/mcuboot_zephyr_runner_mass_erase.cmake)

# find_package(Zephyr) in order to load application boilerplate:
# http://docs.zephyrproject.org/application/application.html
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

target_sources(app PRIVATE
  keys.c
  )

include(${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/cmake/mcuboot_signature_key_file_generation.cmake)
