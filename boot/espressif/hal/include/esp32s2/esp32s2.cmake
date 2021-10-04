list(APPEND hal_srcs
    ${esp_idf_dir}/components/esp_hw_support/port/${MCUBOOT_TARGET}/regi2c_ctrl.c
    )

list(APPEND LINKER_SCRIPTS
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.newlib-funcs.ld
    -T${esp_idf_dir}/components/esp_rom/${MCUBOOT_TARGET}/ld/${MCUBOOT_TARGET}.rom.spiflash.ld
    )
