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

# image type can be BOOT or UPGRADE
IMG_TYPES = BOOT UPGRADE

CUR_APP_PATH = $(PRJ_DIR)/$(APP_NAME)

# TODO: optimize here and in MCUBootApp.mk
# Output folder
OUT := $(APP_NAME)/out
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
	ENC_IMG_XIP := 0
else
	DEFINES_APP := -DUPGRADE_IMAGE
	DEFINES_APP += -DSWAP_DISABLED=$(USE_OVERWRITE)
endif

# Inherit platform default values for application
# if not set directly as make command argument
ifeq ($(USER_APP_START), )
USER_APP_START ?= $(PLATFORM_DEFAULT_USER_APP_START)
endif
ifeq ($(USER_IMG_START), )
USER_IMG_START ?= $(PLATFORM_DEFAULT_USER_IMG_START)
endif
ifeq ($(USER_APP_RAM_START), )
USER_APP_RAM_START ?= $(PLATFORM_DEFAULT_RAM_START)
endif
ifeq ($(USER_APP_RAM_SIZE), )
USER_APP_RAM_SIZE ?= $(PLATFORM_DEFAULT_RAM_SIZE)
endif
# Inherit platform default slot size in not set directly
ifeq ($(SLOT_SIZE), )
SLOT_SIZE ?= $(PLATFORM_DEFAULT_SLOT_SIZE)
endif

DEFINES_APP += -DUSER_APP_RAM_START=$(USER_APP_RAM_START)
DEFINES_APP += -DUSER_APP_RAM_SIZE=$(USER_APP_RAM_SIZE)
DEFINES_APP += -DUSER_APP_START=$(USER_APP_START)
DEFINES_APP += -DUSER_IMG_START=$(USER_IMG_START)
DEFINES_APP += -DUSER_APP_SIZE=$(SLOT_SIZE)
DEFINES_APP += $(PLATFORM_DEFINES_APP)

# Collect Test Application sources
SOURCES_APP_SRC := $(wildcard $(CUR_APP_PATH)/*.c)
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

SIGN_ARGS := $(PLATFORM_SIGN_ARGS)

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
	@$(CC) -E -x c $(CFLAGS) $(INCLUDE_DIRS) $(CUR_APP_PATH)/linker/$(APP_NAME)_$(CORE)_template.ld | grep -v '^#' >$(CUR_APP_PATH)/linker/$(APP_NAME).ld

###############################################################################
# Print debug inform ation about all settings set in this file
ifeq ($(MAKEINFO) , 1)
$(info #### BlinkyApp.mk ####)
$(info OUT_CFG: $(OUT_CFG))
$(info SWAP_UPGRADE: $(SWAP_UPGRADE))
$(info ERASED_VALUE: $(ERASED_VALUE))
$(info USER_APP_START: $(USER_APP_START))
$(info USER_APP_RAM_START: $(USER_APP_RAM_START))
$(info USER_RAM_RAM_SIZE: $(USER_APP_RAM_SIZE))
$(info SLOT_SIZE: $(SLOT_SIZE))
$(info DEFINES_APP: $(DEFINES_APP))
$(info SOURCES_APP: $(SOURCES_APP))
$(info INCLUDE_DIRS_APP: $(INCLUDE_DIRS_APP))
$(info LINKER_SCRIPT: $(LINKER_SCRIPT))
$(info ASM_FILES_APP: $(ASM_FILES_APP))
$(info SIGN_ARGS: $(SIGN_ARGS))
endif