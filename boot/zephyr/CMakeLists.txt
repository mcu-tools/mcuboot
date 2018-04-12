# CMakeLists.txt for building mcuboot as a Zephyr project
#
# Copyright (c) 2017 Open Source Foundries Limited
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.8.2)

set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig)

########################
# Configuration choices.
########################

# If CONF_VALIDATE_SLOT0 is set, the bootloader attempts to validate
# the signature of slot0 every boot.  This adds the signature check
# time to every boot, but can mitigate against some changes that are
# able to modify the flash image itself.
#
# To enable validation (this is the default):
#
#     cmake -DCONF_VALIDATE_SLOT0=YES [...]
#
# To disable validation:
#
#     cmake -DCONF_VALIDATE_SLOT0=NO [...]
if (NOT DEFINED CONF_VALIDATE_SLOT0)
  set(CONF_VALIDATE_SLOT0 YES)
endif()

# If CONF_UPGRADE_ONLY is set, overwrite slot0 with the upgrade image
# instead of swapping them.  This prevents the fallback recovery, but
# uses a much simpler code path.
#
# To enable "upgrade only" mode:
#
#     cmake -DCONF_UPGRADE_ONLY=YES [...]
#
# To disable "upgrade only" mode (this is the default):
#
#     cmake -DCONF_UPGRADE_ONLY=NO [...]
if (NOT DEFINED CONF_UPGRADE_ONLY)
  set(CONF_UPGRADE_ONLY NO)
endif()

# If CONF_ZEPHYR_TRY_MASS_ERASE is set (it is set by default), the
# Zephyr build system configuration attempts to force a mass erase
# before flashing. This ensures the scratch and other partitions are
# in a consistent state.
#
# This is not available for all boards.
#
# To enable the mass erase attempt (this is the default):
#
#     cmake -DCONF_ZEPHYR_TRY_MASS_ERASE=YES [...]
#
# To disable the mass erase attempt:
#
#     cmake -DCONF_ZEPHYR_TRY_MASS_ERASE=NO [...]
if (NOT DEFINED CONF_ZEPHYR_TRY_MASS_ERASE)
  set(CONF_ZEPHYR_TRY_MASS_ERASE YES)
endif()

##############################
# End of configuration blocks.
##############################

set(MCUBOOT_EXTRA_CFLAGS)

# Board-specific CONF_FILES should get merged into the build as well.
#
# Do this by defining the set_conf_file macro:
# http://docs.zephyrproject.org/application/application.html#application-configuration
macro(set_conf_file)
  if (EXISTS ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf)
    set(CONF_FILE "prj.conf ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf")
  else()
    set(CONF_FILE prj.conf)
  endif()
endmacro()

# Check if we need to validate slot 0.
if(CONF_VALIDATE_SLOT0 STREQUAL YES)
  list(APPEND MCUBOOT_EXTRA_CFLAGS "-DMCUBOOT_VALIDATE_SLOT0")
endif()

# Enabling this option uses newer flash map APIs. This saves RAM and
# avoids deprecated API usage.
#
# (This can be deleted when flash_area_to_sectors() is removed instead
# of simply deprecated.)
list(APPEND MCUBOOT_EXTRA_CFLAGS "-DMCUBOOT_USE_FLASH_AREA_GET_SECTORS")

# Check if we're operating in overwrite-only mode.
if(CONF_UPGRADE_ONLY STREQUAL YES)
  list (APPEND MCUBOOT_EXTRA_CFLAGS "-DMCUBOOT_OVERWRITE_ONLY" "-DMCUBOOT_OVERWRITE_ONLY_FAST")
endif()

# Add values in MCUBOOT_EXTRA_CFLAGS_STR to the Zephyr build's
# EXTRA_CFLAGS variable.
string(REPLACE ";" " " MCUBOOT_EXTRA_CFLAGS_STR "${MCUBOOT_EXTRA_CFLAGS}")
set(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${MCUBOOT_EXTRA_CFLAGS_STR}")

# The board should be set to a supported target.
if (NOT DEFINED BOARD)
  set(BOARD qemu_x86)
endif()

# Add a common dts overlay necessary to ensure mcuboot is linked into,
# and fits inside, the boot partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
if(DTC_OVERLAY_FILE)
  set(DTC_OVERLAY_FILE
      "${DTC_OVERLAY_FILE} ${CMAKE_CURRENT_LIST_DIR}/dts.overlay")
else()
  set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dts.overlay)
endif()

# Enable Zephyr runner options which request mass erase if so
# configured.
#
# Note that this also disables the default "leave" option when
# targeting STM32 DfuSe devices with dfu-util, making the chip stay in
# the bootloader after flashing.
#
# That's the right thing, because mcuboot has nothing to do since the
# chip was just erased. The next thing the user is going to want to do
# is flash the application. (Developers can reset DfuSE devices
# manually to test mcuboot behavior on an otherwise erased flash
# device.)
macro(app_set_runner_args)
  if(CONF_ZEPHYR_TRY_MASS_ERASE)
    board_runner_args(dfu-util "--dfuse-modifiers=force:mass-erase")
    board_runner_args(pyocd "--flashtool-opt=-ce")
  endif()
endmacro()

# Standard Zephyr application boilerplate:
# http://docs.zephyrproject.org/application/application.html
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

# Path to "boot" subdirectory of repository root.
get_filename_component(BOOT_DIR ${APPLICATION_SOURCE_DIR} DIRECTORY)
# Path to top-level repository root directory.
get_filename_component(MCUBOOT_DIR ${BOOT_DIR} DIRECTORY)
# Path to tinycrypt library source subdirectory of MCUBOOT_DIR.
set(TINYCRYPT_DIR "${MCUBOOT_DIR}/ext/tinycrypt/lib")
# Path to mbed-tls' asn1 parser library.
set(MBEDTLS_ASN1_DIR "${MCUBOOT_DIR}/ext/mbedtls")

target_include_directories(app PRIVATE include)
target_include_directories(app PRIVATE targets)
if(EXISTS "${APPLICATION_SOURCE_DIR}/targets/${BOARD}.h")
  target_compile_definitions(app PRIVATE "-DMCUBOOT_TARGET_CONFIG=\"${BOARD}.h\"")
endif()

# Zephyr port-specific sources.
target_sources(app PRIVATE main.c)
target_sources(app PRIVATE flash_map.c)
target_sources(app PRIVATE hal_flash.c)
target_sources(app PRIVATE os.c)
target_sources(app PRIVATE keys.c)
if(NOT DEFINED CONFIG_FLASH_PAGE_LAYOUT)
  target_sources(app PRIVATE flash_map_legacy.c)
endif()

# Generic bootutil sources and includes.
target_include_directories(app PRIVATE "${BOOT_DIR}/bootutil/include")
target_sources(app PRIVATE "${BOOT_DIR}/bootutil/src/loader.c")
target_sources(app PRIVATE "${BOOT_DIR}/bootutil/src/bootutil_misc.c")
target_sources(app PRIVATE "${BOOT_DIR}/bootutil/src/image_validate.c")
target_sources(app PRIVATE "${BOOT_DIR}/bootutil/src/image_rsa.c")
target_sources(app PRIVATE "${BOOT_DIR}/bootutil/src/image_ec256.c")
target_sources(app PRIVATE "${BOOT_DIR}/bootutil/src/caps.c")

if(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256)
  # When using ECDSA signatures, pull in our copy of the tinycrypt library.
  target_include_directories(app PRIVATE "${BOOT_DIR}/zephyr/include")
  target_include_directories(app PRIVATE "${TINYCRYPT_DIR}/include")
  target_include_directories(app PRIVATE "${MBEDTLS_ASN1_DIR}/include")

  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/ecc.c")
  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/ecc_dsa.c")
  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/sha256.c")
  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/utils.c")

  # Additionally pull in just the ASN.1 parser from mbedTLS.
  target_compile_definitions(app PRIVATE MBEDTLS_CFG_FILE=config-asn1.h)
  target_sources(app PRIVATE "${MBEDTLS_ASN1_DIR}/src/asn1parse.c")
elseif(CONFIG_BOOT_SIGNATURE_TYPE_RSA)
  # Use mbedTLS provided by Zephyr for RSA signatures. (Its config file
  # is set using Kconfig.)
  zephyr_include_directories(include)
  target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/ext/lib/crypto/mbedtls/include)
endif()

if (CONFIG_MCUBOOT_SERIAL)
zephyr_sources(${BOOT_DIR}/zephyr/serial_adapter.c)

add_subdirectory(${BOOT_DIR}/boot_serial ./boot/boot_serial)
endif()
