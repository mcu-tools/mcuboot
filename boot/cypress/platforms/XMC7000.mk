################################################################################
# \file XMC7000.mk
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
PDL_CAT_SUFFIX := 1C
CRYPTO_ACC_TYPE := MXCRYPTO

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
ifeq ($(PLATFORM), XMC7200)
# base kit KIT_XMC72_EVK
DEVICE ?= XMC7200D_E272K8384
else ifeq ($(PLATFORM), XMC7100)
DEVICE ?= XMC7100D_E272K4160
endif

# Led default config
ifeq ($(PLATFORM), XMC7200)
LED_PORT_DEFAULT ?= GPIO_PRT16
LED_PIN_DEFAULT ?= 1U
else ifeq ($(PLATFORM), XMC7100)
LED_PORT_DEFAULT ?= GPIO_PRT16
LED_PIN_DEFAULT ?= 1U
endif

#UART default config
ifeq ($(PLATFORM), XMC7200)
UART_TX_DEFAULT ?= P13_1
UART_RX_DEFAULT ?= P13_0
else ifeq ($(PLATFORM), XMC7100)
UART_TX_DEFAULT ?= P13_1
UART_RX_DEFAULT ?= P13_0
endif

USE_SWAP_STATUS ?= 1

ifeq ($(USE_SWAP_STATUS), 1)
DEFINES += -DUSE_SWAP_STATUS=1
endif

ifeq ($(USE_INTERNAL_FLASH_CODE_LARGE), 1)
DEFINES += -DUSE_INTERNAL_FLASH_CODE_LARGE
endif

ifeq ($(USE_INTERNAL_FLASH_CODE_SMALL), 1)
DEFINES += -DUSE_INTERNAL_FLASH_CODE_SMALL
endif

ifeq ($(USE_INTERNAL_FLASH_WORK_LARGE), 1)
DEFINES += -DUSE_INTERNAL_FLASH_WORK_LARGE
endif

ifeq ($(USE_INTERNAL_FLASH_WORK_SMALL), 1)
DEFINES += -DUSE_INTERNAL_FLASH_WORK_SMALL
endif

# Add device name to defines
DEFINES += -D$(DEVICE)

# Default upgrade method
PLATFORM_DEFAULT_USE_OVERWRITE ?= 0

PLATFORM_CHUNK_SIZE := 0x200

# Minimum erase size of underlying memory hardware
PLATFORM_MEMORY_ALIGN := 0x200
PLATFORM_MAX_TRAILER_PAGE_SIZE := 0x8000

###############################################################################
# Application specific libraries
###############################################################################
# MCUBootApp
###############################################################################
THIS_APP_PATH = $(PRJ_DIR)/libs

APP_CORE ?= CM7
CORE ?= CM0P
CORE_ID ?= 0
APP_CORE_ID ?= 0

ifeq ($(APP_NAME), MCUBootApp)

ifeq ($(CORE), CM0P)
CORE_SUFFIX = m0plus
else
CORE_SUFFIX = m7
C_FILES += $(PRJ_DIR)/platforms/BSP/XMC7000/system/COMPONENT_XMC7x_CM0P_SLEEP/xmc7200_cm0p_sleep.c
endif

###############################################################################
# Application dependent definitions
# MCUBootApp default settings

USE_CRYPTO_HW ?= 0
###############################################################################

# Platform dependend utils files
C_FILES += $(PRJ_DIR)/platforms/utils/$(FAMILY)/platform_utils.c
INCLUDE_DIRS += $(PRJ_DIR)/platforms/utils/$(FAMILY)

ifeq ($(USE_SECURE_MODE), 1)
C_FILES += $(PRJ_DIR)/platforms/utils/$(FAMILY)/cy_si_config.c
C_FILES += $(PRJ_DIR)/platforms/utils/$(FAMILY)/cy_si_key.c
endif

# Post build job to execute for platform
post_build: $(OUT_CFG)/$(APP_NAME)_unsigned.hex
ifeq ($(POST_BUILD_ENABLE), 1)
	$(info [POST BUILD] - Executing post build script for $(APP_NAME))

ifeq ($(USE_SECURE_MODE), 1)
	cysecuretools -t $(PLATFORM) sign-cysaf -i $(OUT_CFG)/$(APP_NAME).elf --key-path ./keys/$(SECURE_MODE_KEY_NAME).pem --output $(OUT_CFG)/$(APP_NAME)_secure.elf; \
	$(GCC_PATH)/bin/arm-none-eabi-objcopy -O ihex $(OUT_APP)/$(APP_NAME)_secure.elf $(OUT_APP)/$(APP_NAME)_secure.hex; \
	$(GCC_PATH)/bin/arm-none-eabi-objcopy $(OUT_APP)/$(APP_NAME)_secure.elf -S -O binary $(OUT_APP)/$(APP_NAME)_secure.bin --remove-section .cy_sflash_user_data --remove-section .cy_toc_part2; \
	mv -f $(OUT_APP)/$(APP_NAME)_secure.elf $(OUT_APP)/$(APP_NAME).elf; \
	mv -f $(OUT_APP)/$(APP_NAME)_secure.hex $(OUT_APP)/$(APP_NAME).hex; \
	mv -f $(OUT_APP)/$(APP_NAME)_secure.bin $(OUT_APP)/$(APP_NAME).bin

else
	cp -f $(OUT_CFG)/$(APP_NAME)_unsigned.hex $(OUT_CFG)/$(APP_NAME).hex
endif
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
LDFLAGS_PLATFORM := -Wl,--defsym=_CORE_$(CORE)_$(CORE_ID)_=1
ifeq ($(CORE), CM0P)
CORE_SUFFIX = m0plus
else
CORE_SUFFIX = m7
endif

PLATFORM_DEFAULT_ERASED_VALUE := 0xff

# Define start of application, RAM start and size, slot size
ifeq ($(FAMILY), XMC7000)
	PLATFORM_DEFAULT_RAM_START ?= 0x28050000
	PLATFORM_DEFAULT_RAM_SIZE  ?= 0x30000
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

PLATFORM_SIGN_ARGS := --header-size 1024 --align 8

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
	cysecuretools -t $(PLATFORM) sign-image $(SIGN_ARGS) -S $(SLOT_SIZE) -R $(ERASED_VALUE) $(UPGRADE_TYPE) --key-path keys/$(SIGN_KEY_FILE).pem --image $(OUT_CFG)/$(APP_NAME)_unsigned.bin --output $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex --hex-addr=$(HEADER_OFFSET)
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
ifeq ($(CORE), CM0P)
PLATFORM_SYSTEM_FILE_NAME := system_cm0plus.c
PLATFORM_STARTUP_FILE := $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system/COMPONENT_$(CORE)/TOOLCHAIN_$(COMPILER)/startup_cm0plus.S
else ifeq ($(CORE), CM7)
PLATFORM_SYSTEM_FILE_NAME := system_cm7.c
PLATFORM_STARTUP_FILE := $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system/COMPONENT_$(CORE)/TOOLCHAIN_$(COMPILER)/startup_cm7.S
endif

INCLUDE_DIRS += $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system
INCLUDE_DIRS += $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system/COMPONENT_$(CORE)

DEFINES += -DCY_USING_HAL
DEFINES += -DCOMPONENT_$(CORE)
DEFINES += -DCOMPONENT_$(CORE)_$(CORE_ID)
DEFINES += -DCORE_NAME_$(CORE)_$(CORE_ID)=1
DEFINES += -DCOMPONENT_CAT1
DEFINES += -DCOMPONENT_CAT1C
DEFINES += -DCOMPONENT_CAT1C8M

