################################################################################
# \file CYW20829.mk
# \version 1.0
#
# \brief
# This file is dedicated for CYW20829 platform
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

# Default core name of platform
CORE := CM33
CORE_SUFFIX := m33

# PDL category suffix to resolve common path in pdl
PDL_CAT_SUFFIX := 1B
CRYPTO_ACC_TYPE := MXCRYPTOLITE

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
DEVICE ?= CYW20829B0LKML
# If PSVP build is required
ifeq ($(CYW20829_PSVP), 1)
SERVICE_APP_PLATFORM_SUFFIX := _psvp
endif

#Led pin default config
LED_PORT_DEFAULT ?= GPIO_PRT0
LED_PIN_DEFAULT ?= 0U

#UART default config
UART_TX_DEFAULT ?= CYBSP_DEBUG_UART_TX
UART_RX_DEFAULT ?= CYBSP_DEBUG_UART_RX

PLATFORM_SUFFIX ?= cyw20829

# Add device name to defines
DEFINES += $(DEVICE)

# Default upgrade method
PLATFORM_DEFAULT_USE_OVERWRITE ?= 0

# Device flash start
FLASH_START := 0x60000000
FLASH_XIP_START := 0x08000000

###############################################################################
# Application specific libraries
###############################################################################
# MCUBootApp
###############################################################################
THIS_APP_PATH = $(PRJ_DIR)/libs

ifeq ($(APP_NAME), MCUBootApp)

PLATFORM_INCLUDE_DIRS_FLASH := $(PRJ_DIR)/platforms/cy_flash_pal
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/platforms/cy_flash_pal/flash_cyw20829/flash_qspi
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/platforms/cy_flash_pal/flash_cyw20829/include
PLATFORM_SOURCES_FLASH := $(wildcard $(PRJ_DIR)/platforms/cy_flash_pal/flash_cyw20829/*.c)
PLATFORM_SOURCES_FLASH += $(wildcard $(PRJ_DIR)/platforms/cy_flash_pal/flash_cyw20829/flash_qspi/*.c)

# Platform dependend utils files
PLATFORM_APP_SOURCES := $(PRJ_DIR)/platforms/utils/$(FAMILY)/cyw_platform_utils.c
PLATFORM_INCLUDE_DIRS_UTILS := $(PRJ_DIR)/platforms/utils/$(FAMILY)

# mbedTLS hardware acceleration settings
ifeq ($(USE_CRYPTO_HW), 1)
# cy-mbedtls-acceleration related include directories
INCLUDE_DIRS_MBEDTLS_CRYPTOLITE := $(PRJ_DIR)/platforms/crypto/$(FAMILY)/mbedtls_Cryptolite
# Collect source files for MbedTLS acceleration
SOURCES_MBEDTLS_CRYPTOLITE := $(wildcard $(PRJ_DIR)/platforms/crypto/$(FAMILY)/mbedtls_Cryptolite/*.c)
#
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_MBEDTLS_CRYPTOLITE))
# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_MBEDTLS_CRYPTOLITE)
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
PLATFORM_CHUNK_SIZE := 4096U
###############################################################################

###############################################################################
# MCUBootApp service app definitions
###############################################################################
# Service app is only used in SECURE lifecycle
ifeq ($(LCS), SECURE)
# Service app path and file name
SERVICE_APP_PATH := $(PRJ_DIR)/packets/apps/reprovisioning$(SERVICE_APP_PLATFORM_SUFFIX)
SERVICE_APP_NAME := cyapp_reprovisioning_signed_icv0

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
	@echo $(SHELL) $(PRJ_DIR)/run_toc2_generator.sh $(LCS) $(OUT_CFG) $(APP_NAME) $(APPTYPE) $(PROVISION_PATH) $(SMIF_CRYPTO_CONFIG) $(TOOLCHAIN_PATH) $(APP_DEFAULT_POLICY) $(BOOTLOADER_SIZE) $(ENC_IMG) $(PLATFORM_SERVICE_APP_DESC_OFFSET)
	$(shell        $(PRJ_DIR)/run_toc2_generator.sh $(LCS) $(OUT_CFG) $(APP_NAME) $(APPTYPE) $(PROVISION_PATH) $(SMIF_CRYPTO_CONFIG) $(TOOLCHAIN_PATH) $(APP_DEFAULT_POLICY) $(BOOTLOADER_SIZE) $(ENC_IMG) $(PLATFORM_SERVICE_APP_DESC_OFFSET))

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

# Basic settings
LCS ?= NORMAL_NO_SECURE
APPTYPE ?= flash
SIGN_TYPE ?= mcuboot_user_app
SMIF_CRYPTO_CONFIG ?= NONE

ifeq ($(LCS), NORMAL_NO_SECURE)
APP_DEFAULT_POLICY ?= $(PRJ_DIR)/policy/policy_no_secure.json
else
APP_DEFAULT_POLICY ?= $(PRJ_DIR)/policy/policy_secure.json
endif

PLATFORM_DEFAULT_ERASED_VALUE := 0xff

# Define start of application
PLATFORM_USER_APP_START ?= $(shell echo $$(($(PRIMARY_IMG_START)-$(FLASH_START)+$(FLASH_XIP_START))))
# Define RAM start and size, slot size
PLATFORM_DEFAULT_RAM_START ?= 0x2000C000
PLATFORM_DEFAULT_RAM_SIZE  ?= 0x10000

PLATFORM_DEFINES_APP += -DUSER_APP_START_OFF=0x20000

PLATFORM_INCLUDE_DIRS_FLASH := $(PRJ_DIR)/platforms/cy_flash_pal
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/platforms/cy_flash_pal/flash_cyw20829/flash_qspi
PLATFORM_INCLUDE_DIRS_FLASH += $(PRJ_DIR)/platforms/cy_flash_pal/flash_cyw20829/include
PLATFORM_SOURCES_FLASH += $(wildcard $(PRJ_DIR)/platforms/cy_flash_pal/flash_cyw20829/flash_qspi/*.c)

PLATFORM_DEFAULT_IMG_VER_ARG ?= 1.0.0

SIGN_IMG_ID = $(shell expr $(IMG_ID) - 1)
PLATFORM_SIGN_ARGS := --image-format $(SIGN_TYPE) -i $(OUT_CFG)/$(APP_NAME).final.bin -o $(OUT_CFG)/$(APP_NAME)$(UPGRADE_SUFFIX).bin --key-path $(PRJ_DIR)/keys/cypress-test-ec-p256.pem --update-key-path $(PRJ_DIR)/keys/priv_oem_0.pem --slot-size $(SLOT_SIZE) --align 1 --image-id $(SIGN_IMG_ID)

# Use encryption and random initial vector for image
ifeq ($(ENC_IMG), 1)
	PLATFORM_SIGN_ARGS += --encrypt --enckey ../../$(ENC_KEY_FILE).pem
	PLATFORM_SIGN_ARGS += --app-addr=$(PLATFORM_USER_APP_START)
endif

post_build: $(OUT_CFG)/$(APP_NAME).bin
ifeq ($(POST_BUILD_ENABLE), 1)
	$(info [POST BUILD] - Executing post build script for $(APP_NAME))
	$(info [TOC2_Generate] - Execute toc2 generator script for $(APP_NAME))
	$(shell $(PRJ_DIR)/run_toc2_generator.sh $(LCS) $(OUT_CFG) $(APP_NAME) $(APPTYPE) $(PRJ_DIR) $(SMIF_CRYPTO_CONFIG) $(TOOLCHAIN_PATH))

	$(info SIGN_ARGS <-> $(SIGN_ARGS))

	$(shell cysecuretools -q -t cyw20829 -p $(APP_DEFAULT_POLICY) sign-image $(SIGN_ARGS))

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
PLATFORM_SYSTEM_FILE_NAME := ns_system_$(PLATFORM_SUFFIX).c
PLATFORM_SOURCES_PDL_STARTUP := ns_start_$(PLATFORM_SUFFIX).c

PLATFORM_SOURCES_RETARGET_IO := $(wildcard $(PRJ_DIR)/libs/retarget-io/*.c)

PLATFORM_SOURCES_HAL := $(PRJ_DIR)/libs/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/source/pin_packages/cyhal_cyw20829_56_qfn.c
PLATFORM_SOURCES_HAL += $(PRJ_DIR)/libs/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/source/triggers/cyhal_triggers_cyw20829.c
PLATFORM_SOURCES_HAL += $(wildcard $(PRJ_DIR)/libs/mtb-hal-cat1/source/*.c)

PLATFORM_INCLUDE_DIRS_PDL_STARTUP := $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system

PLATFORM_INCLUDE_DIRS_RETARGET_IO := $(PRJ_DIR)/libs/retarget-io

PLATFORM_INCLUDE_DIRS_HAL := $(PRJ_DIR)/libs/mtb-hal-cat1/include
PLATFORM_INCLUDE_DIRS_HAL += $(PRJ_DIR)/libs/mtb-hal-cat1/include_pvt
PLATFORM_INCLUDE_DIRS_HAL += $(PRJ_DIR)/libs/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/
PLATFORM_INCLUDE_DIRS_HAL += $(PRJ_DIR)/libs/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/pin_packages
#PLATFORM_INCLUDE_DIRS_HAL += $(PRJ_DIR)/libs/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/triggers

PLATFORM_DEFINES_LIBS := -DCY_USING_HAL
PLATFORM_DEFINES_LIBS += -DCOMPONENT_CM33
PLATFORM_DEFINES_LIBS += -DCOMPONENT_PSOC6HAL
PLATFORM_DEFINES_LIBS += -DCOMPONENT_PSVP_CYW20829
PLATFORM_DEFINES_LIBS += -DCOMPONENT_SOFTFP
PLATFORM_DEFINES_LIBS += -DFLASH_BOOT

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### CYW20829.mk ####)
$(info APPTYPE <-> $(APPTYPE))
$(info APP_DEFAULT_POLICY <-> $(APP_DEFAULT_POLICY))
$(info APP_NAME <-- $(APP_NAME))
$(info BOOTLOADER_SIZE <-- $(BOOTLOADER_SIZE))
$(info CFLAGS_PLATFORM --> $(CFLAGS_PLATFORM))
$(info CORE <-> $(CORE))
$(info CORE_SUFFIX --> $(CORE_SUFFIX))
$(info CYW20829_PSVP <-- $(CYW20829_PSVP))
$(info DEFINES --> $(DEFINES))
$(info DEVICE <-> $(DEVICE))
$(info ENC_IMG <-- $(ENC_IMG))
$(info ENC_KEY_FILE <-- $(ENC_KEY_FILE))
$(info FAMILY <-- $(FAMILY))
$(info FLASH_START <-> $(FLASH_START))
$(info FLASH_XIP_START <-> $(FLASH_XIP_START))
$(info GCC_PATH <-- $(GCC_PATH))
$(info HEADER_FILES <-- $(HEADER_FILES))
$(info HEADER_OFFSET <-- $(HEADER_OFFSET))
$(info INCLUDE_DIRS_LIBS --> $(INCLUDE_DIRS_LIBS))
$(info INCLUDE_DIRS_MBEDTLS_CRYPTOLITE <-> $(INCLUDE_DIRS_MBEDTLS_CRYPTOLITE))
$(info LCS <-> $(LCS))
$(info LED_PIN_DEFAULT --> $(LED_PIN_DEFAULT))
$(info LED_PORT_DEFAULT --> $(LED_PORT_DEFAULT))
$(info OUT_CFG <-- $(OUT_CFG))
$(info PDL_CAT_SUFFIX <-> $(PDL_CAT_SUFFIX))
$(info PLATFORM_APP_SOURCES --> $(PLATFORM_APP_SOURCES))
$(info PLATFORM_CHUNK_SIZE --> $(PLATFORM_CHUNK_SIZE))
$(info PLATFORM_CY_MAX_EXT_FLASH_ERASE_SIZE --> $(PLATFORM_CY_MAX_EXT_FLASH_ERASE_SIZE))
$(info PLATFORM_DEFAULT_ERASED_VALUE --> $(PLATFORM_DEFAULT_ERASED_VALUE))
$(info PLATFORM_DEFAULT_IMG_VER_ARG --> $(PLATFORM_DEFAULT_IMG_VER_ARG))
$(info PLATFORM_DEFAULT_RAM_SIZE --> $(PLATFORM_DEFAULT_RAM_SIZE))
$(info PLATFORM_DEFAULT_RAM_START --> $(PLATFORM_DEFAULT_RAM_START))
$(info PLATFORM_DEFAULT_USE_OVERWRITE --> $(PLATFORM_DEFAULT_USE_OVERWRITE))
$(info PLATFORM_DEFINES_APP --> $(PLATFORM_DEFINES_APP))
$(info PLATFORM_DEFINES_LIBS --> $(PLATFORM_DEFINES_LIBS))
$(info PLATFORM_INCLUDE_DIRS_FLASH --> $(PLATFORM_INCLUDE_DIRS_FLASH))
$(info PLATFORM_INCLUDE_DIRS_HAL --> $(PLATFORM_INCLUDE_DIRS_HAL))
$(info PLATFORM_INCLUDE_DIRS_PDL_STARTUP --> $(PLATFORM_INCLUDE_DIRS_PDL_STARTUP))
$(info PLATFORM_INCLUDE_DIRS_RETARGET_IO --> $(PLATFORM_INCLUDE_DIRS_RETARGET_IO))
$(info PLATFORM_INCLUDE_DIRS_UTILS --> $(PLATFORM_INCLUDE_DIRS_UTILS))
$(info PLATFORM_SERVICE_APP_DESC_OFFSET <-- $(PLATFORM_SERVICE_APP_DESC_OFFSET))
$(info PLATFORM_SERVICE_APP_OFFSET <-- $(PLATFORM_SERVICE_APP_OFFSET))
$(info PLATFORM_SERVICE_APP_SIZE --> $(PLATFORM_SERVICE_APP_SIZE))
$(info PLATFORM_SIGN_ARGS --> $(PLATFORM_SIGN_ARGS))
$(info PLATFORM_SOURCES_FLASH --> $(PLATFORM_SOURCES_FLASH))
$(info PLATFORM_SOURCES_HAL --> $(PLATFORM_SOURCES_HAL))
$(info PLATFORM_SOURCES_PDL_RUNTIME --> $(PLATFORM_SOURCES_PDL_RUNTIME))
$(info PLATFORM_SOURCES_PDL_STARTUP --> $(PLATFORM_SOURCES_PDL_STARTUP))
$(info PLATFORM_SOURCES_RETARGET_IO --> $(PLATFORM_SOURCES_RETARGET_IO))
$(info PLATFORM_SUFFIX <-> $(PLATFORM_SUFFIX))
$(info PLATFORM_SYSTEM_FILE_NAME --> $(PLATFORM_SYSTEM_FILE_NAME))
$(info PLATFORM_USER_APP_START <-> $(PLATFORM_USER_APP_START))
$(info POST_BUILD_ENABLE <-- $(POST_BUILD_ENABLE))
$(info PRIMARY_IMG_START <-- $(PRIMARY_IMG_START))
$(info PRJ_DIR <-- $(PRJ_DIR))
$(info PROVISION_PATH <-> $(PROVISION_PATH))
$(info SERVICE_APP_NAME <-> $(SERVICE_APP_NAME))
$(info SERVICE_APP_PATH <-> $(SERVICE_APP_PATH))
$(info SERVICE_APP_PLATFORM_SUFFIX <-> $(SERVICE_APP_PLATFORM_SUFFIX))
$(info SHELL <-- $(SHELL))
$(info SIGN_ARGS <-- $(SIGN_ARGS))
$(info SIGN_TYPE <-> $(SIGN_TYPE))
$(info SLOT_SIZE <-- $(SLOT_SIZE))
$(info SMIF_CRYPTO_CONFIG <-> $(SMIF_CRYPTO_CONFIG))
$(info SOURCES_LIBS --> $(SOURCES_LIBS))
$(info SOURCES_MBEDTLS_CRYPTOLITE <-> $(SOURCES_MBEDTLS_CRYPTOLITE))
$(info TOOLCHAIN_PATH <-- $(TOOLCHAIN_PATH))
$(info UART_RX_DEFAULT --> $(UART_RX_DEFAULT))
$(info UART_TX_DEFAULT --> $(UART_TX_DEFAULT))
$(info UPGRADE_SUFFIX <-- $(UPGRADE_SUFFIX))
$(info USE_CRYPTO_HW <-> $(USE_CRYPTO_HW))
$(info USE_CUSTOM_MEMORY_MAP --> $(USE_CUSTOM_MEMORY_MAP))
$(info USE_EXTERNAL_FLASH --> $(USE_EXTERNAL_FLASH))
$(info USE_HW_ROLLBACK_PROT <-- $(USE_HW_ROLLBACK_PROT))
endif
