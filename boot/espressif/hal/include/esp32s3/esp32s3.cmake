# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND hal_srcs
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_init.c
    ${esp_hal_dir}/components/hal/cache_hal.c
    ${esp_hal_dir}/components/efuse/src/efuse_controller/keys/with_key_purposes/esp_efuse_api_key.c
)

if (DEFINED CONFIG_ESP_MULTI_PROCESSOR_BOOT)
    list(APPEND hal_srcs
        ${src_dir}/${MCUBOOT_TARGET}/app_cpu_start.c
        ${esp_hal_dir}/components/esp_hw_support/cpu.c
        )
endif()

list(APPEND LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib.ld
    )

set_source_files_properties(
    ${esp_hal_dir}/components/bootloader_support/src/esp32s3/bootloader_esp32s3.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-unused-variable -Wno-unused-but-set-variable")
