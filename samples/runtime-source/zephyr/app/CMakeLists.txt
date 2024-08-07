# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(non_flash_backend_app)

if (NOT DEFINED FROM_WHO)
  set(FROM_WHO Zephyr)
endif()

target_compile_definitions(app PRIVATE "-DMCUBOOT_HELLO_WORLD_FROM=\"${FROM_WHO}\"")

target_sources(app PRIVATE src/main.c)
