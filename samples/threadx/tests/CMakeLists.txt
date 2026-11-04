#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2026 Eclipse Foundation and ThreadX contributors
#

enable_testing()

add_executable(threadx_mcuboot_test
  test_mcuboot_threadx.c
)

target_include_directories(threadx_mcuboot_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${mcuboot_root}/boot/threadx/include
    ${mcuboot_root}/boot/bootutil/include
)

target_link_libraries(threadx_mcuboot_test
  PRIVATE
    bootutil_threadx
)

add_test(NAME threadx_mcuboot_verification COMMAND threadx_mcuboot_test)
