################################################################################
# \file BlinkyApp.mk
# \version 1.0
#
# \brief
# Makefile to describe demo application BlinkyApp for Cypress MCUBoot based applications.
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

# Cypress' MCUBoot Application supports GCC ARM only at this moment
# Set defaults to:
#     - compiler GCC
#     - build configuration to Debug
#     - image type to BOOT
COMPILER ?= GCC_ARM
IMG_TYPE ?= BOOT
IMG_ID ?= 1

# image type can be BOOT or UPGRADE
IMG_TYPES = BOOT UPGRADE

CUR_APP_PATH = $(PRJ_DIR)/$(APP_NAME)

ifneq ($(FLASH_MAP), )
$(CUR_APP_PATH)/flashmap.mk:
	$(PYTHON_PATH) scripts/flashmap.py -p $(PLATFORM) -m -i $(FLASH_MAP) -o $(PRJ_DIR)/platforms/cy_flash_pal/cy_flash_map.h -d $(IMG_ID) > $(CUR_APP_PATH)/flashmap.mk
include $(CUR_APP_PATH)/flashmap.mk
DEFINES_APP := -DCY_FLASH_MAP_JSON
endif

# TODO: optimize here and in MCUBootApp.mk
# Output folder
ifeq ($(IMG_ID), 1)
        OUT := $(APP_NAME)/out
else
        OUT := $(APP_NAME)/out.id$(IMG_ID)
endif

# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(PLATFORM)

OUT_CFG := $(OUT_TARGET)/$(BUILDCFG)

# Set build directory for BOOT and UPGRADE images
ifeq ($(IMG_TYPE), UPGRADE)
	OUT_CFG := $(OUT_CFG)/upgrade
else
	OUT_CFG := $(OUT_CFG)/boot
endif

# Set parameters needed for signing
ifeq ($(IMG_TYPE), UPGRADE)
	UPGRADE_SUFFIX :=_upgrade
endif

include $(PRJ_DIR)/platforms.mk
include $(PRJ_DIR)/common_libs.mk
include $(PRJ_DIR)/toolchains.mk

# use USE_OVERWRITE = 1 for overwrite only mode
# use USE_OVERWRITE = 0 for swap upgrade mode
ifeq ($(USE_OVERWRITE), )
USE_OVERWRITE ?= $(PLATFORM_DEFAULT_USE_OVERWRITE)
endif

# possible values are 0 and 0xff
# internal Flash by default
ifeq ($(ERASED_VALUE), )
ERASED_VALUE ?= $(PLATFORM_DEFAULT_ERASED_VALUE)
endif

# Application-specific DEFINES
ifeq ($(IMG_TYPE), BOOT)
	DEFINES_APP := -DBOOT_IMAGE
	ENC_IMG := 0
else
	DEFINES_APP := -DUPGRADE_IMAGE
	DEFINES_APP += -DSWAP_DISABLED=$(USE_OVERWRITE)
endif

# Inherit platform default values for application start
# if not set directly as make command argument
# App start may vary, depending on mode of operation
# for example in XIP mode image start adress and app start
# address may be different
USER_APP_START ?= $(PLATFORM_USER_APP_START)

ifeq ($(USER_APP_RAM_START), )
USER_APP_RAM_START ?= $(PLATFORM_DEFAULT_RAM_START)
endif
ifeq ($(USER_APP_RAM_SIZE), )
USER_APP_RAM_SIZE ?= $(PLATFORM_DEFAULT_RAM_SIZE)
endif

DEFINES_APP += -DUSER_APP_RAM_START=$(USER_APP_RAM_START)
DEFINES_APP += -DUSER_APP_RAM_SIZE=$(USER_APP_RAM_SIZE)
DEFINES_APP += -DUSER_APP_START=$(USER_APP_START)
DEFINES_APP += -DPRIMARY_IMG_START=$(PRIMARY_IMG_START)
DEFINES_APP += -DUSER_APP_SIZE=$(SLOT_SIZE)
DEFINES_APP += -DAPP_$(APP_CORE)
DEFINES_APP += $(PLATFORM_DEFINES_APP)

#Use default led if no command line parameter added
ifeq ($(LED_PORT), )
DEFINES_APP += -DLED_PORT=$(LED_PORT_DEFAULT)
else
DEFINES_APP += -DLED_PORT=GPIO_PRT$(LED_PORT)
endif

ifeq ($(LED_PIN), )
DEFINES_APP += -DLED_PIN=$(LED_PIN_DEFAULT)
else
DEFINES_APP += -DLED_PIN=$(LED_PIN)
endif

#Use default UART if no command line parameter added
ifeq ($(UART_TX), )
DEFINES_APP += -DCY_DEBUG_UART_TX=$(UART_TX_DEFAULT)
else
DEFINES_APP += -DCY_DEBUG_UART_TX=$(UART_TX)
endif

ifeq ($(UART_RX), )
DEFINES_APP += -DCY_DEBUG_UART_RX=$(UART_RX_DEFAULT)
else
DEFINES_APP += -DCY_DEBUG_UART_RX=$(UART_RX)
endif

ifeq ($(USE_XIP), 1)
DEFINES_APP += -DUSE_XIP
DEFINES_APP += -DCY_BOOT_USE_EXTERNAL_FLASH
LD_SUFFIX = _xip
endif

# Add version metadata to image
ifneq ($(IMG_VER), )
IMG_VER_ARG = -v "$(IMG_VER)"
DEFINES_APP += -DIMG_VER_MSG=\"$(IMG_VER)\"
else
IMG_VER_ARG = -v "$(PLATFORM_DEFAULT_IMG_VER_ARG)"
DEFINES_APP += -DIMG_VER_MSG=\"$(PLATFORM_DEFAULT_IMG_VER_ARG)\"
$(info WARNING - setting platform default version number, to set custom value - pass IMG_VER=x.x.x argument to make command)
endif

# Add dependencies metadata to image
ifneq ($(IMG_DEPS_ID), )
ifneq ($(IMG_DEPS_VER), )
IMG_DEPS_ARG = -d "($(IMG_DEPS_ID), $(IMG_DEPS_VER))"
endif
endif

# Collect Test Application sources
SOURCES_APP_SRC := $(wildcard $(CUR_APP_PATH)/*.c)

# Set offset for secondary image
ifeq ($(IMG_TYPE), UPGRADE)
HEADER_OFFSET := $(SECONDARY_IMG_START)
else
HEADER_OFFSET := $(PRIMARY_IMG_START)
endif

# Collect all the sources
SOURCES_APP += $(SOURCES_APP_SRC)
SOURCES_APP += $(PLATFORM_SOURCES_FLASH)

# Collect includes for BlinkyApp
INCLUDE_DIRS_APP := $(addprefix -I, $(CURDIR))
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH))
INCLUDE_DIRS_APP += $(addprefix -I, $(PLATFORM_INCLUDE_DIRS_FLASH))

# ++++
INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/MCUBootApp/config)
INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/MCUBootApp)
INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/../bootutil/include)
INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/../bootutil/src)
INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/../bootutil/include/bootutil)
# +++

# Include confirmation flag setting (img_ok) implementation
ifeq ($(IMG_TYPE), UPGRADE)
ifeq ($(USE_OVERWRITE), 0)
SOURCES_APP_SRC += $(PRJ_DIR)/platforms/img_confirm/$(FAMILY)/set_img_ok.c
INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR)/platforms/img_confirm)
endif
endif

# Overwite path to linker script if custom is required, otherwise default from BSP is used
ifeq ($(COMPILER), GCC_ARM)
LINKER_SCRIPT := $(CUR_APP_PATH)/linker/$(APP_NAME).ld
else
$(error Only GCC ARM is supported at this moment)
endif

ASM_FILES_APP :=
ASM_FILES_APP += $(ASM_FILES_STARTUP)

# add flag to imgtool if not using swap for upgrade
ifeq ($(USE_OVERWRITE), 1)
UPGRADE_TYPE := --overwrite-only
endif

ifeq ($(BOOT_RECORD_SW_TYPE), )
	ifeq ($(IMG_TYPE), BOOT)
		BOOT_RECORD_IMG_TYPE_STR = B_Blinky$(IMG_ID)
	else
		BOOT_RECORD_IMG_TYPE_STR = U_Blinky$(IMG_ID)
	endif
	BOOT_RECORD := --boot-record $(BOOT_RECORD_IMG_TYPE_STR)
else
	BOOT_RECORD := --boot-record $(BOOT_RECORD_SW_TYPE)
endif

SIGN_ARGS := $(PLATFORM_SIGN_ARGS) $(IMG_VER_ARG) $(IMG_DEPS_ARG)

# Set parameters needed for signing
ifeq ($(IMG_TYPE), UPGRADE)
	# Set img_ok flag to trigger swap type permanent
	ifeq ($(CONFIRM), 1)
		SIGN_ARGS += --confirm
	endif
	SIGN_ARGS += --pad
endif

$(info $(SIGN_ARGS))

pre_build:
	$(info [PRE_BUILD] - Generating linker script for application $(CUR_APP_PATH)/linker/$(APP_NAME).ld)
	@$(CC) -E -x c $(CFLAGS) $(INCLUDE_DIRS) $(CUR_APP_PATH)/linker/$(APP_NAME)_$(CORE)_template$(LD_SUFFIX).ld | grep -v '^#' >$(CUR_APP_PATH)/linker/$(APP_NAME).ld

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### BlinkyApp.mk ####)
$(info APP_CORE <-- $(APP_CORE))
$(info APP_NAME <-- $(APP_NAME))
$(info ASM_FILES_APP --> $(ASM_FILES_APP))
$(info ASM_FILES_STARTUP <-- $(ASM_FILES_STARTUP))
$(info BOOT_RECORD --> $(BOOT_RECORD))
$(info BOOT_RECORD_IMG_TYPE_STR <-- $(BOOT_RECORD_IMG_TYPE_STR))
$(info BOOT_RECORD_SW_TYPE <-- $(BOOT_RECORD_SW_TYPE))
$(info BUILDCFG <-- $(BUILDCFG))
$(info COMPILER <-> $(COMPILER))
$(info CONFIRM <-- $(CONFIRM))
$(info CURDIR <-- $(CURDIR))
$(info CUR_APP_PATH <-- $(CUR_APP_PATH))
$(info DEFINES_APP --> $(DEFINES_APP))
$(info ENC_IMG --> $(ENC_IMG))
$(info ERASED_VALUE <-> $(ERASED_VALUE))
$(info FAMILY <-- $(FAMILY))
$(info FLASH_MAP <-- $(FLASH_MAP))
$(info HEADER_OFFSET --> $(HEADER_OFFSET))
$(info IMG_DEPS_ARG <-- $(IMG_DEPS_ARG))
$(info IMG_DEPS_ID <-- $(IMG_DEPS_ID))
$(info IMG_DEPS_VER <-- $(IMG_DEPS_VER))
$(info IMG_ID <-> $(IMG_ID))
$(info IMG_TYPE <-> $(IMG_TYPE))
$(info IMG_VER <-- $(IMG_VER))
$(info IMG_VER_ARG <-- $(IMG_VER_ARG))
$(info INCLUDE_DIRS_APP --> $(INCLUDE_DIRS_APP))
$(info LED_PIN <-- $(LED_PIN))
$(info LED_PIN_DEFAULT <-- $(LED_PIN_DEFAULT))
$(info LED_PORT <-- $(LED_PORT))
$(info LED_PORT_DEFAULT <-- $(LED_PORT_DEFAULT))
$(info LINKER_SCRIPT --> $(LINKER_SCRIPT))
$(info OUT <-> $(OUT))
$(info OUT_CFG <-> $(OUT_CFG))
$(info OUT_TARGET <-> $(OUT_TARGET))
$(info PLATFORM <-- $(PLATFORM))
$(info PLATFORM_DEFAULT_ERASED_VALUE <-- $(PLATFORM_DEFAULT_ERASED_VALUE))
$(info PLATFORM_DEFAULT_IMG_VER_ARG <-- $(PLATFORM_DEFAULT_IMG_VER_ARG))
$(info PLATFORM_DEFAULT_RAM_SIZE <-- $(PLATFORM_DEFAULT_RAM_SIZE))
$(info PLATFORM_DEFAULT_RAM_START <-- $(PLATFORM_DEFAULT_RAM_START))
$(info PLATFORM_DEFAULT_USE_OVERWRITE <-- $(PLATFORM_DEFAULT_USE_OVERWRITE))
$(info PLATFORM_DEFINES_APP <-- $(PLATFORM_DEFINES_APP))
$(info PLATFORM_INCLUDE_DIRS_FLASH <-- $(PLATFORM_INCLUDE_DIRS_FLASH))
$(info PLATFORM_SIGN_ARGS <-- $(PLATFORM_SIGN_ARGS))
$(info PLATFORM_SOURCES_FLASH <-- $(PLATFORM_SOURCES_FLASH))
$(info PLATFORM_USER_APP_START <-- $(PLATFORM_USER_APP_START))
$(info PRIMARY_IMG_START <-- $(PRIMARY_IMG_START))
$(info PRJ_DIR <-- $(PRJ_DIR))
$(info PYTHON_PATH <-- $(PYTHON_PATH))
$(info SECONDARY_IMG_START <-- $(SECONDARY_IMG_START))
$(info SIGN_ARGS <-> $(SIGN_ARGS))
$(info SLOT_SIZE <-- $(SLOT_SIZE))
$(info SOURCES_APP --> $(SOURCES_APP))
$(info SOURCES_APP_SRC <-> $(SOURCES_APP_SRC))
$(info UART_RX <-- $(UART_RX))
$(info UART_RX_DEFAULT <-- $(UART_RX_DEFAULT))
$(info UART_TX <-- $(UART_TX))
$(info UART_TX_DEFAULT <-- $(UART_TX_DEFAULT))
$(info UPGRADE_SUFFIX --> $(UPGRADE_SUFFIX))
$(info UPGRADE_TYPE --> $(UPGRADE_TYPE))
$(info USER_APP_RAM_SIZE <-> $(USER_APP_RAM_SIZE))
$(info USER_APP_RAM_START <-> $(USER_APP_RAM_START))
$(info USER_APP_START <-> $(USER_APP_START))
$(info USE_OVERWRITE <-> $(USE_OVERWRITE))
$(info USE_XIP <-- $(USE_XIP))
endif
