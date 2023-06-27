# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND hal_srcs
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_init.c
    ${esp_hal_dir}/components/hal/cache_hal.c
    ${esp_hal_dir}/components/efuse/${MCUBOOT_TARGET}/esp_efuse_table.c
    ${esp_hal_dir}/components/efuse/src/efuse_controller/keys/without_key_purposes/one_key_block/esp_efuse_api_key.c
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
