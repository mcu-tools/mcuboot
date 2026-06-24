# Copyright (c) 2026 Intercreate, Inc.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cobs_test)

set(BOOT_SERIAL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../boot_serial/src)

target_sources(app PRIVATE
  src/main.c
  ${BOOT_SERIAL_SRC_DIR}/cobs.c
)

target_include_directories(app PRIVATE ${BOOT_SERIAL_SRC_DIR})
