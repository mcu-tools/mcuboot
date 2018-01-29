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

# Set CONF_SIGNATURE_TYPE to determine the signature type used.
# Currently, it should be set to either RSA or ECDSA_P256.
#
# To choose RSA (this is the default):
#
#     cmake -DCONF_SIGNATURE_TYPE=RSA [...]
#
# To use ECDSA_P256:
#
#     cmake -DCONF_SIGNATURE_TYPE=ECDSA_P256 [...]
if (NOT DEFINED CONF_SIGNATURE_TYPE)
  set(CONF_SIGNATURE_TYPE RSA)
endif()

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

##############################
# End of configuration blocks.
##############################

set(MCUBOOT_EXTRA_CFLAGS)

# Determine CFLAGS / MCUBOOT_CONF_FILE / NEED_TINYCRYPT from the signature type.
if(CONF_SIGNATURE_TYPE STREQUAL RSA)
  set(MCUBOOT_CONF_FILE prj.conf) # RSA
  list(APPEND MCUBOOT_EXTRA_CFLAGS "-DMCUBOOT_SIGN_RSA" "-DMCUBOOT_USE_MBED_TLS")
  set(NEED_TINYCRYPT NO)
elseif(CONF_SIGNATURE_TYPE STREQUAL ECDSA_P256)
  set(MCUBOOT_CONF_FILE prj-p256.conf) # ECDSA P-256
  list(APPEND MCUBOOT_EXTRA_CFLAGS "-DMCUBOOT_SIGN_EC256" "-DMCUBOOT_USE_TINYCRYPT")
  set(NEED_TINYCRYPT YES)
else()
  message(FATAL_ERROR "Invalid CONF_SIGNATURE_TYPE specified: '${CONF_SIGNATURE_TYPE}'")
endif()

# Board-specific CONF_FILES should get merged into the build as well.
#
# Do this by defining the set_conf_file macro:
# http://docs.zephyrproject.org/application/application.html#application-configuration
macro(set_conf_file)
  if (EXISTS ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf)
    set(CONF_FILE "${MCUBOOT_CONF_FILE} ${APPLICATION_SOURCE_DIR}/boards/${BOARD}.conf")
  else()
    set(CONF_FILE "${MCUBOOT_CONF_FILE}")
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

# This is necessary to ensure mcuboot is linked into, and fits inside,
# the boot partition.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dts.overlay)

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

# Zephyr application include directories.
if (NOT NEED_TINYCRYPT)
  # Zephyr's mbedTLS needs this.
  zephyr_include_directories(include)

  # Use full mbedtls provided by OS for RSA
  target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/ext/lib/crypto/mbedtls/include)
endif()

target_include_directories(app PRIVATE include)
target_include_directories(app PRIVATE targets)
if(EXISTS "${APPLICATION_SOURCE_DIR}/targets/${BOARD}.h")
  target_compile_definitions(app PRIVATE "-DMCUBOOT_TARGET_CONFIG='\"${BOARD}.h\"'")
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

# Tinycrypt sources and includes, if needed.
if (NEED_TINYCRYPT)
  target_include_directories(app PRIVATE "${BOOT_DIR}/zephyr/include")
  target_include_directories(app PRIVATE "${TINYCRYPT_DIR}/include")
  target_include_directories(app PRIVATE "${MBEDTLS_ASN1_DIR}/include")

  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/ecc.c")
  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/ecc_dsa.c")
  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/sha256.c")
  target_sources(app PRIVATE "${TINYCRYPT_DIR}/source/utils.c")

  target_sources(app PRIVATE "${MBEDTLS_ASN1_DIR}/src/asn1parse.c")
endif()

if (CONFIG_MCUBOOT_SERIAL)
zephyr_sources(${BOOT_DIR}/zephyr/serial_adapter.c)

add_subdirectory(${BOOT_DIR}/boot_serial ./boot/boot_serial)
add_subdirectory(${BOOT_DIR}/zephyr/tinycbor)
add_subdirectory(${BOOT_DIR}/zephyr/cborattr)

endif()
