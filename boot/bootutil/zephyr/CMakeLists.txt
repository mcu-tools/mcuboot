# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MCUBOOT_BOOTUTIL_LIB)

zephyr_interface_library_named(MCUBOOT_BOOTUTIL)

target_include_directories(MCUBOOT_BOOTUTIL INTERFACE
  ../
  ../include
  ../../zephyr/include
)

zephyr_library()
zephyr_library_sources(
  ../src/bootutil_public.c
    )
zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
target_link_libraries(MCUBOOT_BOOTUTIL INTERFACE zephyr_interface)
endif()
