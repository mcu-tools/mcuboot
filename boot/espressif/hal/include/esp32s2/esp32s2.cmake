# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND hal_srcs
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/rtc_init.c
    ${esp_hal_dir}/components/hal/cache_hal.c
    ${esp_hal_dir}/components/efuse/src/efuse_controller/keys/with_key_purposes/esp_efuse_api_key.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_crc.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_regi2c_esp32s2.c

    )

list(APPEND LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib-funcs.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.spiflash.ld
    )

set_source_files_properties(
    ${esp_hal_dir}/components/bootloader_support/src/esp32s2/bootloader_esp32s2.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-unused-but-set-variable")
