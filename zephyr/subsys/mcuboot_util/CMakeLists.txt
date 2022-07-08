# Copyright (c) 2020 Nordic Semiconductor ASA
# Copyright (c) 2022 Legrand North America, LLC.
#
# SPDX-License-Identifier: Apache-2.0


zephyr_interface_library_named(MCUBOOT_BOOTUTIL)

zephyr_include_directories(MCUBOOT_BOOTUTIL INTERFACE
  ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/bootutil
  ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/bootutil/include
  ${ZEPHYR_MCUBOOT_MODULE_DIR}/zephyr/include
)

zephyr_library_named(mcuboot_util)
zephyr_library_sources(
  ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/bootutil/src/bootutil_public.c
  )

# Sensitivity to the TEST_BOOT_IMAGE_ACCESS_HOOKS define is implemented for
# allowing the test-build with the hooks feature enabled.
if(TEST_BOOT_IMAGE_ACCESS_HOOKS)
  zephyr_library_sources(
    ${APPLICATION_SOURCE_DIR}/hooks_sample.c
  )
endif()

zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
target_link_libraries(MCUBOOT_BOOTUTIL INTERFACE zephyr_interface)

if(CONFIG_BOOT_USE_TINYCRYPT)
target_include_directories(MCUBOOT_BOOTUTIL INTERFACE
  ${ZEPHYR_MCUBOOT_MODULE_DIR}/ext/tinycrypt/lib/include
)
endif()

if(CONFIG_BOOT_USE_MBEDTLS)
  zephyr_link_libraries(mbedTLS)
endif()
