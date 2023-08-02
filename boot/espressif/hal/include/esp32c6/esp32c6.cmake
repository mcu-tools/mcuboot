# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND include_dirs
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/private_include
)

list(APPEND hal_srcs
    ${esp_hal_dir}/components/hal/cache_hal.c
    ${esp_hal_dir}/components/hal/${MCUBOOT_TARGET}/lp_timer_hal.c
    ${esp_hal_dir}/components/efuse/src/efuse_controller/keys/with_key_purposes/esp_efuse_api_key.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_regi2c_${MCUBOOT_TARGET}.c
)

if (DEFINED CONFIG_ESP_CONSOLE_UART_CUSTOM)
    list(APPEND hal_srcs
        ${src_dir}/${MCUBOOT_TARGET}/console_uart_custom.c
        )
endif()

list(APPEND LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib.ld
)

set_source_files_properties(
    ${esp_hal_dir}/components/bootloader_support/src/esp_image_format.c
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/src/bootloader_flash.c
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/src/bootloader_flash_config_${MCUBOOT_TARGET}.c
    ${esp_hal_dir}/components/hal/mmu_hal.c
    ${esp_hal_dir}/components/hal/cache_hal.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-logical-op")
