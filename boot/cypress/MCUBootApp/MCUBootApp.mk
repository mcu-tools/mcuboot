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

# Cypress' MCUBoot Application supports GCC ARM only at this moment
# Set default compiler to GCC if not specified from command line
COMPILER ?= GCC_ARM

USE_CRYPTO_HW ?= 0
USE_EXTERNAL_FLASH ?= 0
MCUBOOT_IMAGE_NUMBER ?= 1
ENC_IMG ?= 0

# For which core this application is built
CORE ?= CM0P

ifneq ($(COMPILER), GCC_ARM)
$(error Only GCC ARM is supported at this moment)
endif

CUR_APP_PATH = $(PRJ_DIR)/$(APP_NAME)

include $(PRJ_DIR)/platforms.mk
include $(PRJ_DIR)/common_libs.mk
include $(PRJ_DIR)/toolchains.mk

# default slot size is 0x10000, 512bytes per row/sector, so 128 sectors
MAX_IMG_SECTORS ?= 128

# Application-specific DEFINES
DEFINES_APP := -DMBEDTLS_CONFIG_FILE="\"mcuboot_crypto_config.h\""
DEFINES_APP += -DECC256_KEY_FILE="\"keys/$(SIGN_KEY_FILE).pub\""
DEFINES_APP += -DCORE=$(CORE)
DEFINES_APP += -DMCUBOOT_IMAGE_NUMBER=$(MCUBOOT_IMAGE_NUMBER)
ifeq ($(USE_EXTERNAL_FLASH), 1)
DEFINES_APP += -DCY_BOOT_USE_EXTERNAL_FLASH
endif
DEFINES_APP += -DMCUBOOT_MAX_IMG_SECTORS=$(MAX_IMG_SECTORS)
# Hardrware acceleration support
ifeq ($(USE_CRYPTO_HW), 1)
DEFINES_APP += -DMBEDTLS_USER_CONFIG_FILE="\"mcuboot_crypto_acc_config.h\""
DEFINES_APP += -DCY_CRYPTO_HAL_DISABLE
DEFINES_APP += -DCY_MBEDTLS_HW_ACCELERATION
endif
# Encrypted image support
ifeq ($(ENC_IMG), 1)
DEFINES_APP += -DENC_IMG=1
endif

# Collect MCUBoot sourses
SOURCES_MCUBOOT := $(wildcard $(CURDIR)/../bootutil/src/*.c)
# Collect MCUBoot Application sources
SOURCES_APP_SRC := $(wildcard $(CUR_APP_PATH)/*.c)

# Collect Flash Layer port sources
SOURCES_FLASH_PORT := $(wildcard $(CURDIR)/cy_flash_pal/*.c)
SOURCES_FLASH_PORT += $(wildcard $(CURDIR)/cy_flash_pal/flash_qspi/*.c)
# Collect all the sources
SOURCES_APP := $(SOURCES_MCUBOOT)
SOURCES_APP += $(SOURCES_APP_SRC)
SOURCES_APP += $(SOURCES_FLASH_PORT)

INCLUDE_DIRS_MCUBOOT := $(addprefix -I, $(CURDIR)/../bootutil/include)
INCLUDE_DIRS_MCUBOOT += $(addprefix -I, $(CURDIR)/../bootutil/src)
INCLUDE_DIRS_MCUBOOT += $(addprefix -I, $(CURDIR)/..)

INCLUDE_DIRS_APP := $(addprefix -I, $(CURDIR))
INCLUDE_DIRS_APP += $(addprefix -I, $(CURDIR)/cy_flash_pal/flash_qspi)
INCLUDE_DIRS_APP += $(addprefix -I, $(CURDIR)/cy_flash_pal/include)
INCLUDE_DIRS_APP += $(addprefix -I, $(CURDIR)/cy_flash_pal/include/flash_map_backend)
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH))
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH)/config)
INCLUDE_DIRS_APP += $(addprefix -I, $(CUR_APP_PATH)/os)

ASM_FILES_APP :=
ASM_FILES_APP += $(ASM_FILES_STARTUP)

# Output folder
OUT := $(APP_NAME)/out
# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(PLATFORM)

OUT_CFG := $(OUT_TARGET)/$(BUILDCFG)

# Overwite path to linker script if custom is required
ifeq ($(COMPILER), GCC_ARM)
LINKER_SCRIPT := $(subst /cygdrive/c,c:,$(CUR_APP_PATH)/$(APP_NAME).ld)
else
$(error Only GCC ARM is supported at this moment)
endif