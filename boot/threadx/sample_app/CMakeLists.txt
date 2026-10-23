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
        ../include
        ${BOOTUTIL_DIR}/include
)

target_link_libraries(threadx_mcuboot_sample_app
    PRIVATE
        bootutil_threadx
)
