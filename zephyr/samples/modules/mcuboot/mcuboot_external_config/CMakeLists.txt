# Copyright (c) 2022 Legrand North America, LLC.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)


set(APPLICATION_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../boot/zephyr)

set(CONF_FILE
  "${CONF_FILE};${APPLICATION_CONFIG_DIR}/prj.conf;prj.conf"
  CACHE STRING "" FORCE
  )


# Add a common dts overlay necessary to ensure mcuboot is linked into,
# and fits inside, the boot partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
set(DTC_OVERLAY_FILE
  "${DTC_OVERLAY_FILE};${APPLICATION_CONFIG_DIR}/dts.overlay"
  CACHE STRING "" FORCE
  )

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../boot/zephyr/CMakeLists.txt)
project(mcuboot_exernal_config)

target_sources(app PRIVATE
    keys.c
  )
