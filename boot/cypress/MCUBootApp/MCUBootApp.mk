################################################################################
# \file MCUBootApp.mk
# \version 1.0
#
# \brief
# Makefile for Cypress MCUBoot-based application.
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

APP_NAME := MCUBootApp

# Cypress' MCUBoot Application supports GCC ARM only at this moment
# Set default compiler to GCC if not specified from command line
COMPILER ?= GCC_ARM

MCUBOOT_IMAGE_NUMBER ?= 1
ENC_IMG ?= 0
USE_BOOTSTRAP ?= 1
MCUBOOT_LOG_LEVEL ?= MCUBOOT_LOG_LEVEL_DEBUG

ifneq ($(COMPILER), GCC_ARM)
$(error Only GCC ARM is supported at this moment)
endif

CUR_APP_PATH = $(PRJ_DIR)/$(APP_NAME)

# Output folder
OUT := $(APP_NAME)/out
# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(PLATFORM)

OUT_CFG := $(OUT_TARGET)/$(BUILDCFG)

include $(PRJ_DIR)/platforms.mk
include $(PRJ_DIR)/common_libs.mk
include $(PRJ_DIR)/toolchains.mk

# default slot size is 0x10000, 512bytes per row/sector, so 128 sectors

# These parameters can be set from command line as well
ifeq ($(MAX_IMG_SECTORS), )
MAX_IMG_SECTORS ?= $(PLATFORM_MAX_IMG_SECTORS)
endif
ifeq ($(IMAGE_1_SLOT_SIZE), )
IMAGE_1_SLOT_SIZE ?= $(PLATFORM_IMAGE_1_SLOT_SIZE)
endif
ifeq ($(MCUBOOT_IMAGE_NUMBER), 2)
ifeq ($(IMAGE_2_SLOT_SIZE), )
IMAGE_2_SLOT_SIZE ?= $(PLATFORM_IMAGE_2_SLOT_SIZE)
endif
endif

# Application-specific DEFINES
DEFINES_APP := -DMBEDTLS_CONFIG_FILE="\"mcuboot_crypto_config.h\""
DEFINES_APP += -DECC256_KEY_FILE="\"keys/$(SIGN_KEY_FILE).pub\""
DEFINES_APP += -DCORE=$(CORE)
DEFINES_APP += -DMCUBOOT_IMAGE_NUMBER=$(MCUBOOT_IMAGE_NUMBER)

# Define MCUboot size and pass it to linker script
BOOTLOADER_SIZE ?= $(PLATFORM_BOOTLOADER_SIZE)
LDFLAGS_DEFSYM  += -Wl,--defsym,BOOTLOADER_SIZE=$(BOOTLOADER_SIZE)

# If this flag is set, user can pass custom values
ifeq ($(USE_CUSTOM_MEMORY_MAP), 1)

STATUS_PARTITION_OFFSET ?= $(PLATFORM_STATUS_PARTITION_OFFSET)
SCRATCH_SIZE ?= $(PLATFORM_SCRATCH_SIZE)
EXTERNAL_FLASH_SCRATCH_OFFSET ?= $(PLATFORM_EXTERNAL_FLASH_SCRATCH_OFFSET)
EXTERNAL_FLASH_SECONDARY_1_OFFSET ?= $(PLATFORM_EXTERNAL_FLASH_SECONDARY_1_OFFSET)
EXTERNAL_FLASH_SECONDARY_2_OFFSET ?= $(PLATFORM_EXTERNAL_FLASH_SECONDARY_2_OFFSET)

DEFINES_APP += -DCY_BOOT_BOOTLOADER_SIZE=$(BOOTLOADER_SIZE)
# status values
DEFINES_APP += -DSWAP_STATUS_PARTITION_OFF=$(STATUS_PARTITION_OFFSET)
# scratch values
DEFINES_APP += -DCY_BOOT_SCRATCH_SIZE=$(SCRATCH_SIZE)
DEFINES_APP += -DCY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET=$(EXTERNAL_FLASH_SCRATCH_OFFSET)
DEFINES_APP += -DCY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET=$(EXTERNAL_FLASH_SECONDARY_1_OFFSET)
ifeq ($(MCUBOOT_IMAGE_NUMBER), 2)
DEFINES_APP += -DCY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET=$(EXTERNAL_FLASH_SECONDARY_2_OFFSET)
endif

endif # USE_CUSTOM_MEMORY_MAP

APP_DEFAULT_POLICY ?= $(PLATFORM_APP_DEFAULT_POLICY)

ifeq ($(USE_EXTERNAL_FLASH), 1)
DEFINES_APP += -DCY_BOOT_USE_EXTERNAL_FLASH
DEFINES_APP += -DCY_MAX_EXT_FLASH_ERASE_SIZE=$(PLATFORM_CY_MAX_EXT_FLASH_ERASE_SIZE)
endif
DEFINES_APP += -DCY_BOOT_IMAGE_1_SIZE=$(IMAGE_1_SLOT_SIZE)
ifeq ($(MCUBOOT_IMAGE_NUMBER), 2)
DEFINES_APP += -DCY_BOOT_IMAGE_2_SIZE=$(IMAGE_2_SLOT_SIZE)
endif
ifeq ($(USE_OVERWRITE), 1)
DEFINES_APP += -DMCUBOOT_OVERWRITE_ONLY
else
ifeq ($(USE_BOOTSTRAP), 1)
DEFINES_APP += -DMCUBOOT_BOOTSTRAP
endif
endif
DEFINES_APP += -DMCUBOOT_MAX_IMG_SECTORS=$(MAX_IMG_SECTORS)
DEFINES_APP += -DMCUBOOT_LOG_LEVEL=$(MCUBOOT_LOG_LEVEL)

# Hardrware acceleration support
ifeq ($(USE_CRYPTO_HW), 1)
DEFINES_APP += -DMBEDTLS_USER_CONFIG_FILE="\"mcuboot_crypto_acc_config.h\""
DEFINES_APP += -DCY_CRYPTO_HAL_DISABLE
DEFINES_APP += -DCY_MBEDTLS_HW_ACCELERATION
endif

# Compile with user redefined values for UART HW, port, pins
ifeq ($(USE_CUSTOM_DEBUG_UART), 1)
DEFINES_APP += -DUSE_CUSTOM_DEBUG_UART=1
endif

# Encrypted image support
ifeq ($(ENC_IMG), 1)
DEFINES_APP += -DENC_IMG=1
# Use maximum optimization level for PSOC6 encrypted image with
# external flash so it would fit into 0x18000 size of MCUBootApp
ifeq ($(BUILDCFG), Debug)
ifeq ($(USE_EXTERNAL_FLASH), 1)
CFLAGS_OPTIMIZATION := -Os -g3
endif
endif
endif


# Collect MCUBoot sourses
SOURCES_MCUBOOT := $(wildcard $(PRJ_DIR)/../bootutil/src/*.c)
# Collect MCUBoot Application sources
SOURCES_APP_SRC := main.c cy_security_cnt.c keys.c
# Collect platform dependend application files
SOURCES_APP_SRC += $(PLATFORM_APP_SOURCES)

# Collect Flash Layer sources and header files dirs
INCLUDE_DIRS_FLASH := $(PLATFORM_INCLUDE_DIRS_FLASH)
SOURCES_FLASH := $(PLATFORM_SOURCES_FLASH)

# Collect all the sources
SOURCES_APP := $(SOURCES_MCUBOOT)
SOURCES_APP += $(SOURCES_FLASH)
SOURCES_APP += $(addprefix $(CUR_APP_PATH)/, $(SOURCES_APP_SRC))

INCLUDE_DIRS_MCUBOOT := $(addprefix -I, $(PRJ_DIR)/../bootutil/include)
INCLUDE_DIRS_MCUBOOT += $(addprefix -I, $(PRJ_DIR)/../bootutil/src)
INCLUDE_DIRS_MCUBOOT += $(addprefix -I, $(PRJ_DIR)/..)

INCLUDE_DIRS_APP += $(addprefix -I, $(PRJ_DIR))
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH))
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH)/config)
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH)/os)
INCLUDE_DIRS_APP += $(addprefix -I, $(INCLUDE_DIRS_FLASH))

ASM_FILES_APP :=
ASM_FILES_APP += $(ASM_FILES_STARTUP)

# Pass variables to linker script and overwrite path to it, if custom is required
ifeq ($(COMPILER), GCC_ARM)
LDFLAGS += $(LDFLAGS_DEFSYM)
LINKER_SCRIPT := $(CUR_APP_PATH)/$(APP_NAME)_$(CORE).ld
else
$(error Only GCC ARM is supported at this moment)
endif
