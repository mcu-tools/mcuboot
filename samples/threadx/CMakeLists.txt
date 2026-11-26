#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
#

cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(samples_threadx C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(mcuboot_root ${CMAKE_CURRENT_SOURCE_DIR}/../..)

enable_testing()

# Add the ThreadX port library and bootloader from boot/threadx
add_subdirectory(${mcuboot_root}/boot/threadx ${CMAKE_CURRENT_BINARY_DIR}/boot_threadx)

add_subdirectory(sample_app)
add_subdirectory(tests)
