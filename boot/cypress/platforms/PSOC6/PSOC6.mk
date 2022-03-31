################################################################################
# \file PSOC6.mk
# \version 1.0
#
# \brief
# Makefile to describe supported boards and platforms for Cypress MCUBoot based applications.
#
################################################################################
# \copyright
# Copyright 2018-2019 Cypress Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

include host.mk

# PDL category suffix to resolve common path in pdl
PDL_CAT_SUFFIX := 1A

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
ifeq ($(PLATFORM), PSOC_062_2M)
# base kit CY8CPROTO-062-4343W
DEVICE ?= CY8C624ABZI_S2D44
PLATFORM_SUFFIX := 02
else ifeq ($(PLATFORM), PSOC_062_1M)
# base kit CY8CKIT-062-WIFI-BT
DEVICE ?= CY8C6247BZI-D54
PLATFORM_SUFFIX := 01
else ifeq ($(PLATFORM), PSOC_062_512K)
# base kit CY8CPROTO-062S3-4343W
DEVICE ?= CY8C6245LQI-S3D72
PLATFORM_SUFFIX := 03
endif

# Add device name to defines
DEFINES += $(DEVICE)

# Default upgrade method
PLATFORM_DEFAULT_USE_OVERWRITE ?= 0

###############################################################################
# Application specific libraries
###############################################################################
# MCUBootApp
###############################################################################
THIS_APP_PATH = $(PRJ_DIR)/libs

ifeq ($(APP_NAME), MCUBootApp)

CORE := CM0P
CORE_SUFFIX = m0plus

# Add retartget IO implementation using pdl
PLATFORM_SOURCES_RETARGET_IO_PDL := $(wildcard $(THIS_APP_PATH)/retarget_io_pdl/*.c)

# Collect dirrectories containing headers for PLATFORM
PLATFORM_INCLUDE_RETARGET_IO_PDL := $(THIS_APP_PATH)/retarget_io_pdl

# PSOC6HAL source files
PLATFORM_SOURCES_HAL_MCUB := $(THIS_APP_PATH)/mtb-hal-cat1/source/cyhal_crypto_common.c
PLATFORM_SOURCES_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/source/cyhal_hwmgr.c

# needed for Crypto HW Acceleration and headers inclusion, do not use for peripherals
# peripherals should be accessed
PLATFORM_INCLUDE_DIRS_HAL_MCUB := $(THIS_APP_PATH)/mtb-hal-cat1/COMPONENT_CAT1A/include
PLATFORM_INCLUDE_DIRS_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/include
PLATFORM_INCLUDE_DIRS_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/include_pvt
PLATFORM_INCLUDE_DIRS_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/COMPONENT_CAT1A/include/pin_packages

# mbedTLS hardware acceleration settings
ifeq ($(USE_CRYPTO_HW), 1)
# cy-mbedtls-acceleration related include directories
INCLUDE_DIRS_MBEDTLS_MXCRYPTO := $(THIS_APP_PATH)/cy-mbedtls-acceleration/mbedtls_MXCRYPTO
# Collect source files for MbedTLS acceleration
SOURCES_MBEDTLS_MXCRYPTO := $(wildcard $(THIS_APP_PATH)/cy-mbedtls-acceleration/mbedtls_MXCRYPTO/*.c)
#
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS_MXCRYPTO))
# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_MBEDTLS_MXCRYPTO)
endif

###############################################################################
# Application dependent definitions
# MCUBootApp default settings
# 0 by default until mbedtls.3.0 support
USE_CRYPTO_HW ?= 0

# Bootloader size
PLATFORM_BOOTLOADER_SIZE ?= 0x18000
###############################################################################

PLATFORM_INCLUDE_DIRS_FLASH := $(PRJ_DIR)/cy_flash_pal
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/cy_flash_pal/flash_psoc6/flash_qspi
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/cy_flash_pal/flash_psoc6/include
# INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/cy_flash_pal/flash_psoc/include/flash_map_backend)
PLATFORM_SOURCES_FLASH := $(wildcard $(PRJ_DIR)/cy_flash_pal/flash_psoc6/*.c)
PLATFORM_SOURCES_FLASH += $(wildcard $(PRJ_DIR)/cy_flash_pal/flash_psoc6/flash_qspi/*.c)

ifeq ($(USE_EXTERNAL_FLASH), 1)
ifeq ($(BUILDCFG), Debug)
# Include files with statically defined SMIF configuration to enable
# OpenOCD debugging of external memory
PLATFORM_SOURCES_FLASH += cy_serial_flash_prog.c
PLATFORM_SOURCES_FLASH += $(PRJ_DIR)/cy_flash_pal/flash_psoc6/smif_cfg_dbg/cycfg_qspi_memslot.c
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/cy_flash_pal/flash_psoc6/smif_cfg_dbg
endif
endif

# Post build job to execute for platform
post_build: $(OUT_CFG)/$(APP_NAME)_unsigned.hex
ifeq ($(POST_BUILD_ENABLE), 1)
	$(info [POST BUILD] - Executing post build script for $(APP_NAME))
	$(shell cp -f $(OUT_CFG)/$(APP_NAME)_unsigned.hex $(OUT_CFG)/$(APP_NAME).hex)
	$(GCC_PATH)/bin/arm-none-eabi-objdump -s $(OUT_CFG)/$(APP_NAME).hex > $(OUT_CFG)/$(APP_NAME).objdump
else
	$(info Post build is disabled by POST_BUILD_ENABLE parameter)
endif # POST_BUILD_ENABLE
endif ## MCUBootApp

###############################################################################
# BlinkyApp
###############################################################################
ifeq ($(APP_NAME), BlinkyApp)

CORE := CM4
CORE_SUFFIX = m4

ifeq ($(USE_EXTERNAL_FLASH), 1)
PLATFORM_DEFAULT_ERASED_VALUE := 0xff
else
PLATFORM_DEFAULT_ERASED_VALUE := 0
endif

# Define start of application, RAM start and size, slot size
ifeq ($(PLATFORM), PSOC_062_2M)
ifeq ($(USE_XIP), 1)
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0xE0000
else
	PLATFORM_DEFAULT_RAM_START ?= 0x08040000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
else ifeq ($(PLATFORM), PSOC_062_1M)
ifeq ($(USE_XIP), 1)
	PLATFORM_DEFAULT_RAM_START ?= 0x08000800
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x47800
else
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
else ifeq ($(PLATFORM), PSOC_062_512K)
ifeq ($(USE_XIP), 1)
	PLATFORM_DEFAULT_RAM_START ?= 0x08000800
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x3F800
else
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
endif
# Default start address of application (boot)
PLATFORM_USER_APP_START ?= $(PRIMARY_IMG_START)
# For PSOC6 platform PRIMARY_IMG_START start is the same as USER_APP_START
# This parameter can be different in cases when code is resided in
# flash mapped to one address range, but executed using different bus
# for access with another address range. For example, execution of code
# from external memory in XIP mode.
PLATFORM_DEFAULT_PRIMARY_IMG_START ?= $(PLATFORM_DEFAULT_USER_APP_START)

PLATFORM_INCLUDE_DIRS_FLASH := $(PRJ_DIR)/cy_flash_pal
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/cy_flash_pal/flash_psoc6/flash_qspi
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/cy_flash_pal/flash_psoc6/include
PLATFORM_SOURCES_FLASH += $(wildcard $(PRJ_DIR)/cy_flash_pal/flash_psoc6/flash_qspi/*.c)

# We still need this for MCUBoot apps signing
IMGTOOL_PATH ?=	../../scripts/imgtool.py

PLATFORM_DEFAULT_IMG_VER_ARG ?= -v "1.0.0"

PLATFORM_SIGN_ARGS := sign --header-size 1024 --pad-header --align 8 -M 512

# Set parameters needed for signing
ifeq ($(IMG_TYPE), UPGRADE)
	# Use encryption and random initial vector for image
	ifeq ($(ENC_IMG), 1)
		PLATFORM_SIGN_ARGS += --encrypt ../../$(ENC_KEY_FILE).pem
		PLATFORM_SIGN_ARGS += --use-random-iv
	endif
endif

# Post build action to execute after main build job
post_build: $(OUT_CFG)/$(APP_NAME).bin
ifeq ($(POST_BUILD_ENABLE), 1)
	$(info [POST BUILD] - Executing post build script for $(APP_NAME))
	$(shell mv -f $(OUT_CFG)/$(APP_NAME).bin $(OUT_CFG)/$(APP_NAME)_unsigned.bin)
	$(PYTHON_PATH) $(IMGTOOL_PATH) $(SIGN_ARGS) -S $(SLOT_SIZE) -R $(ERASED_VALUE) $(UPGRADE_TYPE) -k keys/$(SIGN_KEY_FILE).pem $(OUT_CFG)/$(APP_NAME)_unsigned.bin $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex --hex-addr=$(HEADER_OFFSET)
	$(GCC_PATH)/bin/arm-none-eabi-objdump -s $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex > $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).objdump
else
	$(info Post build is disabled by POST_BUILD_ENABLE parameter)
endif # POST_BUILD_ENABLE
endif ## BlinkyApp


###############################################################################
# Toolchain
###############################################################################
# Define build flags specific to a certain platform
CFLAGS_PLATFORM := -mcpu=cortex-$(CORE_SUFFIX) -mfloat-abi=soft -fno-stack-protector -fstrict-aliasing

###############################################################################
# Common libraries
###############################################################################
PLATFORM_SYSTEM_FILE_NAME := system_psoc6_c$(CORE_SUFFIX).c
PLATFORM_STARTUP_FILE := $(PRJ_DIR)/libs/mtb-pdl-cat1/devices/COMPONENT_CAT1A/templates/COMPONENT_MTB/COMPONENT_$(CORE)/TOOLCHAIN_$(COMPILER)/startup_psoc6_$(PLATFORM_SUFFIX)_c$(CORE_SUFFIX).S

PLATFORM_INCLUDE_DIRS_PDL_STARTUP := $(PRJ_DIR)/libs/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/templates/COMPONENT_MTB

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### PSOC6.mk ####)
$(info APP_NAME <-- $(APP_NAME))
$(info CFLAGS_PLATFORM <-> $(CFLAGS_PLATFORM))
$(info COMPILER <-- $(COMPILER))
$(info CORE <-> $(CORE))
$(info CORE_SUFFIX <-- $(CORE_SUFFIX))
$(info DEFINES <-> $(DEFINES))
$(info DEVICE <-> $(DEVICE))
$(info ENC_IMG <-- $(ENC_IMG))
$(info ENC_KEY_FILE <-- $(ENC_KEY_FILE))
$(info ERASED_VALUE <-- $(ERASED_VALUE))
$(info FAMILY <-- $(FAMILY))
$(info GCC_PATH <-- $(GCC_PATH))
$(info HEADER_OFFSET <-- $(HEADER_OFFSET))
$(info IMGTOOL_PATH <-> $(IMGTOOL_PATH))
$(info IMG_TYPE <-- $(IMG_TYPE))
$(info INCLUDE_DIRS_LIBS <-> $(INCLUDE_DIRS_LIBS))
$(info INCLUDE_DIRS_MBEDTLS_MXCRYPTO <-> $(INCLUDE_DIRS_MBEDTLS_MXCRYPTO))
$(info MCUBOOT_IMAGE_NUMBER <-- $(MCUBOOT_IMAGE_NUMBER))
$(info OUT_CFG <-- $(OUT_CFG))
$(info PDL_CAT_SUFFIX <-> $(PDL_CAT_SUFFIX))
$(info PLATFORM <-- $(PLATFORM))
$(info PLATFORM_APP_SOURCES <-> $(PLATFORM_APP_SOURCES))
$(info PLATFORM_BOOTLOADER_SIZE <-> $(PLATFORM_BOOTLOADER_SIZE))
$(info PLATFORM_DEFAULT_ERASED_VALUE <-> $(PLATFORM_DEFAULT_ERASED_VALUE))
$(info PLATFORM_DEFAULT_RAM_SIZE <-> $(PLATFORM_DEFAULT_RAM_SIZE))
$(info PLATFORM_DEFAULT_RAM_START <-> $(PLATFORM_DEFAULT_RAM_START))
$(info PLATFORM_DEFAULT_SLOT_SIZE <-> $(PLATFORM_DEFAULT_SLOT_SIZE))
$(info PLATFORM_DEFAULT_USER_APP_START <-> $(PLATFORM_DEFAULT_USER_APP_START))
$(info PLATFORM_DEFAULT_PRIMARY_IMG_START <-> $(PLATFORM_DEFAULT_PRIMARY_IMG_START))
$(info PLATFORM_DEFAULT_USE_OVERWRITE <-> $(PLATFORM_DEFAULT_USE_OVERWRITE))
$(info PLATFORM_DEFINES <-- $(PLATFORM_DEFINES))
$(info PLATFORM_EXTERNAL_FLASH_SCRATCH_OFFSET <-> $(PLATFORM_EXTERNAL_FLASH_SCRATCH_OFFSET))
$(info PLATFORM_EXTERNAL_FLASH_SECONDARY_1_OFFSET <-> $(PLATFORM_EXTERNAL_FLASH_SECONDARY_1_OFFSET))
$(info PLATFORM_EXTERNAL_FLASH_SECONDARY_2_OFFSET <-> $(PLATFORM_EXTERNAL_FLASH_SECONDARY_2_OFFSET))
$(info PLATFORM_IMAGE_1_SLOT_SIZE <-> $(PLATFORM_IMAGE_1_SLOT_SIZE))
$(info PLATFORM_IMAGE_2_SLOT_SIZE <-> $(PLATFORM_IMAGE_2_SLOT_SIZE))
$(info PLATFORM_INCLUDE_DIRS_FLASH <-> $(PLATFORM_INCLUDE_DIRS_FLASH))
$(info PLATFORM_INCLUDE_DIRS_HAL_MCUB <-> $(PLATFORM_INCLUDE_DIRS_HAL_MCUB))
$(info PLATFORM_INCLUDE_DIRS_PDL_STARTUP <-> $(PLATFORM_INCLUDE_DIRS_PDL_STARTUP))
$(info PLATFORM_INCLUDE_RETARGET_IO_PDL <-> $(PLATFORM_INCLUDE_RETARGET_IO_PDL))
$(info PLATFORM_SCRATCH_SIZE <-> $(PLATFORM_SCRATCH_SIZE))
$(info PLATFORM_DEFAULT_IMG_VER_ARG <-> $(PLATFORM_DEFAULT_IMG_VER_ARG))
$(info PLATFORM_SIGN_ARGS <-> $(PLATFORM_SIGN_ARGS))
$(info PLATFORM_SOURCES_FLASH <-> $(PLATFORM_SOURCES_FLASH))
$(info PLATFORM_SOURCES_HAL_MCUB <-> $(PLATFORM_SOURCES_HAL_MCUB))
$(info PLATFORM_SOURCES_RETARGET_IO_PDL <-> $(PLATFORM_SOURCES_RETARGET_IO_PDL))
$(info PLATFORM_STARTUP_FILE <-> $(PLATFORM_STARTUP_FILE))
$(info PLATFORM_SUFFIX <-> $(PLATFORM_SUFFIX))
$(info PLATFORM_SYSTEM_FILE_NAME <-> $(PLATFORM_SYSTEM_FILE_NAME))
$(info POST_BUILD_ENABLE <-- $(POST_BUILD_ENABLE))
$(info PRJ_DIR <-- $(PRJ_DIR))
$(info PYTHON_PATH <-- $(PYTHON_PATH))
$(info SIGN_ARGS <-- $(SIGN_ARGS))
$(info SIGN_KEY_FILE <-- $(SIGN_KEY_FILE))
$(info SLOT_SIZE <-- $(SLOT_SIZE))
$(info SOURCES_LIBS <-> $(SOURCES_LIBS))
$(info SOURCES_MBEDTLS_MXCRYPTO <-> $(SOURCES_MBEDTLS_MXCRYPTO))
$(info THIS_APP_PATH <-- $(THIS_APP_PATH))
$(info UPGRADE_SUFFIX <-- $(UPGRADE_SUFFIX))
$(info UPGRADE_TYPE <-- $(UPGRADE_TYPE))
$(info USER_APP_START <-- $(USER_APP_START))
$(info USE_CRYPTO_HW <-> $(USE_CRYPTO_HW))
$(info USE_CUSTOM_MEMORY_MAP <-> $(USE_CUSTOM_MEMORY_MAP))
$(info USE_EXTERNAL_FLASH <-> $(USE_EXTERNAL_FLASH))
$(info USE_OVERWRITE <-- $(USE_OVERWRITE))
$(info USE_XIP <-- $(USE_XIP))
endif
