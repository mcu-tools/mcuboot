# Copyright (c) 2023 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)


set(CONFIG_TINYCRYPT 1)
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
set(MCUBOOT_ROOT_DIR ${PROJECT_ROOT_DIR}/../..)
set(BOOTUTIL_DIR ${MCUBOOT_ROOT_DIR}/boot/bootutil)
set(BOOT_SERIAL_DIR ${MCUBOOT_ROOT_DIR}/boot/boot_serial)
set(HPMICRO_PORT_DIR ${PROJECT_ROOT_DIR}/port)

set(CUSTOM_GCC_LINKER_FILE ${HPMICRO_PORT_DIR}/${BOARD}/swap.ld)
find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

include(${PROJECT_ROOT_DIR}/tools/utils.cmake)

if (NOT DEFINED MCUBOOT_CONFIG_FILE)
    set(MCUBOOT_CONFIG_FILE "${HPMICRO_PORT_DIR}/${BOARD}/bootloader.conf")
endif()

string(REPLACE " " ";" MCUBOOT_CONFIG_FILE_LIST "${MCUBOOT_CONFIG_FILE}")
foreach(CONFIG_FILE ${MCUBOOT_CONFIG_FILE_LIST})
    if (NOT EXISTS "${CONFIG_FILE}")
        message(FATAL_ERROR "MCUboot configuration file does not exist at ${CONFIG_FILE}")
    endif()
    message("parse config file" ${CONFIG_FILE})
    parse_and_set_config_file(${CONFIG_FILE})
endforeach()

project(mcuboot)

if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "")
    string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)
    string(FIND ${build_type} "upgrade" found)
    if (${found} GREATER_EQUAL 0)
        sdk_compile_definitions(-DMCUBOOT_APP_UPGRADE_MODE=1)
    endif()
endif()

sdk_compile_definitions(-DBOARD_SHOW_CLOCK=0)
sdk_compile_definitions(-DBUILD_TYPE_MCUBOOTAPP=1)
sdk_compile_definitions("-DINIT_EXT_RAM_FOR_DATA=1")

sdk_app_inc(${CMAKE_CURRENT_LIST_DIR}/../hal/include)
sdk_app_inc(${BOOTUTIL_DIR}/include)
sdk_app_inc(${PROJECT_ROOT_DIR}/include)

#add_subdirectory(port)

set(bootutil_srcs
    ${BOOTUTIL_DIR}/src/boot_record.c
    ${BOOTUTIL_DIR}/src/bootutil_misc.c
    ${BOOTUTIL_DIR}/src/bootutil_public.c
    ${BOOTUTIL_DIR}/src/caps.c
    ${BOOTUTIL_DIR}/src/encrypted.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening_delay_rng_mbedtls.c
    ${BOOTUTIL_DIR}/src/image_ec.c
    ${BOOTUTIL_DIR}/src/image_ec256.c
    ${BOOTUTIL_DIR}/src/image_ed25519.c
    ${BOOTUTIL_DIR}/src/image_rsa.c
    ${BOOTUTIL_DIR}/src/image_validate.c
    ${BOOTUTIL_DIR}/src/loader.c
    ${BOOTUTIL_DIR}/src/swap_misc.c
    ${BOOTUTIL_DIR}/src/swap_move.c
    ${BOOTUTIL_DIR}/src/swap_scratch.c
    ${BOOTUTIL_DIR}/src/tlv.c
    )
set(port_srcs
    ${PROJECT_ROOT_DIR}/port/flash_hpmicro.c
    ${PROJECT_ROOT_DIR}/port/flash_map_extended.c
    ${PROJECT_ROOT_DIR}/port/flash_map_layout.c
    ${PROJECT_ROOT_DIR}/port/flash_page_layout.c
    ${PROJECT_ROOT_DIR}/port/hpm_flash_areas.c
    ${PROJECT_ROOT_DIR}/port/hpm_bootutil_ex.c
    ${PROJECT_ROOT_DIR}/os.c
    )

sdk_app_src(${bootutil_srcs})
sdk_app_src(${port_srcs})

sdk_app_src(hello_world.c)
generate_ses_project()