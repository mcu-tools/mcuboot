# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

list(APPEND include_dirs
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/private_include
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/include/hw_ver_mp
    ${esp_hal_dir}/components/soc/${MCUBOOT_TARGET}/register/hw_ver_mp
)

list(APPEND hal_srcs
    ${esp_hal_dir}/components/hal/cache_hal.c
    ${esp_hal_dir}/components/efuse/src/efuse_controller/keys/with_key_purposes/esp_efuse_api_key.c
    ${esp_hal_dir}/components/esp_rom/patches/esp_rom_regi2c_${MCUBOOT_TARGET}.c
    ${esp_hal_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/pmu_param.c
)

# ESP32-H4 uses stub RNG enable/disable in bootloader_random.c (bring-up)
list(REMOVE_ITEM hal_srcs
    ${esp_hal_dir}/components/bootloader_support/src/bootloader_random_esp32h4.c)

if (DEFINED CONFIG_ESP_CONSOLE_UART_CUSTOM)
    list(APPEND hal_srcs
        ${src_dir}/${MCUBOOT_TARGET}/console_uart_custom.c
        )
endif()

list(APPEND LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.ld
    # -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.libgcc.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.libc.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib.ld
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.version.ld
)

# ESP32-H4 ROM does not provide the misaligned-mem libc variant
list(REMOVE_ITEM LINKER_SCRIPTS
    -T${esp_hal_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.libc-suboptimal_for_misaligned_mem.ld)

set_source_files_properties(
    ${esp_hal_dir}/components/bootloader_support/src/esp_image_format.c
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/src/bootloader_flash.c
    ${esp_hal_dir}/components/bootloader_support/bootloader_flash/src/bootloader_flash_config_${MCUBOOT_TARGET}.c
    ${esp_hal_dir}/components/hal/mmu_hal.c
    ${esp_hal_dir}/components/hal/cache_hal.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-logical-op")
