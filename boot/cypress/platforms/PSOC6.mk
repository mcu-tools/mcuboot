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
CRYPTO_ACC_TYPE := MXCRYPTO

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
ifeq ($(PLATFORM), PSOC_062_2M)
# base kit CY8CPROTO-062-4343W
DEVICE ?= CY8C624ABZI_S2D44
PLATFORM_SUFFIX := 02
else ifeq ($(PLATFORM), PSOC_062_1M)
# base kit CY8CKIT-062-WIFI-BT
DEVICE ?= CY8C6247BZI_D54
PLATFORM_SUFFIX := 01
else ifeq ($(PLATFORM), PSOC_062_512K)
# base kit CY8CPROTO-062S3-4343W
DEVICE ?= CY8C6245LQI_S3D72
PLATFORM_SUFFIX := 03
else ifeq ($(PLATFORM), PSOC_063_1M)
# base kit CY8CPROTO-063-BLE
DEVICE ?= CYBLE_416045_02_device
PLATFORM_SUFFIX := 01
else ifeq ($(PLATFORM), PSOC_061_2M)
# FIXME!
DEVICE ?= CY8C614ABZI_S2F44
PLATFORM_SUFFIX := 02
else ifeq ($(PLATFORM), PSOC_061_512K)
DEVICE ?= CY8C6136BZI_F34
PLATFORM_SUFFIX := 03
USE_CRYPTO_HW := 0
endif

# Led default config
ifeq ($(PLATFORM), PSOC_062_512K)
LED_PORT_DEFAULT ?= GPIO_PRT11
LED_PIN_DEFAULT ?= 1U
else ifeq ($(PLATFORM), PSOC_062_1M)
LED_PORT_DEFAULT ?= GPIO_PRT13
LED_PIN_DEFAULT ?= 7U
else ifeq ($(PLATFORM), PSOC_062_2M)
LED_PORT_DEFAULT ?= GPIO_PRT13
LED_PIN_DEFAULT ?= 7U
else ifeq ($(PLATFORM), PSOC_063_1M)
LED_PORT_DEFAULT ?= GPIO_PRT6
LED_PIN_DEFAULT ?= 3U
else ifeq ($(PLATFORM), PSOC_061_2M)
LED_PORT_DEFAULT ?= GPIO_PRT13
LED_PIN_DEFAULT ?= 7U
else ifeq ($(PLATFORM), PSOC_061_512K)
LED_PORT_DEFAULT ?= GPIO_PRT13
LED_PIN_DEFAULT ?= 7U
endif

#UART default config
ifeq ($(PLATFORM), PSOC_062_512K)
UART_TX_DEFAULT ?= P10_1
UART_RX_DEFAULT ?= P10_0
else ifeq ($(PLATFORM), PSOC_061_512K)
# INFO: Since 061 platform development 
# is happening on processor module (PM),
# not dedicated kit, use port 10 for UART,
# since it is present on PM headers
# USE_CUSTOM_DEBUG_UART would use
# custom_debug_uart_cfg.h config in MCUBootApp
USE_CUSTOM_DEBUG_UART := 1
# Definitions for BlinkyApp
UART_TX_DEFAULT ?= P10_1
UART_RX_DEFAULT ?= P10_0

else
UART_TX_DEFAULT ?= P5_1
UART_RX_DEFAULT ?= P5_0
endif

DEFINES += -DUSE_SWAP_STATUS=1
DEFINES += -DCY_DEBUG_UART_TX=$(UART_TX_DEFAULT)
DEFINES += -DCY_DEBUG_UART_RX=$(UART_RX_DEFAULT)
DEFINES += -DCYBSP_DEBUG_UART_TX=$(UART_TX_DEFAULT)
DEFINES += -DCYBSP_DEBUG_UART_RX=$(UART_RX_DEFAULT)

# Add device name to defines
DEFINES += -D$(DEVICE)
DEFINES += -DCY_USING_HAL
DEFINES += -DCORE_NAME_$(CORE)_0=1
DEFINES += -DCOMPONENT_CAT1 
DEFINES += -DCOMPONENT_CAT1A
DEFINES += -DCOMPONENT_$(CORE)

# Minimum erase size of underlying memory hardware
PLATFORM_MEMORY_ALIGN := 0x200
PLATFORM_MAX_TRAILER_PAGE_SIZE := 0x200

# Default upgrade method
PLATFORM_DEFAULT_USE_OVERWRITE ?= 0

# Default chung size
PLATFORM_CHUNK_SIZE := 512U

###############################################################################
# Application specific libraries
###############################################################################
# MCUBootApp
###############################################################################
THIS_APP_PATH = $(PRJ_DIR)/libs

ifeq ($(APP_NAME), MCUBootApp)

CORE ?= CM0P
ifeq ($(CORE), CM0P)
CORE_SUFFIX = m0plus
else
CORE_SUFFIX = m4
endif

C_FILES += $(PRJ_DIR)/platforms/utils/$(FAMILY)/platform_utils.c
INCLUDE_DIRS += $(PRJ_DIR)/platforms/utils/$(FAMILY)
###############################################################################
# Application dependent definitions
# MCUBootApp default settings

USE_CRYPTO_HW ?= 1
MCUBOOT_DEPENDENCY_CHECK ?= 1
###############################################################################

ifeq ($(PLATFORM), $(filter $(PLATFORM), PSOC_061_2M PSOC_061_1M PSOC_061_512K))
# FIXME: not needed for real PSoC 61!
C_FILES += $(PRJ_DIR)/platforms/utils/$(FAMILY)/psoc6_02_cm0p_sleep.c
endif

ifeq ($(USE_SMIF_CONFIG), 1)
	ifeq ($(USE_EXTERNAL_FLASH), 1)
		C_FILES += cy_serial_flash_prog.c
		C_FILES += $(PRJ_DIR)/platforms/memory/PSOC6/smif_cfg_dbg/cycfg_qspi_memslot.c
		INCLUDE_DIRS += $(PRJ_DIR)/platforms/memory/PSOC6/smif_cfg_dbg
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

CORE := $(APP_CORE)
ifeq ($(CORE), CM0P)
CORE_SUFFIX = m0plus
else
CORE_SUFFIX = m4
endif

ifeq ($(USE_EXTERNAL_FLASH), 1)
PLATFORM_DEFAULT_ERASED_VALUE := 0xff
else
PLATFORM_DEFAULT_ERASED_VALUE := 0
endif

# Define start of application, RAM start and size, slot size
ifeq ($(PLATFORM), $(filter $(PLATFORM), PSOC_061_2M PSOC_062_2M))
ifeq ($(USE_XIP), 1)
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0xE0000
else
	PLATFORM_DEFAULT_RAM_START ?= 0x08040000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
else ifeq ($(PLATFORM), $(filter $(PLATFORM), PSOC_061_1M PSOC_062_1M))
ifeq ($(USE_XIP), 1)
	PLATFORM_DEFAULT_RAM_START ?= 0x08000800
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x47800
else
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
else ifeq ($(PLATFORM), $(filter $(PLATFORM), PSOC_061_512K PSOC_062_512K))
ifeq ($(USE_XIP), 1)
	PLATFORM_DEFAULT_RAM_START ?= 0x08000800
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x3F800
else
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
else ifeq ($(PLATFORM), PSOC_063_1M)
	PLATFORM_DEFAULT_RAM_START ?= 0x08020000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000
endif
# Default start address of application (boot)
PLATFORM_USER_APP_START ?= $(PRIMARY_IMG_START)
# For PSOC6 platform PRIMARY_IMG_START start is the same as USER_APP_START
# This parameter can be different in cases when code is resided in
# flash mapped to one address range, but executed using different bus
# for access with another address range. For example, execution of code
# from external memory in XIP mode.
PLATFORM_DEFAULT_PRIMARY_IMG_START ?= $(PLATFORM_DEFAULT_USER_APP_START)

# We still need this for MCUBoot apps signing
IMGTOOL_PATH ?=	../../scripts/imgtool.py

PLATFORM_DEFAULT_IMG_VER_ARG ?= 1.0.0

PLATFORM_SIGN_ARGS := sign --header-size 1024 --pad-header --align 8 -M 512

# Set parameters needed for signing
ifeq ($(IMG_TYPE), UPGRADE)
	# Use encryption and random initial vector for image
	ifeq ($(ENC_IMG), 1)
		PLATFORM_SIGN_ARGS += --encrypt ../../$(ENC_KEY_FILE).pem
		PLATFORM_SIGN_ARGS += --use-random-iv
	endif
endif

pre_build:
	$(info [PRE_BUILD] - Generating linker script for application $(CUR_APP_PATH)/linker/$(APP_NAME).ld)
	@$(CC) -E -x c $(CFLAGS) $(INCLUDE_DIRS) $(CUR_APP_PATH)/linker/$(APP_NAME)_$(CORE)_template$(LD_SUFFIX).ld | grep -v '^#' >$(CUR_APP_PATH)/linker/$(APP_NAME).ld

# Post build action to execute after main build job
post_build: $(OUT_CFG)/$(APP_NAME).bin
ifeq ($(POST_BUILD_ENABLE), 1)
	$(info [POST BUILD] - Executing post build script for $(APP_NAME))
	$(shell mv -f $(OUT_CFG)/$(APP_NAME).bin $(OUT_CFG)/$(APP_NAME)_unsigned.bin)
	$(PYTHON_PATH) $(IMGTOOL_PATH) $(SIGN_ARGS) $(BOOT_RECORD) -S $(SLOT_SIZE) -R $(ERASED_VALUE) $(UPGRADE_TYPE) -k keys/$(SIGN_KEY_FILE).pem $(OUT_CFG)/$(APP_NAME)_unsigned.bin $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex --hex-addr=$(HEADER_OFFSET)
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
PLATFORM_STARTUP_FILE := $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system/COMPONENT_$(CORE)/TOOLCHAIN_$(COMPILER)/startup_psoc6_$(PLATFORM_SUFFIX)_c$(CORE_SUFFIX).S

INCLUDE_DIRS += $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system
