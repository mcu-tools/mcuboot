#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
#

add_executable(threadx_mcuboot_sample_app
  main.c
)

target_include_directories(threadx_mcuboot_sample_app
  PRIVATE
    ${mcuboot_root}/boot/threadx/include
    ${mcuboot_root}/boot/bootutil/include
)

target_link_libraries(threadx_mcuboot_sample_app
  PRIVATE
    bootutil_threadx
)
