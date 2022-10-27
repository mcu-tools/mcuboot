#
# Copyright (c) 2020 Nordic Semiconductor ASA
# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(caps)

target_sources(app PRIVATE main.c)

target_include_directories(app PRIVATE
  ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/bootutil/src
  )
#zephyr_compile_definitions(MCUBOOT_SWAP_USING_SCRATCH=0)
