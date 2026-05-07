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
    ${CMAKE_CURRENT_LIST_DIR}/include/mcuboot_config
    )

list(APPEND include_dirs
    ${esp_hal_dir}/components/esp_common/include
    ${esp_hal_dir}/components/bootloader_support/include
    ${esp_hal_dir}/components/bootloader_support/private_include
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/include
    ${esp_hal_dir}/components/esp_bootloader_format/include
    ${esp_hal_dir}/components/spi_flash/include
    ${esp_hal_dir}/components/spi_flash/include/spi_flash
    ${esp_hal_dir}/components/esp_hal_mspi/include
    ${esp_hal_dir}/components/esp_hal_mspi/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_gpspi/include
    ${esp_hal_dir}/components/esp_hal_gpspi/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_clock/include
    ${esp_hal_dir}/components/esp_hal_clock/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_pmu/include
    ${esp_hal_dir}/components/esp_hal_pmu/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_ana_conv/include
    ${esp_hal_dir}/components/esp_hal_ana_conv/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_security/include
    ${esp_hal_dir}/components/esp_hal_security/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_blockdev/include
    ${esp_hal_dir}/components/esp_blockdev/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_app_format/include
    ${esp_hal_dir}/components/esp_libc/platform_include
    ${esp_hal_dir}/components/${MCUBOOT_ARCH}/include
    ${esp_hal_dir}/components/esp_rom/include
    ${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/include/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/soc/include
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/register
    ${esp_hal_dir}/components/efuse/include
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/efuse/private_include
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/private_include
    ${esp_hal_dir}/components/esp_hw_support/include
    ${esp_hal_dir}/components/esp_hw_support/include/soc
    ${esp_hal_dir}/components/esp_hw_support/include/soc/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/esp_hw_support/port/include
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hw_support/include/esp_private
    ${esp_hal_dir}/components/esp_hw_support/mspi_timing_tuning/include
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}
    ${esp_hal_dir}/components/hal/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/hal/include
    ${esp_hal_dir}/components/esp_hal_rtc_timer/include
    ${esp_hal_dir}/components/esp_hal_rtc_timer/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_timg/include
    ${esp_hal_dir}/components/esp_hal_timg/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_gpio/include
    ${esp_hal_dir}/components/esp_hal_gpio/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_wdt/include
    ${esp_hal_dir}/components/esp_hal_wdt/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/esp_hal_uart/include
    ${esp_hal_dir}/components/esp_hal_uart/${MCUBOOT_TARGET}/include
    ${esp_hal_dir}/components/hal/platform_port/include
    ${esp_hal_dir}/components/esp_system/include
    ${esp_hal_dir}/components/log/include
    )

if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
    list(APPEND include_dirs
        ${esp_hal_dir}/components/${MCUBOOT_ARCH}/${MCUBOOT_TARGET}/include
        ${esp_hal_dir}/components/${MCUBOOT_ARCH}/include
        )
elseif("${MCUBOOT_ARCH}" STREQUAL "riscv")
    list(APPEND include_dirs
        ${esp_hal_dir}/components/riscv/include
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
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_sha.c
    ${esp_hal_dir}/components/hal/efuse_hal.c
    ${esp_hal_dir}/components/hal/mmu_hal.c
    ${esp_hal_dir}/components/esp_hal_wdt/wdt_hal_iram.c
    ${esp_hal_dir}/components/hal/${MCUBOOT_TARGET}/efuse_hal.c
    ${esp_hal_dir}/components/esp_hal_uart/${MCUBOOT_TARGET}/uart_periph.c
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/gpio_periph.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_time.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_clk.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_clk_init.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/cpu_region_protect.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_serial_output.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_sys.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_spiflash.c
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/esp_efuse_table.c
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/esp_efuse_fields.c
    ${esp_hal_dir}/components/efuse/src/esp_efuse_fields.c
    ${esp_hal_dir}/components/efuse/src/esp_efuse_api.c
    ${esp_hal_dir}/components/efuse/src/esp_efuse_utility.c
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/esp_efuse_utility.c
    ${esp_hal_dir}/components/log/src/noos/log_timestamp.c
    ${src_dir}/bootloader_banner.c
    ${src_dir}/bootloader_wdt.c
    )

set_source_files_properties(
    ${esp_hal_dir}/components/efuse/src/esp_efuse_utility.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-unused-variable"
    )

set_source_files_properties(
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_utility.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-logical-op"
    )

if(DEFINED CONFIG_SECURE_BOOT_V2_ENABLED OR DEFINED CONFIG_SECURE_FLASH_ENC_ENABLED)
    list(APPEND hal_srcs
        ${esp_hal_dir}/components/esp_security/src/esp_crypto_lock.c
        ${esp_hal_dir}/components/esp_security/src/esp_crypto_periph_clk.c
        )
    list(APPEND include_dirs
        ${esp_hal_dir}/components/esp_security/include
        )
    if(EXISTS "${esp_hal_dir}/components/esp_hal_security/${MCUBOOT_TARGET}/include")
        list(APPEND include_dirs "${esp_hal_dir}/components/esp_hal_security/${MCUBOOT_TARGET}/include")
    endif()
    list(APPEND include_dirs "${esp_hal_dir}/components/esp_hal_security/include")

    if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
        list(APPEND hal_srcs
            ${esp_hal_dir}/components/esp_hal_security/mpu_hal.c
        )
    endif()

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
endif()

if("${MCUBOOT_ARCH}" STREQUAL "xtensa")
    list(APPEND hal_srcs
        ${esp_hal_dir}/components/esp_rom/patches/esp_rom_longjmp.S
        )
elseif("${MCUBOOT_ARCH}" STREQUAL "riscv")
    list(APPEND hal_srcs ${esp_hal_dir}/components/riscv/rv_utils.c)
endif()

set(CFLAGS
    "-specs=picolibc.specs"
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
    "-std=gnu23"
    "-Wno-old-style-declaration"
    "-Wno-implicit-int"
    "-Wno-pointer-to-int-cast"
    "-Wno-int-to-pointer-cast"
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
        "-fno-builtin-memcpy"
        "-fno-builtin-memset"
        "-fno-builtin-bzero"
        )
    list(APPEND LDFLAGS
        "-mlongcalls"
        "-fno-builtin-memcpy"
        "-fno-builtin-memset"
        "-fno-builtin-bzero"
        )
endif()

set(LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.libgcc.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.api.ld
    -T${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.peripherals.ld
    )

if(NOT "${MCUBOOT_ARCH}" STREQUAL "xtensa")
    list(APPEND LINKER_SCRIPTS
        -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.libc-suboptimal_for_misaligned_mem.ld
        -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.libc.ld
        )
endif()

include(${CMAKE_CURRENT_LIST_DIR}/include/${MCUBOOT_TARGET}/${MCUBOOT_TARGET}.cmake)

add_library(hal STATIC ${hal_srcs})

# Wrap for overriding the print banner function from bootloader_support
add_definitions(-DIDF_VER=0)
target_link_libraries(
    hal
    INTERFACE
    "-Wl,--wrap=bootloader_print_banner")

target_precompile_headers(
    hal
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include/esp_log.h
    ${esp_hal_dir}/components/esp_common/include/esp_attr.h
    ${esp_hal_dir}/components/esp_rom/include/esp_rom_sys.h
    )

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
