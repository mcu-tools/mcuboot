# Copyright (c) 2024 Jamie Smith
# SPDX-License-Identifier: Apache-2.0

check_python_package(imgtool.main MCUBOOT_IMGTOOL_FOUND)

get_filename_component(IMGTOOL_SCRIPTS_DIR ${CMAKE_CURRENT_LIST_DIR}/../../scripts REALPATH)

# Find or install imgtool

if(NOT MCUBOOT_IMGTOOL_FOUND)
  # If we are using the Mbed venv, we can install asn1tools automatically
  if(MBED_CREATE_PYTHON_VENV)
    message(STATUS "mcuboot: Installing imgtool into Mbed's Python virtualenv")
    execute_process(
      COMMAND ${Python3_EXECUTABLE} -m pip install ${IMGTOOL_SCRIPTS_DIR}
      COMMAND_ERROR_IS_FATAL ANY
    )
  else()
    message(FATAL_ERROR "The mcuboot imgtool python package needs to be installed (from mcuboot/scripts/) into Mbed's python interpreter (${Python3_EXECUTABLE})")
  endif()
endif()

# Signing key
set(MCUBOOT_SIGNING_KEY "" CACHE STRING "Path to key file (.pem) used to sign firmware updates for your device. The public key will be stored in the bootloader. This file must be kept safe!")

# Make sure the signing key path is absolute for EXISTS, relative to the top level build dir
get_filename_component(MCUBOOT_SIGNING_KEY_ABSPATH "${MCUBOOT_SIGNING_KEY}" ABSOLUTE BASE_DIR ${CMAKE_SOURCE_DIR})
set(MCUBOOT_SIGNING_KEY_ABSPATH ${MCUBOOT_SIGNING_KEY_ABSPATH} CACHE INTERNAL "Absolute version of MCUBOOT_SIGNING_KEY" FORCE)

if("${MCUBOOT_SIGNING_KEY}" STREQUAL "" OR NOT EXISTS "${MCUBOOT_SIGNING_KEY_ABSPATH}")
  message(FATAL_ERROR "Must specify path to valid image signing key via MCUBOOT_SIGNING_KEY CMake option in order to build this project.")
endif()

# Encryption key
if("MCUBOOT_ENCRYPT_RSA=1" IN_LIST MBED_CONFIG_DEFINITIONS)
  set(MCUBOOT_ENCRYPTION_KEY "" CACHE STRING "Path to key file (.pem) used to encrypt firmware updates for your device. The private key will be stored in the bootloader. This file must be kept safe!")

  # Make sure the signing key path is absolute for EXISTS, relative to the top level build dir
  get_filename_component(MCUBOOT_ENCRYPTION_KEY_ABSPATH "${MCUBOOT_ENCRYPTION_KEY}" ABSOLUTE BASE_DIR ${CMAKE_SOURCE_DIR})
  set(MCUBOOT_ENCRYPTION_KEY_ABSPATH ${MCUBOOT_ENCRYPTION_KEY_ABSPATH} CACHE INTERNAL "Absolute version of MCUBOOT_ENCRYPTION_KEY" FORCE)

  if("${MCUBOOT_ENCRYPTION_KEY}" STREQUAL "" OR NOT EXISTS "${MCUBOOT_ENCRYPTION_KEY_ABSPATH}")
    message(FATAL_ERROR "Since mcuboot.encrypt-rsa is enabled, you must specify the path to a valid image encryption key via the MCUBOOT_ENCRYPTION_KEY CMake option in order to build this project.")
  endif()
endif()

# Imgtool usage functions

#
# Generate a signed image hex file for the given executable target.
#
function(_mcuboot_generate_image TARGET IMAGE_TYPE IMAGE_BASE_PATH)
  if("${MBED_OUTPUT_EXT}" STREQUAL "bin")
    message(FATAL_ERROR "Hex file output must be enabled to use mcuboot.  Set MBED_OUTPUT_EXT to empty string after including app.cmake in your top level CMakeLists.txt!")
  endif()

  if("${PROJECT_VERSION}" STREQUAL "")
    message(FATAL_ERROR "You must set the project version to sign images by passing a version number into your app's project() command!")
  endif()

  # mbed_generate_bin_hex() puts the hex file at the following path
  set(TARGET_HEX_FILE ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_BASE_NAME:${TARGET}>.hex)

  # Grab header size
  if(NOT "${MBED_CONFIG_DEFINITIONS}" MATCHES "MCUBOOT_HEADER_SIZE=(0x[0-9A-Fa-f]+)")
    message(FATAL_ERROR "Couldn't find MCUBOOT_HEADER_SIZE in Mbed configuration!")
  endif()
  set(HEADER_SIZE_HEX ${CMAKE_MATCH_1})

  # Grab slot size
  if(NOT "${MBED_CONFIG_DEFINITIONS}" MATCHES "MCUBOOT_SLOT_SIZE=(0x[0-9A-Fa-f]+)")
    message(FATAL_ERROR "Couldn't find MCUBOOT_SLOT_SIZE in Mbed configuration!")
  endif()
  set(SLOT_SIZE_HEX ${CMAKE_MATCH_1})

  get_property(objcopy GLOBAL PROPERTY ELF2BIN)

  if(${IMAGE_TYPE} STREQUAL "update" AND "MCUBOOT_ENCRYPT_RSA=1" IN_LIST MBED_CONFIG_DEFINITIONS)
    set(IMGTOOL_EXTRA_ARGS --encrypt ${MCUBOOT_ENCRYPTION_KEY_ABSPATH})
  elseif(${IMAGE_TYPE} STREQUAL "initial" AND "MCUBOOT_ENCRYPT_RSA=1" IN_LIST MBED_CONFIG_DEFINITIONS)
    # If encryption is enabled, generate unencrypted initial image which supports encryption.
    # See https://github.com/mbed-ce/mbed-os/issues/401#issuecomment-2567099213
    set(IMGTOOL_EXTRA_ARGS --clear)
  else()
    set(IMGTOOL_EXTRA_ARGS "")
  endif()

  add_custom_command(
    TARGET ${TARGET}
    POST_BUILD
    DEPENDS ${MCUBOOT_SIGNING_KEY_ABSPATH}
    COMMAND
      ${Python3_EXECUTABLE} -m imgtool.main
      sign
      --key ${MCUBOOT_SIGNING_KEY_ABSPATH} # this specifies the file containing the keys used to sign/verify the application
      --align 4 # this lets mcuboot know the intrinsic alignment of the flash (32-bits = 4 byte alignment)
      --version ${PROJECT_VERSION} # this sets the version number of the application
      --header-size ${HEADER_SIZE_HEX} # this must be the same as the value specified in mcuboot.header-size configuration
      --pad-header # this tells imgtool to insert the entire header, including any necessary padding bytes.
      --slot-size ${SLOT_SIZE_HEX} # this specifies the maximum size of the application ("slot size"). It must be the same as the configured mcuboot.slot-size!
      ${IMGTOOL_EXTRA_ARGS}
      ${TARGET_HEX_FILE} ${IMAGE_BASE_PATH}.hex

    COMMAND
      ${CMAKE_COMMAND} -E echo "-- built: ${IMAGE_BASE_PATH}.hex"

    # Also generate bin file
    COMMAND
      ${objcopy} -I ihex -O binary ${IMAGE_BASE_PATH}.hex ${IMAGE_BASE_PATH}.bin

    COMMAND
      ${CMAKE_COMMAND} -E echo "-- built: ${IMAGE_BASE_PATH}.bin"

    COMMENT "Generating mcuboot ${IMAGE_TYPE} image for ${TARGET}..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
  )
endfunction(_mcuboot_generate_image)

#
# Generate an initial image hex file for the given executable target.
# This initial image is what should be flashed to a blank device (along with the bootloader).
# A flash target (ninja flash-${TARGET}-initial-image) will also be created.
#
# NOTE: This function must be called *after* mbed_set_post_build() for the target!
#
# If you wish to specify the base name of the initial image, pass that as the second argument to
# this function.  Otherwise, it will default to $<target name>-initial-image
#
function(mcuboot_generate_initial_image TARGET) # optional 2nd arg: initial image base filename
  # Figure out file path
  if("${ARGN}" STREQUAL "")
    set(INITIAL_IMAGE_BASE_PATH ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_BASE_NAME:${TARGET}>-initial-image)
  else()
    set(INITIAL_IMAGE_BASE_PATH ${CMAKE_CURRENT_BINARY_DIR}/${ARGN})
  endif()

  _mcuboot_generate_image(${TARGET} initial ${INITIAL_IMAGE_BASE_PATH})

  # Create a flash target.
  # We need to be slightly creative here -- Mbed thinks that the application start address
  # is <primary slot address> + <header size>, but we actually want to upload to <primary slot address>.
  # So we need to temporarily override MBED_UPLOAD_BASE_ADDR with an offset value
  if(NOT "${MBED_CONFIG_DEFINITIONS}" MATCHES "MCUBOOT_HEADER_SIZE=(0x[0-9A-Fa-f]+)")
    message(FATAL_ERROR "Couldn't find MCUBOOT_HEADER_SIZE in Mbed configuration!")
  endif()
  set(HEADER_SIZE_HEX ${CMAKE_MATCH_1})
  math(EXPR MBED_UPLOAD_BASE_ADDR "${MBED_UPLOAD_BASE_ADDR} - ${HEADER_SIZE_HEX}" OUTPUT_FORMAT HEXADECIMAL)

  gen_upload_target(${TARGET}-initial-image ${INITIAL_IMAGE_BASE_PATH}.bin)
  if(TARGET flash-${TARGET}-initial-image)
    add_dependencies(flash-${TARGET}-initial-image ${TARGET})
  endif()
endfunction(mcuboot_generate_initial_image)

#
# Generate an update image hex file for the given executable target.
# This image is what should be flashed to the secondary block device and passed to
# mcuboot as an update file.
#
# NOTE: This function must be called *after* mbed_set_post_build() for the target!
#
# NOTE 2: The hex file produced by this function will still "declare" its address as the primary slot
# address. This can cause issues if you pass it to a tool that uses this offset to decide where to load it.
# If this is a problem, we recommend the "arm-none-eabi-objcopy --change-addresses" command to change this address.
#
# If you wish to specify the base name of the update image, pass that as the second argument to
# this function.  Otherwise, it will default to $<target name>-update-image
#
function(mcuboot_generate_update_image TARGET) # optional 2nd arg: update image base filename
  # Figure out file path
  if("${ARGN}" STREQUAL "")
    set(UPDATE_IMAGE_BASE_PATH ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_BASE_NAME:${TARGET}>-update-image)
  else()
    set(UPDATE_IMAGE_BASE_PATH${CMAKE_CURRENT_BINARY_DIR}/${ARGN})
  endif()

  _mcuboot_generate_image(${TARGET} update ${UPDATE_IMAGE_BASE_PATH})
endfunction(mcuboot_generate_update_image)

#
# Generate a C source file with signing keys in it at the given location.
# The file should be added as a source file to a library built in the same directory.
#
function(mcuboot_generate_signing_keys_file SIGNING_KEYS_C_PATH)
  add_custom_command(
    OUTPUT ${SIGNING_KEYS_C_PATH}
    COMMAND
      ${Python3_EXECUTABLE} -m imgtool.main
      getpub
      --key ${MCUBOOT_SIGNING_KEY_ABSPATH}
      --output ${SIGNING_KEYS_C_PATH}

    DEPENDS ${MCUBOOT_SIGNING_KEY_ABSPATH}
    COMMENT "Converting signing key to C source..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
  )
endfunction(mcuboot_generate_signing_keys_file)

#
# Generate a C source file with the encryption private key in it at the given location.
# The file should be added as a source file to a library built in the same directory.
#
function(mcuboot_generate_encryption_key_file ENC_KEY_C_PATH)
  add_custom_command(
    OUTPUT ${ENC_KEY_C_PATH}
    COMMAND
      ${Python3_EXECUTABLE} -m imgtool.main
      getpriv
      --key ${MCUBOOT_SIGNING_KEY_ABSPATH}
      > ${ENC_KEY_C_PATH}

    DEPENDS ${MCUBOOT_SIGNING_KEY_ABSPATH}
    COMMENT "Converting encryption key to C source..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
  )
endfunction(mcuboot_generate_encryption_key_file)