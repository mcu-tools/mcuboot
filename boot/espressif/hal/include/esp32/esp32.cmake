# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND include_dirs
    ${esp_idf_dir}/components/${MCUBOOT_TARGET}/include
    )

list(APPEND hal_srcs
    ${esp_idf_dir}/components/efuse/src/esp_efuse_api_key_esp32.c
    )

if (DEFINED CONFIG_ESP_MULTI_PROCESSOR_BOOT)
    list(APPEND hal_srcs
        ${src_dir}/${MCUBOOT_TARGET}/app_cpu_start.c
        )
endif()

list(APPEND LINKER_SCRIPTS
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib-funcs.ld
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.eco3.ld
    )
