# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND hal_srcs
    ${esp_idf_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/regi2c_ctrl.c
    ${esp_idf_dir}/components/efuse/src/esp_efuse_api_key_esp32xx.c
    )

list(APPEND LINKER_SCRIPTS
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib-funcs.ld
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.spiflash.ld
    )
