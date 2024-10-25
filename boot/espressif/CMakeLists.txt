# SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0109 NEW)

include(${CMAKE_CURRENT_LIST_DIR}/tools/utils.cmake)

message(STATUS "Building MCUboot -- ESPRESSIF PORT")

if(NOT DEFINED MCUBOOT_TARGET)
  message(FATAL_ERROR "MCUBOOT_TARGET not defined. Please pass -DMCUBOOT_TARGET flag.")
endif()

set(MCUBOOT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(ESPRESSIF_PORT_DIR ${MCUBOOT_ROOT_DIR}/boot/espressif)
set(APP_NAME mcuboot_${MCUBOOT_TARGET})
set(APP_EXECUTABLE ${APP_NAME}.elf)
set(EXPECTED_IDF_HAL_VERSION "5.1.6")

project(mcuboot_${MCUBOOT_TARGET})

add_executable(
  ${APP_EXECUTABLE}
  ${ESPRESSIF_PORT_DIR}/main.c
  )

add_definitions(-DCONFIG_MCUBOOT_ESPRESSIF=1)

# Set MCUboot Espressif Port configuration file
if(NOT DEFINED MCUBOOT_CONFIG_FILE)
  set(MCUBOOT_CONFIG_FILE "${ESPRESSIF_PORT_DIR}/port/${MCUBOOT_TARGET}/bootloader.conf")
  message("MCUBOOT_CONFIG_FILE: ${MCUBOOT_CONFIG_FILE}")
endif()

string(REPLACE " " ";" MCUBOOT_CONFIG_FILE_LIST "${MCUBOOT_CONFIG_FILE}")
foreach(CONFIG_FILE ${MCUBOOT_CONFIG_FILE_LIST})
  if(NOT EXISTS "${CONFIG_FILE}")
      message(FATAL_ERROR "MCUboot configuration file does not exist at ${CONFIG_FILE}")
  endif()
  parse_and_set_config_file(${CONFIG_FILE})
endforeach()

# Fetch and set toolchain
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED TOOLCHAIN_BIN_DIR)
    message("CMAKE_TOOLCHAIN_FILE not defined, searching for toolchain compiler in TOOLCHAIN_BIN_DIR: ${TOOLCHAIN_BIN_DIR}")
    set(CMAKE_SYSTEM_NAME Generic)

    file(GLOB C_COMPILER_BIN "${TOOLCHAIN_BIN_DIR}/*${MCUBOOT_ARCH}*elf-gcc")
    if(NOT C_COMPILER_BIN)
      message(FATAL_ERROR "No C compiler found. Please ensure that TOOLCHAIN_BIN_DIR directory contains a set of C compiling tools compatible with the target")
    endif()
    set(CMAKE_C_COMPILER ${C_COMPILER_BIN})
    set(CMAKE_ASM_COMPILER ${C_COMPILER_BIN})
    message("C compiler found: ${CMAKE_C_COMPILER}")

    file(GLOB CXX_COMPILER_BIN "${TOOLCHAIN_BIN_DIR}/*${MCUBOOT_ARCH}*elf-g++")
    if(NOT CXX_COMPILER_BIN)
      message(FATAL_ERROR "No C++ compiler found. Please ensure that TOOLCHAIN_BIN_DIR directory contains a set of C++ compiling tools compatible with the target")
    endif()
    set(CMAKE_CXX_COMPILER ${CXX_COMPILER_BIN})
    message("CXX compiler found: ${CMAKE_CXX_COMPILER}")
  else()
    # Set toolchain file that expect the same toolchain as IDF sets on PATH
    set(CMAKE_TOOLCHAIN_FILE ${ESPRESSIF_PORT_DIR}/tools/toolchain-${MCUBOOT_TARGET}.cmake)
    message("No user-defined toolchain, setting default toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
  endif()

  # This flag is needed when redefining a different compiler toolchain at this point
  # on CMakeLists, the reason is that CMake does a compiler testing prior to building
  # that may fail due to cross-compilation
  set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
else()
  message("CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
endif()

set(LINKER_SCRIPT_INCLUDES ${ESPRESSIF_PORT_DIR}/port/include/${MCUBOOT_TARGET})

include(common.cmake)

list(APPEND LDFLAGS
    "-Wl,--Map=${APP_NAME}.map"
    )

target_compile_options(
  ${APP_EXECUTABLE}
  PUBLIC
  ${CFLAGS}
  )

target_sources(
  ${APP_EXECUTABLE}
  PUBLIC
  ${bootutil_srcs}
  ${crypto_srcs}
  ${port_srcs}
  )

target_include_directories(
  ${APP_EXECUTABLE}
  PUBLIC
  ${inc_directories}
  )

target_link_libraries(
  ${APP_EXECUTABLE}
  PUBLIC
  -T${ld_output}
  ${LDFLAGS}
  )

add_subdirectory(hal)

target_link_libraries(
  ${APP_EXECUTABLE}
  PUBLIC
  hal
  )

# Find installed esptool, if not found falls to IDF's
find_program(ESPTOOL_COMMAND
  NAMES esptool esptool.py
  )
if("${ESPTOOL_COMMAND}" MATCHES "ESPTOOL_COMMAND-NOTFOUND")
  if(DEFINED ENV{IDF_PATH})
    set(esptool_path "${IDF_PATH}/components/esptool_py/esptool/esptool.py")
  else()
    message(FATAL_ERROR "esptool.py not found. Please install it using \'pip install esptool\'.")
  endif()
else()
  set(esptool_path "${ESPTOOL_COMMAND}")
endif()

# Flash frequency parameter for esptool.py, for more information, check `esptool.py -h`
if(NOT DEFINED ESP_FLASH_FREQ)
  if("${MCUBOOT_TARGET}" STREQUAL "esp32" OR
    "${MCUBOOT_TARGET}" STREQUAL "esp32s2" OR
    "${MCUBOOT_TARGET}" STREQUAL "esp32s3" OR
    "${MCUBOOT_TARGET}" STREQUAL "esp32c3" OR
    "${MCUBOOT_TARGET}" STREQUAL "esp32c6")
    set(ESP_FLASH_FREQ "40m")
  elseif("${MCUBOOT_TARGET}" STREQUAL "esp32c2")
    set(ESP_FLASH_FREQ "60m")
  elseif("${MCUBOOT_TARGET}" STREQUAL "esp32h2")
    set(ESP_FLASH_FREQ "24m")
  endif()
endif()

# Flash mode parameter for esptool.py, for more information, check `esptool.py -h`
if(NOT DEFINED ESP_FLASH_MODE)
  set(ESP_FLASH_MODE "dio")
endif()

# Serial baud rate parameter for esptool.py flash use, for more information, check `esptool.py -h`
if(NOT DEFINED ESP_BAUD_RATE)
  set(ESP_BAUD_RATE 115200)
endif()

# This step uses esptool.py for generating the final bootloader binary in
# Espressif compatible format.
# Note: Both binary generation and flash steps still have some default arguments
add_custom_command(
  TARGET ${APP_EXECUTABLE} POST_BUILD
  COMMAND
  ${esptool_path}
  --chip ${MCUBOOT_TARGET} elf2image --min-rev ${ESP_MIN_REVISION}
  --flash_mode ${ESP_FLASH_MODE} --flash_freq ${ESP_FLASH_FREQ} --flash_size ${CONFIG_ESP_FLASH_SIZE}
  -o ${APP_NAME}.bin ${APP_NAME}.elf
  )

if(DEFINED MCUBOOT_FLASH_PORT)
  set(FLASH_PORT ${MCUBOOT_FLASH_PORT})
else()
  # Defaults to the first USB serial port
  set(FLASH_PORT "/dev/ttyUSB0")
endif()

if(NOT EXISTS "${FLASH_PORT}")
  message(WARNING "Could not open ${FLASH_PORT}, serial port does not exist")
endif()

# Target for flashing the bootloader using esptool.py
add_custom_target(flash DEPENDS ${APP_NAME}.bin)
add_custom_command(
  TARGET flash
  USES_TERMINAL
  COMMAND
  ${esptool_path}
  -p ${FLASH_PORT} -b ${ESP_BAUD_RATE} --before default_reset --after no_reset
  --chip ${MCUBOOT_TARGET} write_flash
  --flash_mode ${ESP_FLASH_MODE} --flash_size ${CONFIG_ESP_FLASH_SIZE}
  --flash_freq ${ESP_FLASH_FREQ} ${CONFIG_ESP_BOOTLOADER_OFFSET}
  ${APP_NAME}.bin
  )
