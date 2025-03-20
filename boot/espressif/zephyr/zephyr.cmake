# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

zephyr_library_include_directories(
    ${inc_directories}
    )

zephyr_library_sources(
    ${main_src}
    ${bootutil_srcs}
    ${crypto_srcs}
    ${port_srcs}
    )

zephyr_library_link_libraries(
  gcc
  -T${ld_output}
  ${LDFLAGS}
  )

if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
  zephyr_include_directories(
    ${ESP_HAL_PATH}/components/${MCUBOOT_ARCH}/${MCUBOOT_TARGET}/include
    ${ESP_HAL_PATH}/components/${MCUBOOT_ARCH}/include
  )
endif()

add_subdirectory(${ESPRESSIF_PORT_DIR}/hal)
zephyr_library_link_libraries(hal)
