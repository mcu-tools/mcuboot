#
# Copyright (c) 2020 Nordic Semiconductor ASA
# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mcuboot_util_caps)

set(SOURCES main.c)

include_directories(
  ${CMAKE_BINARY_DIR}/zephyr/include/generated
  ../../../../../boot/bootutil/include
  ../../../../../boot/zephyr/include
  ../../../../../boot/bootutil/src
  )
#zephyr_compile_definitions(MCUBOOT_SWAP_USING_SCRATCH=0)
