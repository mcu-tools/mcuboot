################################################################################
# \file BlinkyApp.mk
# \version 1.0
#
# \brief
# Makefile to describe demo application BlinkyApp for Cypress MCUBoot based applications.
#
################################################################################
# \copyright
# Copyright 2018-2025 Cypress Semiconductor Corporation
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
USE_HW_KEY ?= 0
DISABLE_WDT_FREE ?= 0

# image type can be BOOT or UPGRADE
IMG_TYPES = BOOT UPGRADE

CUR_APP_PATH = $(PRJ_DIR)/$(APP_NAME)

-include $(CUR_APP_PATH)/memorymap.mk

# TODO: optimize here and in MCUBootApp.mk
# Output folder
ifeq ($(IMG_ID), 1)
    OUT ?= $(APP_NAME)/out
else
    OUT ?= $(APP_NAME)/out.id$(IMG_ID)
endif

# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(PLATFORM)

OUT_CFG := $(OUT_TARGET)/$(BUILDCFG)

# Set build directory for BOOT and UPGRADE images

ifeq ($(USE_DIRECT_XIP), 1)
    ifeq ($(APP_SLOT), 1)
        OUT_CFG := $(OUT_CFG)/primary
    else ifeq ($(APP_SLOT), 2)
        OUT_CFG := $(OUT_CFG)/secondary
    endif
else
    ifeq ($(IMG_TYPE), UPGRADE)
        OUT_CFG := $(OUT_CFG)/upgrade
    else
        OUT_CFG := $(OUT_CFG)/boot
    endif
endif

# Set parameters needed for signing
ifeq ($(IMG_TYPE), UPGRADE)
    UPGRADE_SUFFIX :=_upgrade
endif

include $(PRJ_DIR)/platforms.mk

ifneq ($(FLASH_MAP), )
ifeq ($(FAMILY), CYW20829)
$(CUR_APP_PATH)/memorymap.mk:
	$(PYTHON_PATH) scripts/memorymap.py -p $(PLATFORM) -i $(FLASH_MAP) -o $(PRJ_DIR)/platforms/memory/memorymap.c -a $(PRJ_DIR)/platforms/memory/memorymap.h -d $(IMG_ID) -k $(CUR_APP_PATH)/memorymap.mk -c $(PRJ_DIR)/policy/policy_reprovisioning_secure.json
else ifeq ($(FAMILY), PSOC6)
$(CUR_APP_PATH)/memorymap.mk:
	$(PYTHON_PATH) scripts/memorymap.py -p $(PLATFORM) -i $(FLASH_MAP) -o $(PRJ_DIR)/platforms/memory/memorymap.c -a $(PRJ_DIR)/platforms/memory/memorymap.h -d $(IMG_ID) -k $(CUR_APP_PATH)/memorymap.mk -m
else
$(CUR_APP_PATH)/memorymap.mk:
	$(PYTHON_PATH) scripts/memorymap_rework.py run -p $(PLATFORM_CONFIG) -i $(FLASH_MAP) -o $(PRJ_DIR)/platforms/memory -n memorymap -d $(IMG_ID) > $(CUR_APP_PATH)/memorymap.mk
endif
    DEFINES += -DCY_FLASH_MAP_JSON
endif

include $(PRJ_DIR)/common_libs.mk

#Blinky Release XIP mode workaround
ifneq ($(FAMILY), CYW20829)
    ifeq ($(BUILDCFG), Release)
        ifeq ($(USE_EXTERNAL_FLASH), 1)
            CFLAGS_OPTIMIZATION := -Og -g3
        endif
    endif
endif

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
    DEFINES += -DBOOT_IMAGE
    ENC_IMG := 0
else
    DEFINES += -DUPGRADE_IMAGE
    DEFINES += -DSWAP_DISABLED=$(USE_OVERWRITE)
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

DEFINES += -DMCUBOOT_IMAGE_NUMBER=$(MCUBOOT_IMAGE_NUMBER)
DEFINES += -DUSER_APP_RAM_START=$(USER_APP_RAM_START)
DEFINES += -DUSER_APP_RAM_SIZE=$(USER_APP_RAM_SIZE)
DEFINES += -DUSER_APP_START=$(USER_APP_START)
DEFINES += -DPRIMARY_IMG_START=$(PRIMARY_IMG_START)
DEFINES += -DSECONDARY_IMG_START=$(SECONDARY_IMG_START)
DEFINES += -DUSER_APP_SIZE=$(SLOT_SIZE)
DEFINES += -DAPP_SLOT=$(APP_SLOT)
DEFINES += -DAPP_$(APP_CORE)
DEFINES += -DBOOT_$(APP_CORE)

ifneq ($(APP_CORE_ID),)
    DEFINES += -DAPP_CORE_ID=$(APP_CORE_ID)
endif

DEFINES += -DMEMORY_ALIGN=$(PLATFORM_MEMORY_ALIGN)
DEFINES += -DPLATFORM_MAX_TRAILER_PAGE_SIZE=$(PLATFORM_MAX_TRAILER_PAGE_SIZE)

#Use default led if no command line parameter added
ifeq ($(LED_PORT), )
    DEFINES += -DLED_PORT=$(LED_PORT_DEFAULT)
else
    DEFINES += -DLED_PORT=GPIO_PRT$(LED_PORT)
endif

ifeq ($(LED_PIN), )
    DEFINES += -DLED_PIN=$(LED_PIN_DEFAULT)
else
    DEFINES += -DLED_PIN=$(LED_PIN)
endif

#Use default UART if no command line parameter added
ifeq ($(UART_TX), )
    DEFINES += -DCY_DEBUG_UART_TX=$(UART_TX_DEFAULT)
else
    DEFINES += -DCY_DEBUG_UART_TX=$(UART_TX)
endif

ifeq ($(UART_RX), )
    DEFINES += -DCY_DEBUG_UART_RX=$(UART_RX_DEFAULT)
else
    DEFINES += -DCY_DEBUG_UART_RX=$(UART_RX)
endif

ifeq ($(USE_EXTERNAL_FLASH), 1)
    ifeq ($(USE_XIP), 1)
        DEFINES += -DUSE_XIP
        LD_SUFFIX = _xip
    endif
    DEFINES += -DCY_BOOT_USE_EXTERNAL_FLASH
endif

# Add version metadata to image
ifneq ($(IMG_VER), )
    IMG_VER_ARG = -v "$(IMG_VER)"
    DEFINES += -DIMG_VER_MSG=\"$(IMG_VER)\"
else
    IMG_VER_ARG = -v "$(PLATFORM_DEFAULT_IMG_VER_ARG)"
    DEFINES += -DIMG_VER_MSG=\"$(PLATFORM_DEFAULT_IMG_VER_ARG)\"
    $(info WARNING - setting platform default version number, to set custom value - pass IMG_VER=x.x.x argument to make command)
endif

# Add dependencies metadata to image
ifneq ($(IMG_DEPS_ID), )
    ifneq ($(IMG_DEPS_VER), )
        IMG_DEPS_ARG = -d "($(IMG_DEPS_ID), $(IMG_DEPS_VER))"
    endif
endif

# Collect Test Application sources
C_FILES += $(wildcard $(CUR_APP_PATH)/*.c)


ifeq ($(USE_DIRECT_XIP), 1)
    # Set offset for secondary image
    ifeq ($(APP_SLOT), 2)
        HEADER_OFFSET := $(SECONDARY_IMG_START)
    else
        HEADER_OFFSET := $(PRIMARY_IMG_START)
    endif
else
    # Set offset for secondary image
    ifeq ($(IMG_TYPE), UPGRADE)
        HEADER_OFFSET := $(SECONDARY_IMG_START)
    else
        HEADER_OFFSET := $(PRIMARY_IMG_START)
    endif
endif

# Collect all the sources


# Collect includes for BlinkyApp
INCLUDE_DIRS += $(CURDIR)
INCLUDE_DIRS += $(CUR_APP_PATH)

# ++++
INCLUDE_DIRS += $(PRJ_DIR)/MCUBootApp/config
INCLUDE_DIRS += $(PRJ_DIR)/MCUBootApp
INCLUDE_DIRS += $(PRJ_DIR)/../bootutil/include
INCLUDE_DIRS += $(PRJ_DIR)/../bootutil/src
INCLUDE_DIRS += $(PRJ_DIR)/../bootutil/include/bootutil
# +++

# Include confirmation flag setting (img_ok) implementation
ifeq ($(IMG_TYPE), UPGRADE)
    ifeq ($(USE_OVERWRITE), 0)
        C_FILES += $(PRJ_DIR)/platforms/img_confirm/$(FAMILY)/set_img_ok.c
        INCLUDE_DIRS += $(PRJ_DIR)/platforms/img_confirm
    endif
endif

ifeq ($(USE_DIRECT_XIP), 1)
    C_FILES += $(PRJ_DIR)/platforms/img_confirm/$(FAMILY)/set_img_ok.c
    INCLUDE_DIRS += $(PRJ_DIR)/platforms/img_confirm
endif

# Overwite path to linker script if custom is required, otherwise default from BSP is used

LINKER_SCRIPT := $(CUR_APP_PATH)/linker/$(APP_NAME).ld

# add flag to imgtool if not using swap for upgrade
ifeq ($(USE_OVERWRITE), 1)
    UPGRADE_TYPE := --overwrite-only
    DEFINES += -DMCUBOOT_OVERWRITE_ONLY
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

# Include full public key to signed image TLV insted of its hash
ifeq ($(USE_HW_KEY), 1)
    SIGN_ARGS += --public-key-format
endif

# Set parameters needed for signing
ifeq ($(IMG_TYPE), UPGRADE)
    # Set img_ok flag to trigger swap type permanent
    ifeq ($(CONFIRM), 1)
        SIGN_ARGS += --confirm
    endif
    SIGN_ARGS += --pad
endif

$(info $(SIGN_ARGS))

# Disble wdt free hal call
ifneq ($(DISABLE_WDT_FREE), 0)
    DEFINES += -DDISABLE_WDT_FREE
endif
