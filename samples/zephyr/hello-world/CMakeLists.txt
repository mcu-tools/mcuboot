# Top-level CMakeLists.txt for the skeleton application.
#
# Copyright (c) 2017 Open Source Foundries Limited
# Copyright (c) 2018 Foundries.io Ltd
#
# SPDX-License-Identifier: Apache-2.0
#
# This provides a basic application structure suitable for loading by
# mcuboot, which is easy to customize on a per-board basis. It can be
# used as a starting point for new applications.

cmake_minimum_required(VERSION 3.8)

# The default top-level application configuration is prj.conf.
# You can place additional board-specific files in boards/${BOARD}.conf,
# and they will be merged into the configuration along with prj.conf.
macro(set_conf_file)
  if(EXISTS ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf)
    set(CONF_FILE "prj.conf ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf")
  else()
    set(CONF_FILE "prj.conf")
  endif()
endmacro()

# Standard Zephyr application boilerplate.
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

# This string ends up getting printed in the device console
if (NOT DEFINED FROM_WHO)
  set(FROM_WHO Zephyr)
endif()

target_compile_definitions(app PRIVATE "-DMCUBOOT_HELLO_WORLD_FROM=\"${FROM_WHO}\"")

target_sources(app PRIVATE src/main.c)
