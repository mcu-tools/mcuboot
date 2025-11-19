# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

add_definitions(-DMCUBOOT_TARGET=${MCUBOOT_TARGET})
add_definitions(-D__ESPRESSIF__=1)

# Set directories
set(BOOTUTIL_DIR ${MCUBOOT_ROOT_DIR}/boot/bootutil)
set(BOOT_SERIAL_DIR ${MCUBOOT_ROOT_DIR}/boot/boot_serial)
set(ZCBOR_DIR ${MCUBOOT_ROOT_DIR}/boot/zcbor)

# Set chip arch
if("${MCUBOOT_TARGET}" STREQUAL "esp32" OR
  "${MCUBOOT_TARGET}" STREQUAL "esp32s2" OR
  "${MCUBOOT_TARGET}" STREQUAL "esp32s3")
  set(MCUBOOT_ARCH "xtensa")
elseif("${MCUBOOT_TARGET}" STREQUAL "esp32c3" OR
  "${MCUBOOT_TARGET}" STREQUAL "esp32c6" OR
  "${MCUBOOT_TARGET}" STREQUAL "esp32c2" OR
  "${MCUBOOT_TARGET}" STREQUAL "esp32h2")
  set(MCUBOOT_ARCH "riscv")
endif()

# Set the minimum revision for each supported chip
if("${MCUBOOT_TARGET}" STREQUAL "esp32")
  set(ESP_MIN_REVISION 3)
elseif("${MCUBOOT_TARGET}" STREQUAL "esp32s2")
  set(ESP_MIN_REVISION 0)
elseif("${MCUBOOT_TARGET}" STREQUAL "esp32s3")
  set(ESP_MIN_REVISION 0)
elseif("${MCUBOOT_TARGET}" STREQUAL "esp32c3")
  set(ESP_MIN_REVISION 3)
elseif("${MCUBOOT_TARGET}" STREQUAL "esp32c6")
  set(ESP_MIN_REVISION 0)
elseif("${MCUBOOT_TARGET}" STREQUAL "esp32c2")
  set(ESP_MIN_REVISION 0)
elseif("${MCUBOOT_TARGET}" STREQUAL "esp32h2")
  set(ESP_MIN_REVISION 0)
else()
  message(FATAL_ERROR "Unsupported target ${MCUBOOT_TARGET}")
endif()

# Get MCUboot revision to be added into bootloader image version
execute_process(
  COMMAND git describe --tags
  WORKING_DIRECTORY ${ESPRESSIF_PORT_DIR}
  OUTPUT_VARIABLE MCUBOOT_VER
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
add_definitions(-DMCUBOOT_VER=\"${MCUBOOT_VER}\")

# Set Espressif HAL to use
if(NOT DEFINED ESP_HAL_PATH)
  if(DEFINED ENV{ESP_HAL_PATH})
    set(ESP_HAL_PATH $ENV{ESP_HAL_PATH})
  else()
    message(WARNING "ESP_HAL_PATH not defined, checking if IDF_PATH exists.")
    if(DEFINED ENV{IDF_PATH})
      set(ESP_HAL_PATH $ENV{IDF_PATH})
      message("IDF installation found in the system, using IDF_PATH as ESP_HAL_PATH.")
    else ()
      message(FATAL_ERROR "Please set -DESP_HAL_PATH parameter or define ESP_HAL_PATH environment variable.")
    endif()
  endif()
endif()
message(STATUS "Defined ESP_HAL_PATH: ${ESP_HAL_PATH}")

# Verify from which IDF version the HAL is based on
set(IDF_VER_HEADER_FILE "${ESP_HAL_PATH}/components/esp_common/include/esp_idf_version.h")

get_version_from_header("ESP_IDF_VERSION_MAJOR" ${IDF_VER_HEADER_FILE} IDF_VERSION_MAJOR)
get_version_from_header("ESP_IDF_VERSION_MINOR" ${IDF_VER_HEADER_FILE} IDF_VERSION_MINOR)
get_version_from_header("ESP_IDF_VERSION_PATCH" ${IDF_VER_HEADER_FILE} IDF_VERSION_PATCH)

set(IDF_VERSION "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")

if(NOT IDF_VERSION VERSION_EQUAL ${EXPECTED_IDF_HAL_VERSION})
  message(
    FATAL_ERROR "
    Unsupported HAL version ${IDF_VERSION}, expected ${EXPECTED_IDF_HAL_VERSION}. \
    Verify if the RTOS repository, where you are trying to build from, is up to date, \
    or check the installation pointed on ESP_HAL_PATH."
    )
else ()
    message(STATUS "HAL based on ESP-IDF version: ${IDF_VERSION}")
endif()

# Find imgtool.
# Go with an explicitly installed imgtool first, falling
# back to mcuboot/scripts/imgtool.py.
find_program(IMGTOOL_COMMAND
  NAMES imgtool imgtool.py
  )
if("${IMGTOOL_COMMAND}" MATCHES "IMGTOOL_COMMAND-NOTFOUND")
  set(imgtool_path "${MCUBOOT_ROOT_DIR}/scripts/imgtool.py")
else()
  set(imgtool_path "${IMGTOOL_COMMAND}")
endif()

list(APPEND inc_directories
    ${BOOTUTIL_DIR}/include
    ${BOOTUTIL_DIR}/src
    ${ESPRESSIF_PORT_DIR}/include
    )

set(main_src ${ESPRESSIF_PORT_DIR}/main.c)

set(port_srcs
    ${ESPRESSIF_PORT_DIR}/port/esp_mcuboot.c
    ${ESPRESSIF_PORT_DIR}/port/esp_loader.c
    ${ESPRESSIF_PORT_DIR}/os.c
    )

set(bootutil_srcs
    ${BOOTUTIL_DIR}/src/boot_record.c
    ${BOOTUTIL_DIR}/src/bootutil_find_key.c
    ${BOOTUTIL_DIR}/src/bootutil_img_hash.c
    ${BOOTUTIL_DIR}/src/bootutil_img_security_cnt.c
    ${BOOTUTIL_DIR}/src/bootutil_misc.c
    ${BOOTUTIL_DIR}/src/bootutil_area.c
    ${BOOTUTIL_DIR}/src/bootutil_loader.c
    ${BOOTUTIL_DIR}/src/bootutil_public.c
    ${BOOTUTIL_DIR}/src/caps.c
    ${BOOTUTIL_DIR}/src/encrypted.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening_delay_rng_mbedtls.c
    ${BOOTUTIL_DIR}/src/image_ecdsa.c
    ${BOOTUTIL_DIR}/src/image_ed25519.c
    ${BOOTUTIL_DIR}/src/image_rsa.c
    ${BOOTUTIL_DIR}/src/image_validate.c
    ${BOOTUTIL_DIR}/src/loader.c
    ${BOOTUTIL_DIR}/src/swap_misc.c
    ${BOOTUTIL_DIR}/src/swap_move.c
    ${BOOTUTIL_DIR}/src/swap_scratch.c
    ${BOOTUTIL_DIR}/src/tlv.c
    )

if(CONFIG_BOOT_RAM_LOAD)
  list(APPEND bootutil_srcs ${BOOTUTIL_DIR}/src/ram_load.c)
endif()

if(DEFINED CONFIG_ESP_SIGN_RSA)
  include(${ESPRESSIF_PORT_DIR}/include/crypto_config/rsa.cmake)
elseif(DEFINED CONFIG_ESP_SIGN_EC256)
  include(${ESPRESSIF_PORT_DIR}/include/crypto_config/ec256.cmake)
elseif(DEFINED CONFIG_ESP_SIGN_ED25519)
  include(${ESPRESSIF_PORT_DIR}/include/crypto_config/ed25519.cmake)
else()
  # No signature verification
  set(TINYCRYPT_DIR ${MCUBOOT_ROOT_DIR}/ext/tinycrypt/lib)
  set(CRYPTO_INC
      ${TINYCRYPT_DIR}/include
      )
  set(crypto_srcs
      ${TINYCRYPT_DIR}/source/sha256.c
      ${TINYCRYPT_DIR}/source/utils.c
      )
endif()

if(DEFINED CONFIG_ESP_SIGN_KEY_FILE)
  if(IS_ABSOLUTE ${CONFIG_ESP_SIGN_KEY_FILE})
    set(KEY_FILE ${CONFIG_ESP_SIGN_KEY_FILE})
  else()
    set(KEY_FILE ${MCUBOOT_ROOT_DIR}/${CONFIG_ESP_SIGN_KEY_FILE})
  endif()
  message("MCUBoot bootloader key file: ${KEY_FILE}")

  set(GENERATED_PUBKEY ${CMAKE_CURRENT_BINARY_DIR}/autogen-pubkey.c)
  add_custom_command(
    OUTPUT ${GENERATED_PUBKEY}
    COMMAND
    ${imgtool_path}
    getpub
    -k
    ${KEY_FILE}
    > ${GENERATED_PUBKEY}
    DEPENDS ${KEY_FILE}
  )
  list(APPEND crypto_srcs ${GENERATED_PUBKEY})
endif()

if(CONFIG_ESP_MCUBOOT_SERIAL)
  set(MBEDTLS_DIR "${MCUBOOT_ROOT_DIR}/ext/mbedtls")

  list(APPEND bootutil_srcs
      ${BOOT_SERIAL_DIR}/src/boot_serial.c
      ${BOOT_SERIAL_DIR}/src/zcbor_bulk.c
      ${ZCBOR_DIR}/src/zcbor_decode.c
      ${ZCBOR_DIR}/src/zcbor_encode.c
      ${ZCBOR_DIR}/src/zcbor_common.c
      )
  list(APPEND inc_directories
      ${BOOT_SERIAL_DIR}/include
      ${ZCBOR_DIR}/include
      )
  list(APPEND port_srcs
      ${ESPRESSIF_PORT_DIR}/port/serial_adapter.c
      ${MBEDTLS_DIR}/library/base64.c
      ${MBEDTLS_DIR}/library/constant_time.c
      )
  list(APPEND CRYPTO_INC
      ${MBEDTLS_DIR}/include
      )
endif()

list(APPEND inc_directories
    ${CRYPTO_INC}
    )

set(CFLAGS
    "-Wno-frame-address"
    "-Wall"
    "-Wextra"
    "-W"
    "-Wdeclaration-after-statement"
    "-Wwrite-strings"
    "-Wlogical-op"
    "-Wshadow"
    "-ffunction-sections"
    "-fdata-sections"
    "-fstrict-volatile-bitfields"
    "-fdump-rtl-expand"
    "-Werror=all"
    "-Wno-error=unused-function"
    "-Wno-error=unused-but-set-variable"
    "-Wno-error=unused-variable"
    "-Wno-error=deprecated-declarations"
    "-Wno-unused-parameter"
    "-Wno-sign-compare"
    "-ggdb"
    "-Os"
    "-D_GNU_SOURCE"
    "-std=gnu17"
    "-Wno-old-style-declaration"
    "-Wno-implicit-int"
    "-Wno-declaration-after-statement"
    )

set(LDFLAGS
    "-nostdlib"
    "-Wno-frame-address"
    "-Wl,--cref"
    "-Wl,--print-memory-usage"
    "-fno-rtti"
    "-fno-lto"
    "-Wl,--gc-sections"
    "-Wl,--undefined=uxTopUsedPriority"
    "-lm"
    "-lgcc"
    "-lgcov"
    )

if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
  list(APPEND CFLAGS
      "-mlongcalls"
      )
  list(APPEND LDFLAGS
      "-mlongcalls"
      )
endif()

# Set linker script
set(ld_input ${ESPRESSIF_PORT_DIR}/port/${MCUBOOT_TARGET}/ld/bootloader.ld)
set(ld_output ${CMAKE_CURRENT_BINARY_DIR}/ld/bootloader.ld)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ld")

get_directory_property(configs COMPILE_DEFINITIONS)
foreach(c ${configs})
  list(APPEND conf_defines "-D${c}")
endforeach()

# Preprocess linker script
add_custom_command(
  TARGET ${APP_EXECUTABLE} PRE_LINK
  COMMAND ${CMAKE_C_COMPILER} -x c -E -P
  -o ${ld_output}
  -I${LINKER_SCRIPT_INCLUDES}
  ${conf_defines}
  ${ld_input}
  MAIN_DEPENDENCY ${ld_input}
  COMMENT "Preprocessing bootloader.ld linker script..."
  )
