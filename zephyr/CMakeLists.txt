if(CONFIG_BOOTLOADER_MCUBOOT)
  # Build a second bootloader image

  set(MCUBOOT_BASE ${CMAKE_CURRENT_LIST_DIR}/..)

  zephyr_add_executable(mcuboot require_build)

  if (${require_build})
    add_subdirectory(${MCUBOOT_BASE}/boot/zephyr ${CMAKE_CURRENT_BINARY_DIR}/mcuboot)

    # TODO: Assert that the bootloader and image use the same key.

    set(SIGNED_IMAGE signed.hex)

    set_property(GLOBAL APPEND PROPERTY
      extra_post_build_commands
      COMMAND
      ${PYTHON_EXECUTABLE}
      ${MCUBOOT_BASE}/scripts/imgtool.py
      sign
      --key ${MCUBOOT_BASE}/${CONFIG_BOOT_SIGNATURE_KEY_FILE}
      --header-size ${CONFIG_TEXT_SECTION_OFFSET}
      --align       ${DT_FLASH_WRITE_BLOCK_SIZE}
      --version 0.1       # TODO: Configurable?
      --slot-size 0x32000 # TODO: Configurable?
      ${KERNEL_HEX_NAME}  # TODO: Enforce that this will be present through Kconfig
      ${SIGNED_IMAGE}
      )

    set_property(GLOBAL APPEND PROPERTY
      HEX_FILES_TO_MERGE
      ${SIGNED_IMAGE}
      )
    set_property(GLOBAL APPEND PROPERTY
      HEX_FILES_TO_MERGE_TARGET
      ${logical_target_for_zephyr_elf}
      )
  endif() # ${require_build}
endif()
