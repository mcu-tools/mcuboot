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

# Cypress' MCUBoot Application supports GCC ARM only at this moment
# Set defaults to:
#     - compiler GCC
#     - build configuration to Debug
#     - image type to BOOT
COMPILER ?= GCC_ARM
IMG_TYPE ?= BOOT

# image type can be BOOT or UPGRADE
IMG_TYPES = BOOT UPGRADE

# use SWAP_UPGRADE = 0 for overwrite only mode
# use SWAP_UPGRADE = 1 for swap upgrade mode
# use SWAP_UPGRADE = 2 for swap upgrade mode, upgrade image is in the external memory
SWAP_UPGRADE ?= 1

# possible values are 0 and 0xff
# internal Flash by default
ERASED_VALUE ?= 0

ifneq ($(COMPILER), GCC_ARM)
$(error Only GCC ARM is supported at this moment)
endif

CUR_APP_PATH = $(CURDIR)/$(APP_NAME)

include $(CUR_APP_PATH)/platforms.mk
include $(CUR_APP_PATH)/libs.mk
include $(CUR_APP_PATH)/toolchains.mk

# Application-specific DEFINES
ifeq ($(IMG_TYPE), BOOT)
	DEFINES_APP := -DBOOT_IMG
else
	DEFINES_APP := -DUPGRADE_IMG
	DEFINES_APP += -DSWAP_ENABLED=$(SWAP_UPGRADE)
endif

# Define start of application, RAM start and size, slot size
ifeq ($(PLATFORM), PSOC_062_2M)
	DEFINES_APP += -DRAM_START=0x08020800
	DEFINES_APP += -DRAM_SIZE=0x20000
	DEFINES_APP += -DUSER_APP_START=0x10018000
	# size of image slot
ifeq ($(SWAP_UPGRADE), 2)
	SLOT_SIZE ?= 0xC0000
else
	SLOT_SIZE ?= 0x10000
endif
endif

DEFINES_APP += -DUSER_APP_SIZE=$(SLOT_SIZE)

# Collect Test Application sources
SOURCES_APP_SRC := $(wildcard $(CUR_APP_PATH)/*.c)
# Collect all the sources
SOURCES_APP += $(SOURCES_APP_SRC)

# Collect includes for BlinkyApp
INCLUDE_DIRS_APP := $(addprefix -I, $(CURDIR))
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH))

# Overwite path to linker script if custom is required, otherwise default from BSP is used
ifeq ($(COMPILER), GCC_ARM)
LINKER_SCRIPT := $(subst /cygdrive/c,c:,$(CUR_APP_PATH)/linker/$(APP_NAME).ld)
else
$(error Only GCC ARM is supported at this moment)
endif

ASM_FILES_APP :=

# We still need this for MCUBoot apps signing
IMGTOOL_PATH ?=	../../scripts/imgtool.py

# add flag to imgtool if not using swap for upgrade
ifeq ($(SWAP_UPGRADE), 0)
UPGRADE_TYPE := --overwrite-only
endif

SIGN_ARGS := sign --header-size 1024 --pad-header --align 8 -v "2.0" -S $(SLOT_SIZE) -M 512 $(UPGRADE_TYPE) -R $(ERASED_VALUE) -k keys/$(SIGN_KEY_FILE).pem

# Output folder
OUT := $(APP_NAME)/out
# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(PLATFORM)

OUT_CFG := $(OUT_TARGET)/$(BUILDCFG)

# Set build directory for BOOT and UPGRADE images
ifeq ($(IMG_TYPE), UPGRADE)
	SIGN_ARGS += --pad
	UPGRADE_SUFFIX :=_upgrade
	OUT_CFG := $(OUT_CFG)/upgrade
else
	OUT_CFG := $(OUT_CFG)/boot
endif

pre_build:
	$(info [PRE_BUILD] - Generating linker script for application $(CUR_APP_PATH)/linker/$(APP_NAME).ld)
	@$(CC) -E -x c $(CFLAGS) $(INCLUDE_DIRS) $(CUR_APP_PATH)/linker/$(APP_NAME)_template.ld | grep -v '^#' >$(CUR_APP_PATH)/linker/$(APP_NAME).ld

# Post build action to execute after main build job
post_build: $(OUT_CFG)/$(APP_NAME).hex
	$(info [POST_BUILD] - Executing post build script for $(APP_NAME))
	mv -f $(OUT_CFG)/$(APP_NAME).hex $(OUT_CFG)/$(APP_NAME)_unsigned.hex
	$(PYTHON_PATH) $(IMGTOOL_PATH) $(SIGN_ARGS) $(OUT_CFG)/$(APP_NAME)_unsigned.hex $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex
