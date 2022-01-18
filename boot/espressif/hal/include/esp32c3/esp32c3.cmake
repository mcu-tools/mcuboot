# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND hal_srcs
    ${esp_idf_dir}/components/bootloader_support/src/flash_qio_mode.c
    ${esp_idf_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/cpu_util_esp32c3.c
    ${esp_idf_dir}/components/efuse/src/esp_efuse_api_key_esp32xx.c
)

list(APPEND LINKER_SCRIPTS
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib.ld
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.eco3.ld
)

set_source_files_properties(
    ${esp_idf_dir}/components/bootloader_support/src/flash_qio_mode.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-unused-variable")
