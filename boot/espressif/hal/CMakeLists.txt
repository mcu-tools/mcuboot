# SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13)

project(hal)

set(esp_hal_dir ${ESP_HAL_PATH})
set(src_dir ${CMAKE_CURRENT_LIST_DIR}/src)
set(include_dirs
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/include/${MCUBOOT_TARGET}
    )

list(APPEND include_dirs
    ${esp_hal_dir}/components/bootloader_support/include
    ${esp_hal_dir}/components/bootloader_support/private_include
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/include
    ${esp_hal_dir}/components/spi_flash/include
    ${esp_hal_dir}/components/spi_flash/include/spi_flash
    ${esp_hal_dir}/components/esp_app_format/include
    ${esp_hal_dir}/components/newlib/platform_include
    ${esp_hal_dir}/components/esp_common/include
    ${esp_hal_dir}/components/${MCUBOOT_ARCH}/include
    ${esp_hal_dir}/components/esp_rom/include
    ${esp_hal_dir}/components/esp_rom/include/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/soc/include
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/efuse/include
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/efuse/private_include
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/private_include
    ${esp_hal_dir}/components/esp_hw_support/include
    ${esp_hal_dir}/components/esp_hw_support/include/soc
    ${esp_hal_dir}/components/esp_hw_support/include/soc/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/esp_hw_support/port/include
    ${esp_hal_dir}/components/esp_hw_support/include/esp_private
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/hal/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/hal/include
    ${esp_hal_dir}/components/hal/platform_port/include
    ${esp_hal_dir}/components/esp_system/include
    ${esp_hal_dir}/components/log/include
    )

if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
    list(APPEND include_dirs
        ${esp_hal_dir}/components/${MCUBOOT_ARCH}/${MCUBOOT_TARGET}/include
        ${esp_hal_dir}/components/${MCUBOOT_ARCH}/include
        )
endif()

set(hal_srcs
    ${esp_hal_dir}/components/bootloader_support/src/${MCUBOOT_TARGET}/bootloader_${MCUBOOT_TARGET}.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_init.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_common.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_common_loader.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_console.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_console_loader.c
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/src/bootloader_flash.c
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/src/bootloader_flash_config_${MCUBOOT_TARGET}.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_clock_init.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_clock_loader.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_efuse.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_panic.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_mem.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_random.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_random_${MCUBOOT_TARGET}.c
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_utility.c
    ${esp_hal_dir}/components/bootloader_support/src/esp_image_format.c
    ${esp_hal_dir}/components/bootloader_support/src/${MCUBOOT_TARGET}/bootloader_soc.c
    ${esp_hal_dir}/components/bootloader_support/src/${MCUBOOT_TARGET}/bootloader_sha.c
    ${esp_hal_dir}/components/hal/mpu_hal.c
    ${esp_hal_dir}/components/hal/efuse_hal.c
    ${esp_hal_dir}/components/hal/mmu_hal.c
    ${esp_hal_dir}/components/hal/wdt_hal_iram.c
    ${esp_hal_dir}/components/hal/${MCUBOOT_TARGET}/efuse_hal.c
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/uart_periph.c
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/gpio_periph.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_time.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_clk.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_clk_init.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_uart.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_sys.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_spiflash.c
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/esp_efuse_table.c
    ${esp_hal_dir}/components/efuse/src/esp_efuse_fields.c
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/esp_efuse_fields.c
    ${esp_hal_dir}/components/efuse/src/esp_efuse_api.c
    ${esp_hal_dir}/components/efuse/src/esp_efuse_utility.c
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/esp_efuse_utility.c
    ${esp_hal_dir}/components/log/log_noos.c
    ${src_dir}/bootloader_banner.c
    ${src_dir}/bootloader_wdt.c
    )

if(DEFINED CONFIG_SECURE_BOOT_V2_ENABLED)
    list(APPEND hal_srcs
        ${src_dir}/secure_boot.c
        ${esp_hal_dir}/components/bootloader_support/src/secure_boot_v2/secure_boot_signatures_bootloader.c
        ${esp_hal_dir}/components/bootloader_support/src/${MCUBOOT_TARGET}/secure_boot_secure_features.c
        )
    list(APPEND include_dirs
        ${esp_hal_dir}/components/bootloader_support/src/secure_boot_v2
        )
endif()

if(DEFINED CONFIG_SECURE_FLASH_ENC_ENABLED)
    list(APPEND hal_srcs
        ${src_dir}/flash_encrypt.c
        ${esp_hal_dir}/components/bootloader_support/src/${MCUBOOT_TARGET}/flash_encryption_secure_features.c
        )
    set_source_files_properties(
        ${src_dir}/flash_encrypt.c
        PROPERTIES COMPILE_FLAGS
        "-Wno-unused-variable"
        )
endif()

if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
    list(APPEND hal_srcs
        ${esp_hal_dir}/components/esp_rom/patches/esp_rom_longjmp.S
        )
endif()

set(CFLAGS
    "-nostdlib"
    "-Wno-frame-address"
    "-Wall"
    "-Wextra"
    "-W"
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
    )

set(LDFLAGS
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

if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
    list(APPEND CFLAGS
        "-mlongcalls"
        )
    list(APPEND LDFLAGS
        "-mlongcalls"
        )
endif()

set(LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.libgcc.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.api.ld
    -T${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.peripherals.ld
    )

include(${CMAKE_CURRENT_LIST_DIR}/include/${MCUBOOT_TARGET}/${MCUBOOT_TARGET}.cmake)

add_library(hal STATIC ${hal_srcs} ${include_dirs})

# Wrap for overriding the print banner function from bootloader_support
add_definitions(-DIDF_VER=0)
target_link_libraries(
    hal
    INTERFACE
    "-Wl,--wrap=bootloader_print_banner")

target_include_directories(
    hal
    PUBLIC
    ${include_dirs}
    )

target_compile_options(
    hal
    PUBLIC
    ${CFLAGS}
    )

target_link_libraries(
    hal
    PUBLIC
    ${LDFLAGS}
    ${LINKER_SCRIPTS}
    )
