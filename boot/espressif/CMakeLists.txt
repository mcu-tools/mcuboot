# SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0109 NEW)

include(${CMAKE_CURRENT_LIST_DIR}/tools/utils.cmake)

if (NOT DEFINED MCUBOOT_TARGET)
    message(FATAL_ERROR "MCUBOOT_TARGET not defined. Please pass -DMCUBOOT_TARGET flag.")
endif()

project(mcuboot_${MCUBOOT_TARGET})

add_definitions(-DMCUBOOT_TARGET=${MCUBOOT_TARGET})
add_definitions(-D__ESPRESSIF__=1)

if ("${MCUBOOT_TARGET}" STREQUAL "esp32" OR
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
if ("${MCUBOOT_TARGET}" STREQUAL "esp32")
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

if (NOT DEFINED ESP_HAL_PATH)
    if (DEFINED ENV{ESP_HAL_PATH})
        set(ESP_HAL_PATH $ENV{ESP_HAL_PATH})
    else()
        message(WARNING "ESP_HAL_PATH not found. Please set -DESP_HAL_PATH parameter or define ESP_HAL_PATH environment variable.")
        if (DEFINED ENV{IDF_PATH})
            set(ESP_HAL_PATH $ENV{IDF_PATH})
            message("IDF installation found in the system, using IDF_PATH as ESP_HAL_PATH.")
        endif()
    endif()
endif()

execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE MCUBOOT_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
add_definitions(-DMCUBOOT_VER=\"${MCUBOOT_VER}\")

if (NOT DEFINED MCUBOOT_CONFIG_FILE)
    set(MCUBOOT_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/port/${MCUBOOT_TARGET}/bootloader.conf")
endif()

string(REPLACE " " ";" MCUBOOT_CONFIG_FILE_LIST "${MCUBOOT_CONFIG_FILE}")
foreach(CONFIG_FILE ${MCUBOOT_CONFIG_FILE_LIST})
    if (NOT EXISTS "${CONFIG_FILE}")
        message(FATAL_ERROR "MCUboot configuration file does not exist at ${CONFIG_FILE}")
    endif()
    parse_and_set_config_file(${CONFIG_FILE})
endforeach()

set(APP_NAME mcuboot_${MCUBOOT_TARGET})
set(APP_EXECUTABLE ${APP_NAME}.elf)

set(MCUBOOT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(BOOTUTIL_DIR ${MCUBOOT_ROOT_DIR}/boot/bootutil)
set(BOOT_SERIAL_DIR ${MCUBOOT_ROOT_DIR}/boot/boot_serial)
set(ZCBOR_DIR ${MCUBOOT_ROOT_DIR}/boot/zcbor)
set(ESPRESSIF_PORT_DIR ${CMAKE_CURRENT_LIST_DIR})

# Find imgtool.
# Go with an explicitly installed imgtool first, falling
# back to mcuboot/scripts/imgtool.py.
find_program(IMGTOOL_COMMAND
    NAMES imgtool imgtool.py
    )
if ("${IMGTOOL_COMMAND}" MATCHES "IMGTOOL_COMMAND-NOTFOUND")
    set(imgtool_path "${MCUBOOT_ROOT_DIR}/scripts/imgtool.py")
else()
    set(imgtool_path "${IMGTOOL_COMMAND}")
endif()

# Find installed esptool, if not found falls to IDF's
find_program(ESPTOOL_COMMAND
    NAMES esptool esptool.py
    )
if ("${ESPTOOL_COMMAND}" MATCHES "ESPTOOL_COMMAND-NOTFOUND")
    if (DEFINED ENV{IDF_PATH})
        set(esptool_path "${IDF_PATH}/components/esptool_py/esptool/esptool.py")
    else()
        message(FATAL_ERROR "esptool.py not found. Please install it using \'pip install esptool\'.")
    endif()
else()
    set(esptool_path "${ESPTOOL_COMMAND}")
endif()

# Flash frequency parameter for esptool.py, for more information, check `esptool.py -h`
if (NOT DEFINED ESP_FLASH_FREQ)
    if ("${MCUBOOT_TARGET}" STREQUAL "esp32" OR
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
if (NOT DEFINED ESP_FLASH_MODE)
    set(ESP_FLASH_MODE "dio")
endif()

# Serial baud rate parameter for esptool.py flash use, for more information, check `esptool.py -h`
if (NOT DEFINED ESP_BAUD_RATE)
    set(ESP_BAUD_RATE 115200)
endif()

if (DEFINED CONFIG_ESP_SIGN_RSA)
    include(${CMAKE_CURRENT_LIST_DIR}/include/crypto_config/rsa.cmake)
elseif (DEFINED CONFIG_ESP_SIGN_EC256)
    include(${CMAKE_CURRENT_LIST_DIR}/include/crypto_config/ec256.cmake)
elseif (DEFINED CONFIG_ESP_SIGN_ED25519)
    include(${CMAKE_CURRENT_LIST_DIR}/include/crypto_config/ed25519.cmake)
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

set(bootutil_srcs
    ${BOOTUTIL_DIR}/src/boot_record.c
    ${BOOTUTIL_DIR}/src/bootutil_misc.c
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
set(bootutil_paths)

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
    "-Wl,--Map=${APP_NAME}.map"
    "-fno-rtti"
    "-fno-lto"
    "-Wl,--gc-sections"
    "-Wl,--undefined=uxTopUsedPriority"
    "-lm"
    "-lgcc"
    "-lgcov"
    )

if ("${MCUBOOT_ARCH}" STREQUAL "xtensa")
    list(APPEND CFLAGS
        "-mlongcalls"
        )
    list(APPEND LDFLAGS
        "-mlongcalls"
        )
endif()

add_subdirectory(hal)
add_executable(
    ${APP_EXECUTABLE}
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    )

target_compile_options(
    ${APP_EXECUTABLE}
    PUBLIC
    ${CFLAGS}
    )

set(port_srcs
    ${CMAKE_CURRENT_LIST_DIR}/port/esp_mcuboot.c
    ${CMAKE_CURRENT_LIST_DIR}/port/esp_loader.c
    ${CMAKE_CURRENT_LIST_DIR}/os.c
    )

if(CONFIG_ESP_MCUBOOT_SERIAL)
    set(MBEDTLS_DIR "${MCUBOOT_ROOT_DIR}/ext/mbedtls")

    list(APPEND bootutil_srcs
        ${BOOT_SERIAL_DIR}/src/boot_serial.c
        ${BOOT_SERIAL_DIR}/src/zcbor_bulk.c
        ${ZCBOR_DIR}/src/zcbor_decode.c
        ${ZCBOR_DIR}/src/zcbor_encode.c
        ${ZCBOR_DIR}/src/zcbor_common.c
        )
    list(APPEND bootutil_paths
        ${ZCBOR_DIR}/include
        )
    list(APPEND port_srcs
        ${CMAKE_CURRENT_LIST_DIR}/port/${MCUBOOT_TARGET}/serial_adapter.c
        ${MBEDTLS_DIR}/library/base64.c
        )
    list(APPEND CRYPTO_INC
        ${MBEDTLS_DIR}/include
        )
endif()

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
    ${BOOTUTIL_DIR}/include
    ${BOOTUTIL_DIR}/src
    ${BOOT_SERIAL_DIR}/include
    ${CRYPTO_INC}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${bootutil_paths}
    )

set(ld_input ${CMAKE_CURRENT_LIST_DIR}/port/${MCUBOOT_TARGET}/ld/bootloader.ld)
set(ld_output ${CMAKE_CURRENT_BINARY_DIR}/ld/bootloader.ld)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ld")

get_directory_property(configs COMPILE_DEFINITIONS)
foreach(c ${configs})
    list(APPEND conf_defines "-D${c}")
endforeach()

# Preprocess linker script
add_custom_command(
    TARGET ${APP_EXECUTABLE} PRE_LINK
    COMMAND ${CMAKE_C_COMPILER} -x c -E -P -o ${ld_output} ${conf_defines} ${ld_input}
    MAIN_DEPENDENCY ${ld_input}
    COMMENT "Preprocessing bootloader.ld linker script..."
    )

target_link_libraries(
    ${APP_EXECUTABLE}
    PUBLIC
    -T${ld_output}
    ${LDFLAGS}
    )

target_link_libraries(
    ${APP_EXECUTABLE}
    PUBLIC
    hal
    )

# This step uses esptool.py for generating the final bootloader binary in
# Espressif compatible format.
# Note: Both binary generation and flash steps still have some default arguments
add_custom_command(TARGET ${APP_EXECUTABLE} POST_BUILD
    COMMAND
    ${esptool_path}
    --chip ${MCUBOOT_TARGET} elf2image --min-rev ${ESP_MIN_REVISION}
    --flash_mode ${ESP_FLASH_MODE} --flash_freq ${ESP_FLASH_FREQ} --flash_size ${CONFIG_ESP_FLASH_SIZE}
    -o ${APP_NAME}.bin ${APP_NAME}.elf
    )

if (DEFINED MCUBOOT_FLASH_PORT)
    set(FLASH_PORT ${MCUBOOT_FLASH_PORT})
else()
    # Defaults to the first USB serial port
    set(FLASH_PORT "/dev/ttyUSB0")
endif()

if (NOT EXISTS "${FLASH_PORT}")
    message(WARNING "Could not open ${FLASH_PORT}, serial port does not exist")
endif()

add_custom_target(flash DEPENDS ${APP_NAME}.bin)
add_custom_command(TARGET flash
    USES_TERMINAL
    COMMAND
    ${esptool_path}
    -p ${FLASH_PORT} -b ${ESP_BAUD_RATE} --before default_reset --after no_reset
    --chip ${MCUBOOT_TARGET} write_flash
    --flash_mode ${ESP_FLASH_MODE} --flash_size ${CONFIG_ESP_FLASH_SIZE}
    --flash_freq ${ESP_FLASH_FREQ} ${CONFIG_ESP_BOOTLOADER_OFFSET}
    ${APP_NAME}.bin
    )
