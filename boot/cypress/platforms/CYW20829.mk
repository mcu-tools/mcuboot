################################################################################
# \file CYW20829.mk
# \version 1.0
#
# \brief
# This file is dedicated for CYW20829 platform
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

# Default core name of platform
CORE := CM33
CORE_SUFFIX := m33

# PDL category suffix to resolve common path in pdl
PDL_CAT_SUFFIX := 1B
CRYPTO_ACC_TYPE := MXCRYPTOLITE

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
ifeq ($(PLATFORM), CYW20829)
DEVICE ?= CYW20829B0LKML
else ifeq ($(PLATFORM), CYW89829)
DEVICE ?= CYW89829B0232
endif

#Led pin default config
LED_PORT_DEFAULT ?= GPIO_PRT0
LED_PIN_DEFAULT ?= 0U

#UART default config
UART_TX_DEFAULT ?= CYBSP_DEBUG_UART_TX
UART_RX_DEFAULT ?= CYBSP_DEBUG_UART_RX

PLATFORM_SUFFIX ?= cyw20829

# Add device name to defines
DEFINES += -D$(DEVICE)

USE_SWAP_STATUS ?= 0

ifeq ($(USE_DIRECT_XIP), 1)
DEFINES += -DMCUBOOT_DIRECT_XIP=1
DEFINES += -DMCUBOOT_DIRECT_XIP_REVERT=1
endif

ifeq ($(USE_SWAP_STATUS), 1)
DEFINES += -DUSE_SWAP_STATUS=1
endif

# Default upgrade method
PLATFORM_DEFAULT_USE_OVERWRITE ?= 0

# Minimum erase size of underlying memory hardware
PLATFORM_MEMORY_ALIGN := 0x1000
PLATFORM_MAX_TRAILER_PAGE_SIZE := 0x1000

# Device flash start
FLASH_START := 0x60000000
FLASH_XIP_START := 0x08000000

ifeq ($(SMIF_ENC), 1)
    DEFINES += -DMCUBOOT_ENC_IMAGES_SMIF=1
endif

###############################################################################
# Application specific libraries
###############################################################################
# MCUBootApp
###############################################################################
THIS_APP_PATH = $(PRJ_DIR)/libs

ifeq ($(APP_NAME), MCUBootApp)

DEFINES += -DBSP_DISABLE_WDT

SMIF_ENC ?= 0

ifeq ($(CYW20829_PSVP), )
DEFINES += -DCOMPONENT_CUSTOM_DESIGN_MODUS
endif

# Platform dependend utils files
C_FILES += $(PRJ_DIR)/platforms/utils/$(FAMILY)/platform_utils.c
INCLUDE_DIRS += $(PRJ_DIR)/platforms/utils/$(FAMILY)

# mbedTLS hardware acceleration settings
ifeq ($(USE_CRYPTO_HW), 1)
# cy-mbedtls-acceleration related include directories
INCLUDE_DIRS += $(PRJ_DIR)/platforms/crypto/$(FAMILY)/mbedtls_Cryptolite
# Collect source files for MbedTLS acceleration
C_FILES += $(wildcard $(PRJ_DIR)/platforms/crypto/$(FAMILY)/mbedtls_Cryptolite/*.c)
#
endif

###############################################################################
# Application dependent definitions
# MCUBootApp default settings
USE_CRYPTO_HW ?= 1

USE_EXTERNAL_FLASH := 1
PROVISION_PATH ?= $(PRJ_DIR)
LCS ?= NORMAL_NO_SECURE
APPTYPE ?= flash
SIGN_TYPE ?= bootrom_next_app
SMIF_CRYPTO_CONFIG ?= NONE
MCUBOOT_DEPENDENCY_CHECK ?= 1

ifeq ($(LCS), NORMAL_NO_SECURE)
APP_DEFAULT_POLICY ?= $(PRJ_DIR)/policy/policy_no_secure.json
else
APP_DEFAULT_POLICY ?= $(PRJ_DIR)/policy/policy_secure.json
endif

# Define MCUBootApp specific parameters
PLATFORM_MAX_IMG_SECTORS = 32U

###############################################################################
# MCUBootApp flash map custom settings
###############################################################################
# Set this flag to 1 to enable custom settings in MCUBootApp
USE_CUSTOM_MEMORY_MAP ?= 1

PLATFORM_CY_MAX_EXT_FLASH_ERASE_SIZE ?= 4096U

# Default memory single chunk size
PLATFORM_CHUNK_SIZE := 4096U
###############################################################################

SIGN_ENC := 0

ifeq ($(SMIF_ENC), 1)
    SIGN_ENC := 1
endif

###############################################################################
# MCUBootApp service app definitions
###############################################################################
# Service app is only used in SECURE lifecycle
ifeq ($(LCS), SECURE)
# Service app path and file name
SERVICE_APP_PATH := $(PRJ_DIR)/packets/apps/reprovisioning$(SERVICE_APP_PLATFORM_SUFFIX)
SERVICE_APP_NAME := cyapp_reprovisioning_signed_icv0

ifeq ($(ENC_IMG), 1)
    SIGN_ENC := 1
endif

# Service app size is calculated here and converted to hex format
PLATFORM_SERVICE_APP_SIZE ?= 0x$(shell printf "%x" `wc -c < $(SERVICE_APP_PATH)/$(SERVICE_APP_NAME).bin`)
else
ifeq ($(USE_HW_ROLLBACK_PROT), 1)
$(warning Hardware rollback protection USE_HW_ROLLBACK_PROT=1 is only valid for LCS=SECURE mode)
endif
endif
###############################################################################

# Post build job to execute for platform
post_build: $(OUT_CFG)/$(APP_NAME).elf
ifeq ($(POST_BUILD_ENABLE), 1)
	$(info [TOC2_Generate] - Execute toc2 generator script for $(APP_NAME))
	@echo $(SHELL) $(PRJ_DIR)/run_toc2_generator.sh $(LCS) $(OUT_CFG) $(APP_NAME) $(APPTYPE) $(PROVISION_PATH) $(SMIF_CRYPTO_CONFIG) $(TOOLCHAIN_PATH) $(APP_DEFAULT_POLICY) $(BOOTLOADER_SIZE) $(SIGN_ENC) $(PLATFORM) $(PLATFORM_SERVICE_APP_DESC_OFFSET)
	$(shell        $(PRJ_DIR)/run_toc2_generator.sh $(LCS) $(OUT_CFG) $(APP_NAME) $(APPTYPE) $(PROVISION_PATH) $(SMIF_CRYPTO_CONFIG) $(TOOLCHAIN_PATH) $(APP_DEFAULT_POLICY) $(BOOTLOADER_SIZE) $(SIGN_ENC) $(PLATFORM) $(PLATFORM_SERVICE_APP_DESC_OFFSET))

	# Convert binary to hex and rename
	$(shell mv -f $(OUT_CFG)/$(APP_NAME).final.bin $(OUT_CFG)/$(APP_NAME).bin || rm -f $(OUT_CFG)/$(APP_NAME).bin)

	$(GCC_PATH)/bin/arm-none-eabi-objcopy --change-address=$(FLASH_START) -I binary -O ihex $(OUT_CFG)/$(APP_NAME).bin $(OUT_CFG)/$(APP_NAME).hex
ifeq ($(USE_HW_ROLLBACK_PROT), 1)
	$(GCC_PATH)/bin/arm-none-eabi-objcopy --change-address=$$(($(FLASH_START)+$(PLATFORM_SERVICE_APP_OFFSET))) -I binary -O ihex $(SERVICE_APP_PATH)/$(SERVICE_APP_NAME).bin $(OUT_CFG)/$(SERVICE_APP_NAME).hex
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

APP_SLOT ?= 1

ifeq ($(shell [ $(APP_SLOT) -gt 2 ] && echo true), true)
    $(error APP_SLOT must be 1 or 2)
endif

# Basic settings
LCS ?= NORMAL_NO_SECURE
APPTYPE ?= flash
SIGN_TYPE ?= mcuboot_user_app
SMIF_CRYPTO_CONFIG ?= NONE
USE_IMG_TRAILER ?= 1

ifeq ($(LCS), NORMAL_NO_SECURE)
APP_DEFAULT_POLICY ?= $(PRJ_DIR)/policy/policy_no_secure.json
else
APP_DEFAULT_POLICY ?= $(PRJ_DIR)/policy/policy_secure.json
endif

PLATFORM_DEFAULT_ERASED_VALUE := 0xff

# Define start of application
PLATFORM_USER_APP_START ?= $(shell printf "0x%X" $$(($(PRIMARY_IMG_START)-$(FLASH_START)+$(FLASH_XIP_START))))
PLATFORM_USER_APP_LOCATION ?= $(shell printf "0x%X" $$(($(PRIMARY_IMG_START)-$(FLASH_START)+$(FLASH_XIP_START))))

ifeq ($(IMG_TYPE), UPGRADE)
    PLATFORM_USER_APP_LOCATION := $(shell printf "0x%X" $$(($(SECONDARY_IMG_START)-$(FLASH_START)+$(FLASH_XIP_START))))
endif

ifeq ($(APP_SLOT), 2)
    PLATFORM_USER_APP_LOCATION := $(shell printf "0x%X" $$(($(SECONDARY_IMG_START)-$(FLASH_START)+$(FLASH_XIP_START))))
endif

# Define RAM start and size, slot size
PLATFORM_DEFAULT_RAM_START ?= 0x2000C000
PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000

ifeq ($(APP_SLOT), 1)
DEFINES += -DUSER_APP_START_OFF=$(shell echo $$(($(PRIMARY_IMG_START)-$(FLASH_START))))
else
DEFINES += -DUSER_APP_START_OFF=$(shell echo $$(($(SECONDARY_IMG_START)-$(FLASH_START))))
endif

PLATFORM_DEFAULT_IMG_VER_ARG ?= 1.0.0

SIGN_IMG_ID = $(shell expr $(IMG_ID) - 1)
PLATFORM_SIGN_ARGS := --image-format $(SIGN_TYPE) -i $(OUT_CFG)/$(APP_NAME).final.bin -o $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).bin --key-path $(PRJ_DIR)/keys/cypress-test-ec-p256.pem --update-key-path $(PRJ_DIR)/keys/priv_oem_0.pem --slot-size $(SLOT_SIZE) --align 1 --image-id $(SIGN_IMG_ID)

# Use encryption and random initial vector for image
ifeq ($(ENC_IMG), 1)
	PLATFORM_SIGN_ARGS += --encrypt --enckey ../../$(ENC_KEY_FILE).pem
	PLATFORM_SIGN_ARGS += --app-addr=$(PLATFORM_USER_APP_START)
endif

ifeq ($(USE_IMG_TRAILER), 1)
	PLATFORM_SIGN_ARGS += --pad
endif

pre_build:
	$(info [PRE_BUILD] - Generating linker script for application $(CUR_APP_PATH)/linker/$(APP_NAME).ld)
	@$(CC) -E -x c $(CFLAGS) $(INCLUDE_DIRS) $(CUR_APP_PATH)/linker/$(APP_NAME)_$(CORE)_template$(LD_SUFFIX).ld | grep -v '^#' >$(CUR_APP_PATH)/linker/$(APP_NAME).ld

post_build: $(OUT_CFG)/$(APP_NAME).bin
ifeq ($(POST_BUILD_ENABLE), 1)
	$(info [POST BUILD] - Executing post build script for $(APP_NAME))
	$(info [TOC2_Generate] - Execute toc2 generator script for $(APP_NAME))
	$(shell $(PRJ_DIR)/run_toc2_generator.sh $(LCS) $(OUT_CFG) $(APP_NAME) $(APPTYPE) $(PRJ_DIR) $(SMIF_CRYPTO_CONFIG) $(TOOLCHAIN_PATH))

	$(info SIGN_ARGS <-> $(SIGN_ARGS))
	$(info cysecuretools -q -t cyw20829 -p $(APP_DEFAULT_POLICY) sign-image $(SIGN_ARGS))
	$(shell cysecuretools -q -t cyw20829 -p $(APP_DEFAULT_POLICY) sign-image $(SIGN_ARGS))

ifeq ($(SMIF_ENC), 1)
	$(info edgeprotecttools -t cyw20829 encrypt --input $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).bin --output $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).bin --iv $(PLATFORM_USER_APP_LOCATION) --enckey keys/encrypt_key.bin --nonce ./MCUBootApp/out/CYW20829/$(BUILDCFG)/MCUBootApp.signed_nonce.bin)
	$(shell edgeprotecttools -t cyw20829 encrypt --input $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).bin --output $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).bin --iv $(PLATFORM_USER_APP_LOCATION) --enckey keys/encrypt_key.bin --nonce ./MCUBootApp/out/CYW20829/$(BUILDCFG)/MCUBootApp.signed_nonce.bin)
endif

	$(GCC_PATH)/bin/arm-none-eabi-objcopy --change-address=$(HEADER_OFFSET) -I binary -O ihex $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).bin $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex
	$(GCC_PATH)/bin/arm-none-eabi-objdump -s $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).hex > $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).objdump
else
	$(info Post build is disabled by POST_BUILD_ENABLE parameter)
endif # POST_BUILD_ENABLE
endif ## BlinkyApp

###############################################################################
# Toolchain
###############################################################################
# Define build flags specific to a certain platform
# Define build flags specific to a certain platform
CFLAGS_PLATFORM := -c -mcpu=cortex-m33+nodsp --specs=nano.specs

###############################################################################
# Common libraries
###############################################################################
C_FILES += ns_system_$(PLATFORM_SUFFIX).c
C_FILES += ns_start_$(PLATFORM_SUFFIX).c

INCLUDE_DIRS += $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system

INCLUDE_DIRS += $(PRJ_DIR)/libs/retarget-io

INCLUDE_DIRS += $(PRJ_DIR)/libs/mtb-hal-cat1/include
INCLUDE_DIRS += $(PRJ_DIR)/libs/mtb-hal-cat1/include_pvt
INCLUDE_DIRS += $(PRJ_DIR)/libs/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/
INCLUDE_DIRS += $(PRJ_DIR)/libs/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/pin_packages

DEFINES += -DCY_USING_HAL
DEFINES += -DCOMPONENT_CM33
DEFINES += -DCOMPONENT_PSOC6HAL
DEFINES += -DCOMPONENT_PSVP_CYW20829
DEFINES += -DCOMPONENT_SOFTFP
DEFINES += -DFLASH_BOOT

