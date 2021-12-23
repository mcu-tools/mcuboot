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
DEVICE ?= CY8C624ABZI-D44
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
PLATFORM_SOURCES_HAL_MCUB := $(THIS_APP_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/source/cyhal_crypto_common.c
PLATFORM_SOURCES_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/source/cyhal_hwmgr.c

# needed for Crypto HW Acceleration and headers inclusion, do not use for peripherals
# peripherals should be accessed
PLATFORM_INCLUDE_DIRS_HAL_MCUB := $(THIS_APP_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/COMPONENT_CAT1A/include
PLATFORM_INCLUDE_DIRS_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/include
PLATFORM_INCLUDE_DIRS_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/include
PLATFORM_INCLUDE_DIRS_HAL_MCUB += $(THIS_APP_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/COMPONENT_CAT1A/include/pin_packages

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
USE_EXTERNAL_FLASH ?= 0

# define slot sizes for IMAGE1 and IMAGE2 in case of usage with
# external memory upgrade. 0x40000 slot size is acceptable for
# all platforms in single image case with external upgrade
ifeq ($(USE_EXTERNAL_FLASH), 1)
PLATFORM_MAX_IMG_SECTORS = 1536
PLATFORM_IMAGE_1_SLOT_SIZE ?= 0x40200
ifeq ($(MCUBOOT_IMAGE_NUMBER), 2)
PLATFORM_IMAGE_2_SLOT_SIZE ?= 0x40200
endif
else
PLATFORM_MAX_IMG_SECTORS = 128
PLATFORM_IMAGE_1_SLOT_SIZE ?= 0x10000
ifeq ($(MCUBOOT_IMAGE_NUMBER), 2)
PLATFORM_IMAGE_2_SLOT_SIZE ?= 0x20000
endif
endif

###############################################################################
# MCUBootApp flash map custom settings
###############################################################################
# Set this flag to 1 to enable custom settings in MCUBootApp
USE_CUSTOM_MEMORY_MAP ?= 0

# Bootloader size
PLATFORM_BOOTLOADER_SIZE ?= 0x18000
# status values
#PLATFORM_STATUS_PARTITION_OFFSET ?= 
# scratch values
ifeq ($(USE_EXTERNAL_FLASH), 1)
PLATFORM_SCRATCH_SIZE ?= 0x80000
PLATFORM_EXTERNAL_FLASH_SCRATCH_OFFSET ?= 0x440000
else
PLATFORM_SCRATCH_SIZE ?= 0x1000
endif
# multi image
PLATFORM_EXTERNAL_FLASH_SECONDARY_1_OFFSET ?= 0x0
PLATFORM_EXTERNAL_FLASH_SECONDARY_2_OFFSET ?= $(PLATFORM_IMAGE_1_SLOT_SIZE)

PLATFORM_CY_MAX_EXT_FLASH_ERASE_SIZE ?= 512
PLATFORM_CHUNK_SIZE := 512
###############################################################################

PLATFORM_INCLUDE_DIRS_FLASH := $(PRJ_DIR)/cy_flash_pal/flash_psoc6/flash_qspi
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/cy_flash_pal/flash_psoc6/include
# INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/cy_flash_pal/flash_psoc/include/flash_map_backend)
PLATFORM_SOURCES_FLASH := $(wildcard $(PRJ_DIR)/cy_flash_pal/flash_psoc6/*.c)
PLATFORM_SOURCES_FLASH += $(wildcard $(PRJ_DIR)/cy_flash_pal/flash_psoc6/flash_qspi/*.c)

# Platform dependend application files
PLATFORM_APP_SOURCES := cy_serial_flash_prog.c

# Post build job to execute for platform
post_build: $(OUT_CFG)/$(APP_NAME)_unsigned.hex
	$(info [POST_BUILD] - Executing post build script for $(APP_NAME))
	$(shell cp -f $(OUT_CFG)/$(APP_NAME)_unsigned.hex $(OUT_CFG)/$(APP_NAME).hex)
	$(GCC_PATH)/bin/arm-none-eabi-objdump -s $(OUT_CFG)/$(APP_NAME).hex > $(OUT_CFG)/$(APP_NAME).objdump

endif ## MCUBootApp

###############################################################################
# BlinkyApp
###############################################################################
ifeq ($(APP_NAME), BlinkyApp)

CORE := CM4
CORE_SUFFIX = m4

PLATFORM_DEFAULT_ERASED_VALUE := 0

# Define start of application, RAM start and size, slot size
ifeq ($(PLATFORM), PSOC_062_2M)
	PLATFORM_DEFAULT_USER_APP_START ?= 0x10018000
	PLATFORM_DEFAULT_RAM_START ?= 0x08040000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
else ifeq ($(PLATFORM), PSOC_062_1M)
	PLATFORM_DEFAULT_USER_APP_START ?= 0x10018000
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
else ifeq ($(PLATFORM), PSOC_062_512K)
	PLATFORM_DEFAULT_USER_APP_START ?= 0x10018000
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
# For PSOC6 platform USER_IMG_STARTo start is the same as USER_APP_START
# This parameter can be different in cases when code is resided in
# flash mapped to one address range, but executed using different bus
# for access with another address range. For example, execution of code
# from external memory in XIP mode.
PLATFORM_DEFAULT_USER_IMG_START ?= $(PLATFORM_DEFAULT_USER_APP_START)

ifeq ($(USE_EXTERNAL_FLASH), 1)
$(warning You are trying to build BlinkyApp for MCUBootApp with external memory support. Ensure you build MCUBootApp with USE_EXTERNAL_FLASH=1 flag!)
	PLATFORM_DEFAULT_SLOT_SIZE ?= 0x40200
else
	PLATFORM_DEFAULT_SLOT_SIZE ?= 0x10000
endif

# We still need this for MCUBoot apps signing
IMGTOOL_PATH ?=	../../scripts/imgtool.py

PLATFORM_SIGN_ARGS := sign --header-size 1024 --pad-header --align 8 -v "2.0" -M 512

# Set parameters needed for signing
ifeq ($(IMG_TYPE), UPGRADE)
	# Use encryption and random initial vector for image
	ifeq ($(ENC_IMG), 1)
		PLATFORM_SIGN_ARGS += --encrypt ../../$(ENC_KEY_FILE).pem
	endif
endif

# Post build action to execute after main build job
post_build: $(OUT_CFG)/$(APP_NAME).bin
	$(info [POST_BUILD] - Executing post build script for $(APP_NAME))
	$(shell mv -f $(OUT_CFG)/$(APP_NAME).bin $(OUT_CFG)/$(APP_NAME)_unsigned.bin)
	$(PYTHON_PATH) $(IMGTOOL_PATH) $(SIGN_ARGS) -S $(SLOT_SIZE) -R $(ERASED_VALUE) $(UPGRADE_TYPE) -k keys/$(SIGN_KEY_FILE).pem $(OUT_CFG)/$(APP_NAME)_unsigned.bin $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex --hex-addr=$$(($(USER_APP_START)+$(HEADER_OFFSET)))
	$(GCC_PATH)/bin/arm-none-eabi-objdump -s $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex > $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).objdump

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
# Print debug information about all settings set in this file
ifeq ($(MAKEINFO) , 1)
$(info #### PSOC6.mk ####)
$(info USE_CRYPTO_HW: $(USE_CRYPTO_HW))
$(info USE_EXTERNAL_FLASH: $(USE_EXTERNAL_FLASH))
$(info USE_OVERWRITE: $(USE_OVERWRITE))
$(info MAX_IMG_SECTORS: $(PLATFORM_SUFFIX))
$(info IMAGE_1_SLOT_SIZE: $(FAMILY))
$(info IMAGE_2_SLOT_SIZE: $(PLATFORM))
$(info PLATFORM_DEFINES: $(PLATFORM_DEFINES))
endif
