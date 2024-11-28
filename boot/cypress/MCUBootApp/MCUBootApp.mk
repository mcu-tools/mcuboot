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

CUR_APP_PATH = $(PRJ_DIR)/$(APP_NAME)

-include $(CUR_APP_PATH)/memorymap.mk

MCUBOOT_IMAGE_NUMBER ?= 1
ENC_IMG ?= 0
USE_HW_KEY ?= 0
USE_BOOTSTRAP ?= 1
USE_SHARED_SLOT ?= 0
FIH_PROFILE_LEVEL_LIST := OFF LOW MEDIUM HIGH
FIH_PROFILE_LEVEL ?= MEDIUM
MCUBOOT_SWAP_STATUS_FAST_BOOT ?= 0

ifeq ($(BUILDCFG), Release)
    MCUBOOT_LOG_LEVEL ?= MCUBOOT_LOG_LEVEL_INFO
else
    MCUBOOT_LOG_LEVEL ?= MCUBOOT_LOG_LEVEL_DEBUG
endif

ifneq ($(COMPILER), GCC_ARM)
$(error Only GCC ARM is supported at this moment)
endif

# Check FIH profile param
ifneq ($(filter $(FIH_PROFILE_LEVEL), $(FIH_PROFILE_LEVEL_LIST)),)
    ifneq ($(FIH_PROFILE_LEVEL), OFF) 
        DEFINES += -DMCUBOOT_FIH_PROFILE_ON
        DEFINES += -DMCUBOOT_FIH_PROFILE_$(FIH_PROFILE_LEVEL)
    endif
else
    $(error Wrong FIH_PROFILE_LEVEL param)
endif

# Output folder
OUT := $(APP_NAME)/out
# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(PLATFORM)

OUT_CFG := $(OUT_TARGET)/$(BUILDCFG)

include $(PRJ_DIR)/platforms.mk

ifneq ($(FLASH_MAP), )

ifeq ($(FAMILY), CYW20829)
$(CUR_APP_PATH)/memorymap.mk:
	$(PYTHON_PATH) scripts/memorymap.py -p $(PLATFORM) -i $(FLASH_MAP) -o $(PRJ_DIR)/platforms/memory/memorymap.c -a $(PRJ_DIR)/platforms/memory/memorymap.h -c $(PRJ_DIR)/policy/policy_secure.json > $(CUR_APP_PATH)/memorymap.mk
else ifeq ($(FAMILY), PSOC6)
$(CUR_APP_PATH)/memorymap.mk:
	$(PYTHON_PATH) scripts/memorymap.py -p $(PLATFORM) -m -i $(FLASH_MAP) -o $(PRJ_DIR)/platforms/memory/memorymap.c -a $(PRJ_DIR)/platforms/memory/memorymap.h > $(CUR_APP_PATH)/memorymap.mk
else
$(CUR_APP_PATH)/memorymap.mk:
	$(PYTHON_PATH) scripts/memorymap_rework.py run -p $(PLATFORM_CONFIG) -i $(FLASH_MAP) -o $(PRJ_DIR)/platforms/memory -n memorymap > $(CUR_APP_PATH)/memorymap.mk
endif

    DEFINES += -DCY_FLASH_MAP_JSON
endif

include $(PRJ_DIR)/common_libs.mk
include $(PRJ_DIR)/toolchains.mk

ifeq ($(MAX_IMG_SECTORS), )
    MAX_IMG_SECTORS ?= $(PLATFORM_MAX_IMG_SECTORS)
endif

# Application-specific DEFINES
DEFINES += -DMBEDTLS_CONFIG_FILE="\"mcuboot_crypto_config.h\""
DEFINES += -DECC256_KEY_FILE="\"keys/$(SIGN_KEY_FILE).pub\""
DEFINES += -DBOOT_$(CORE)
DEFINES += -DAPP_$(APP_CORE)

ifneq ($(APP_CORE_ID),)
    DEFINES += -DAPP_CORE_ID=$(APP_CORE_ID)
endif

DEFINES += -DMCUBOOT_IMAGE_NUMBER=$(MCUBOOT_IMAGE_NUMBER)
DEFINES += -DUSE_SHARED_SLOT=$(USE_SHARED_SLOT)
DEFINES += -DMCUBOOT_PLATFORM_CHUNK_SIZE=$(PLATFORM_CHUNK_SIZE)
DEFINES += -DMEMORY_ALIGN=$(PLATFORM_MEMORY_ALIGN)
DEFINES += -DPLATFORM_MAX_TRAILER_PAGE_SIZE=$(PLATFORM_MAX_TRAILER_PAGE_SIZE)

# Define MCUboot size and pass it to linker script
LDFLAGS_DEFSYM  += -Wl,--defsym,BOOTLOADER_SIZE=$(BOOTLOADER_SIZE)

APP_DEFAULT_POLICY ?= $(PLATFORM_APP_DEFAULT_POLICY)

ifeq ($(USE_EXTERNAL_FLASH), 1)
    ifeq ($(USE_XIP), 1)
        DEFINES += -DUSE_XIP
    endif

    DEFINES += -DCY_BOOT_USE_EXTERNAL_FLASH
    DEFINES += -DCY_MAX_EXT_FLASH_ERASE_SIZE=$(PLATFORM_CY_MAX_EXT_FLASH_ERASE_SIZE)
endif

ifeq ($(USE_OVERWRITE), 1)
    DEFINES += -DMCUBOOT_OVERWRITE_ONLY
    ifeq ($(USE_SW_DOWNGRADE_PREV), 1)
        DEFINES += -DMCUBOOT_DOWNGRADE_PREVENTION
    endif
else
    ifeq ($(USE_BOOTSTRAP), 1)
        DEFINES += -DMCUBOOT_BOOTSTRAP
    endif
endif

DEFINES += -DMCUBOOT_MAX_IMG_SECTORS=$(MAX_IMG_SECTORS)
DEFINES += -DMCUBOOT_LOG_LEVEL=$(MCUBOOT_LOG_LEVEL)

ifeq ($(USE_HW_ROLLBACK_PROT), 1)
    DEFINES += -DMCUBOOT_HW_ROLLBACK_PROT
    # Service RAM app address (size 0x8000)
    DEFINES += -DSERVICE_APP_OFFSET=$(PLATFORM_SERVICE_APP_OFFSET)
    # Service RAM app input parameters address (size 0x400)
    DEFINES += -DSERVICE_APP_INPUT_PARAMS_OFFSET=$(PLATFORM_SERVICE_APP_INPUT_PARAMS_OFFSET)
    # Service RAM app descriptor addr (size 0x20)
    DEFINES += -DSERVICE_APP_DESC_OFFSET=$(PLATFORM_SERVICE_APP_DESC_OFFSET)
    # Service RAM app size
    DEFINES += -DSERVICE_APP_SIZE=$(PLATFORM_SERVICE_APP_SIZE)
endif

# Hardware acceleration support
ifeq ($(USE_CRYPTO_HW), 1)
    DEFINES += -DMBEDTLS_USER_CONFIG_FILE="\"mcuboot_crypto_acc_config.h\""
    DEFINES += -DCY_CRYPTO_HAL_DISABLE
    DEFINES += -DCY_MBEDTLS_HW_ACCELERATION

    INCLUDE_DIRS += $(CY_LIBS_PATH)/cy-mbedtls-acceleration
    INCLUDE_DIRS += $(CY_LIBS_PATH)/cy-mbedtls-acceleration/COMPONENT_CAT1/include

    INCLUDE_DIRS += $(CY_LIBS_PATH)/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_$(CRYPTO_ACC_TYPE)
    C_FILES += $(wildcard $(CY_LIBS_PATH)/cy-mbedtls-acceleration/COMPONENT_CAT1/mbedtls_$(CRYPTO_ACC_TYPE)/*.c)
    
    ifeq ($(FAMILY), CYW20829)
        DEFINES += -Dcy_stc_cryptolite_context_sha256_t=cy_stc_cryptolite_context_sha_t
    endif

endif

ifneq ($(MCUBOOT_IMAGE_NUMBER), 1)
    ifeq ($(MCUBOOT_DEPENDENCY_CHECK), 1)
        DEFINES += -DMCUBOOT_DEPENDENCY_CHECK
    endif
endif

# Use key provisioned in device to verify images
ifeq ($(USE_HW_KEY), 1)
    DEFINES += -DMCUBOOT_HW_KEY
endif

# Compile with user redefined values for UART HW, port, pins
ifeq ($(USE_CUSTOM_DEBUG_UART), 1)
    DEFINES += -DUSE_CUSTOM_DEBUG_UART=1
endif

# Log timestamp information
ifeq ($(USE_LOG_TIMESTAMP), 1)
    DEFINES += -DUSE_LOG_TIMESTAMP
endif

# Encrypted image support
ifeq ($(ENC_IMG), 1)
    DEFINES += -DENC_IMG=1
    ifeq ($(FAMILY), CYW20829)
        DEFINES += -DMCUBOOT_ENC_IMAGES_XIP
    endif
# Use maximum optimization level for PSOC6 encrypted image with
# external flash so it would fit into 0x18000 size of MCUBootApp
    ifneq ($(FAMILY), CYW20829)
        ifeq ($(BUILDCFG), Debug)
            ifeq ($(USE_EXTERNAL_FLASH), 1)
                CFLAGS_OPTIMIZATION := -Os -g3
            endif
        endif
    endif
endif

ifeq ($(USE_MEASURED_BOOT), 1)
    DEFINES += -DMCUBOOT_MEASURED_BOOT
    DEFINES += -DMAX_BOOT_RECORD_SZ=512
    DEFINES += -DMCUBOOT_SHARED_DATA_BASE=0x08000800
    DEFINES += -DMCUBOOT_SHARED_DATA_SIZE=0x200
endif

ifeq ($(USE_DATA_SHARING), 1)
    DEFINES += -DMCUBOOT_DATA_SHARING
    DEFINES += -DMAX_BOOT_RECORD_SZ=512
    DEFINES += -DMCUBOOT_SHARED_DATA_BASE=0x08000800
    DEFINES += -DMCUBOOT_SHARED_DATA_SIZE=0x200
endif

ifeq ($(BOOT_RECORD_SW_TYPE), )
    BOOT_RECORD := --boot-record MCUBootApp
else
    BOOT_RECORD := --boot-record $(BOOT_RECORD_SW_TYPE)
endif

# Collect MCUBoot sourses
C_FILES += $(wildcard $(PRJ_DIR)/../bootutil/src/*.c)

# Collect MCUBoot Application sources
C_FILES += $(CUR_APP_PATH)/main.c $(CUR_APP_PATH)/keys.c

ifeq ($(USE_EXEC_TIME_CHECK), 1)
    DEFINES += -DUSE_EXEC_TIME_CHECK=1
    C_FILES += $(CUR_APP_PATH)/misc/timebase_us.c
endif


INCLUDE_DIRS += $(PRJ_DIR)/../bootutil/include
INCLUDE_DIRS += $(PRJ_DIR)/../bootutil/include/bootutil
INCLUDE_DIRS += $(PRJ_DIR)/../bootutil/include/bootutil/crypto
INCLUDE_DIRS += $(PRJ_DIR)/../bootutil/src
INCLUDE_DIRS += $(PRJ_DIR)/..

INCLUDE_DIRS += $(PRJ_DIR)
INCLUDE_DIRS += $(CUR_APP_PATH)
INCLUDE_DIRS += $(CUR_APP_PATH)/config
INCLUDE_DIRS += $(CUR_APP_PATH)/os

# Pass variables to linker script and overwrite path to it, if custom is required
ifeq ($(FAMILY), XMC7000)
    LINKER_SCRIPT := $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system/COMPONENT_$(CORE)/TOOLCHAIN_$(COMPILER)/linker.ld
else
    LINKER_SCRIPT := $(CUR_APP_PATH)/$(APP_NAME)_$(CORE).ld
endif

ifeq ($(COMPILER), GCC_ARM)
    LDFLAGS += $(LDFLAGS_DEFSYM)
else ifeq ($(COMPILER), IAR)
    $(error $(COMPILER) not supported at this moment)

else ifeq ($(COMPILER), ARM)
    $(error $(COMPILER) not supported at this moment)

else
    $(error $(COMPILER) not supported at this moment)
endif
       
