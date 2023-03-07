# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND hal_srcs
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_init.c
    ${esp_hal_dir}/components/efuse/src/efuse_controller/keys/without_key_purposes/three_key_blocks/esp_efuse_api_key.c
    )

if (DEFINED CONFIG_ESP_MULTI_PROCESSOR_BOOT)
    list(APPEND hal_srcs
        ${src_dir}/${MCUBOOT_TARGET}/app_cpu_start.c
        ${esp_hal_dir}/components/esp_hw_support/cpu.c
        )
endif()

if (DEFINED CONFIG_ESP_CONSOLE_UART_CUSTOM)
    list(APPEND hal_srcs
        ${src_dir}/${MCUBOOT_TARGET}/console_uart_custom.c
        )
endif()

list(APPEND LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib-funcs.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.eco3.ld
    )

set_source_files_properties(
    ${esp_hal_dir}/components/bootloader_support/src/esp32/bootloader_esp32.c
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/src/bootloader_flash.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-unused-variable -Wno-unused-but-set-variable")
