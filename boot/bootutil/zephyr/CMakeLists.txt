# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MCUBOOT_BOOTUTIL_LIB)

zephyr_interface_library_named(MCUBOOT_BOOTUTIL)

target_include_directories(MCUBOOT_BOOTUTIL INTERFACE
  ../
  ../include
  ../../zephyr/include
)

zephyr_library_named(mcuboot_util)
zephyr_library_sources(
  ../src/bootutil_public.c
    )

if(CONFIG_BOOT_IMAGE_ACCESS_HOOKS)
  if(NOT CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE STREQUAL "")
    if(IS_ABSOLUTE ${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE})
      if(EXISTS ${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE})
        set(HOOKS_FILE ${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE})
      endif()
    elseif((DEFINED CONF_DIR) AND
           (EXISTS ${CONF_DIR}/${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE}))
      set(HOOKS_FILE ${CONF_DIR}/${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE})
    else(EXISTS ${APPLICATION_SOURCE_DIR}/${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE})
      set(HOOKS_FILE ${APPLICATION_SOURCE_DIR}/${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE})
    endif()
  endif()

  if(DEFINED HOOKS_FILE)
      zephyr_library_sources(
        ${HOOKS_FILE}
      )
  else()
    message(STATUS "No hooks implementation file.")
  endif()
endif()

zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
target_link_libraries(MCUBOOT_BOOTUTIL INTERFACE zephyr_interface)

if(CONFIG_BOOT_USE_TINYCRYPT)
target_include_directories(MCUBOOT_BOOTUTIL INTERFACE
  ../../../ext/tinycrypt/lib/include
)
endif()
endif()
