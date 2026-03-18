# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
cmake_policy(SET CMP0109 NEW)

include(${CMAKE_CURRENT_LIST_DIR}/../tools/utils.cmake)

message(STATUS "Building MCUboot for Zephyr OS -- ESPRESSIF PORT")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Set Zephyr referenced variables
set(MCUBOOT_ROOT_DIR ${ZEPHYR_MCUBOOT_MODULE_DIR})
set(ESPRESSIF_PORT_DIR ${MCUBOOT_ROOT_DIR}/boot/espressif)
set(MCUBOOT_TARGET ${CONFIG_SOC})
set(ESP_HAL_PATH ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR})
set(APP_EXECUTABLE ${ZEPHYR_CURRENT_LIBRARY})
set(EXPECTED_IDF_HAL_VERSION "5.1.6")

project(mcuboot_espressif_port)

list(APPEND conf_defines "-DCONFIG_MCUBOOT_ESPRESSIF=1")
set(LINKER_SCRIPT_INCLUDES ${SOC_FULL_DIR}/${MCUBOOT_TARGET})

include(../common.cmake)

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

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../hal ${CMAKE_CURRENT_BINARY_DIR}/hal)
zephyr_library_link_libraries(hal)
