################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Main Makefile for building MCUBoot application for Cypress target.
#
################################################################################
# \copyright
# Copyright 2018-2021 Cypress Semiconductor Corporation
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

# minimum Python 3.7 is required
# Python path definition
ifeq ($(OS),Windows_NT)
PYTHON_PATH?=python
else
PYTHON_PATH?=python3
endif

################################################################################
# Main settings
################################################################################

# Defines whether or not show verbose build output
VERBOSE ?= 0
# Application name by default
APP_NAME ?= MCUBootApp
# Weather or now execute post build script after build - set to 0 for CI
POST_BUILD_ENABLE ?= 1

# Default number of GCC compilation threads
THREADS_NUM ?= 8

SIGN_KEY_FILE ?= cypress-test-ec-p256
ENC_KEY_FILE ?= enc-ec256-pub
ENC_IMG ?= 0

# set this variable to a path, where cysecuretools python package is installed
# use command `python -m pip show cysecuretools` to find out this path
# or rely on scripts that automates this action, bit not work for virtual envs
CY_SEC_TOOLS_PATH ?= $(shell $(PYTHON_PATH) $(CURDIR)/scripts/find_cysectools.py)

BUILDCFG ?= Debug

ifeq ($(PLATFORM), CYW20829)
# until mbedtls.3.0 support
USE_CRYPTO_HW ?= 0
endif

# Set of supported applications
APPS := MCUBootApp BlinkyApp

HEADER_OFFSET ?= 0

# Defines whether or not make all compile warnings into errors for application
# source code (but not for library source code)
WARN_AS_ERR ?= 1

ifneq ($(filter $(APP_NAME), $(APPS)),)
include ./$(APP_NAME)/$(APP_NAME).mk
include ./$(APP_NAME)/libs.mk
else
$(error Not supported application: '$(APP_NAME)')
endif

ASM_FILES := $(ASM_FILES_APP)
ASM_FILES += $(ASM_FILES_LIBS)

C_FILES := $(SOURCES_APP)
C_FILES += $(SOURCES_PLATFORM)
C_LIBS := $(SOURCES_LIBS)

INCLUDE_DIRS := $(INCLUDE_DIRS_APP)
INCLUDE_DIRS += $(INCLUDE_DIRS_MCUBOOT)
INCLUDE_DIRS += $(INCLUDE_DIRS_LIBS)

#INCLUDE_FILES := $(INCLUDE_FILES_APP)

#INCLUDES := $(addprefix -include , $(INCLUDE_FILES))

O_FILES := $(notdir $(C_FILES:.c=.o)) $(addsuffix .o, $(notdir $(basename $(ASM_FILES))))
O_LIBS := $(notdir $(C_LIBS:.c=.o))

DEFINES := $(DEFINES_APP) -D$(APP_NAME)
DEFINES += $(DEFINES_LIBS)
AS_FLAGS += $(DEFINES)

ifeq ($(VERBOSE), 1)
$(info ==============================================================================)
$(info = Directories to look for header files: =)
$(info ==============================================================================)
$(info $(INCLUDE_DIRS))

$(info ==============================================================================)
$(info = Collected Defines string: =)
$(info ==============================================================================)
$(info $(DEFINES))
endif

# updating CFLAGS at this point as DEFINES are completed
CFLAGS += $(DEFINES) $(CFLAGS_OPTIMIZATION)

ifeq ($(WARN_AS_ERR), 1)
CC_WARN_IN_ERR_FLAGS := -Werror
endif

VPATH = $(dir $(C_FILES) $(ASM_FILES) $(C_LIBS))

LDFLAGS += $(LDFLAGS_OPTIMIZATION)

# Default name pattern for output files
# may be modified in %Application%.mk file
OUT_FILE_NAME ?= $(OUT_APP)/$(APP_NAME)

OUT_OBJ := $(OUT_CFG)/obj
OUT_OBJ_LIBS_DIR := $(OUT_CFG)/obj/libs
OUT_APP := $(OUT_CFG)

.PHONY: all app build clean pre_build post_build

all: clean app

app:
	@`mkdir -p ./$(OUT)`
	@`mkdir -p ./$(OUT_TARGET)`
	@`mkdir -p ./$(OUT_CFG)`
	@`mkdir -p ./$(OUT_OBJ)`
	@`mkdir -p ./$(OUT_OBJ_LIBS_DIR)`
ifeq ($(VERBOSE) , 1)
	@echo
	@echo ======================== PRE_BUILD STAGE ========================
	@echo =================================================================
	@echo
endif
	$(MAKE) pre_build
ifeq ($(VERBOSE) , 1)
	@echo
	@echo ======================== BUILD STAGE ========================
	@echo =============================================================
	@echo
endif
	$(MAKE) build -j $(THREADS_NUM)
ifeq ($(VERBOSE) , 1)
	@echo
	@echo ======================== POST_BUILD STAGE ========================
	@echo ==================================================================
	@echo
endif
	$(MAKE) post_build

build: $(OUT_APP)/$(APP_NAME)_unsigned.hex
	$(GCC_PATH)/bin/arm-none-eabi-objdump $(OUT_APP)/$(APP_NAME).elf -S --disassemble > $(OUT_APP)/$(APP_NAME).lst
	$(GCC_PATH)/bin/arm-none-eabi-objdump -h $(OUT_APP)/$(APP_NAME).elf
	$(GCC_PATH)/bin/arm-none-eabi-size --format=SysV $(OUT_APP)/$(APP_NAME).elf

$(OUT_APP)/$(APP_NAME)_unsigned.hex: $(OUT_APP)/$(APP_NAME).bin
	$(GCC_PATH)/bin/arm-none-eabi-objcopy --change-addresses=$(HEADER_OFFSET) -O ihex $(OUT_APP)/$(APP_NAME).elf $(OUT_APP)/$(APP_NAME)_unsigned.hex

$(OUT_APP)/$(APP_NAME).bin: $(OUT_APP)/$(APP_NAME).elf
	$(GCC_PATH)/bin/arm-none-eabi-objcopy $(OUT_APP)/$(APP_NAME).elf -S -O binary $(OUT_APP)/$(APP_NAME).bin --remove-section .cy_sflash_user_data --remove-section .cy_toc_part2

$(OUT_APP)/$(APP_NAME).elf: $(addprefix $(OUT_OBJ)/, $(O_FILES)) $(addprefix $(OUT_OBJ_LIBS_DIR)/, $(O_LIBS))
	@echo "LD $@"
ifeq ($(VERBOSE), 1)
	@echo
	@echo $(LD) $(O_FILES) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -T $(LINKER_SCRIPT) -Wl,-Map,$(OUT_FILE_NAME).map
endif
	@$(LD) $(addprefix $(OUT_OBJ)/, $(O_FILES)) $(addprefix $(OUT_OBJ_LIBS_DIR)/, $(O_LIBS)) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -T $(LINKER_SCRIPT) -Wl,-Map,$(OUT_FILE_NAME).map


$(OUT_OBJ)/%.o: %.c
	@echo "CC $<"
ifeq ($(VERBOSE), 1)
	@echo
	@echo $(CC) $(CFLAGS) $(CC_WARN_IN_ERR_FLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
endif
	@$(CC) $(CFLAGS) $(CC_WARN_IN_ERR_FLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@

$(OUT_OBJ)/%.o: %.S
	@echo "AS $<"
ifeq ($(COMPILER), GCC_ARM)
ifeq ($(VERBOSE), 1)
	@echo
	@echo @$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
endif
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
else
	@echo $(AS) $< -o $@ $(AS_FLAGS)
	@$(AS) $< -o $@ $(AS_FLAGS)
endif

$(OUT_OBJ_LIBS_DIR)/%.o: %.c
	@echo "CC $<"
ifeq ($(VERBOSE), 1)
	@echo
	@echo $(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
endif
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@

clean:
	@echo "Cleanup out directory..."
	rm -f ./$(APP_NAME)/flashmap.mk ./cy_flash_pal/cy_flash_map.h
	rm -rf $(OUT_TARGET)/$(BUILDCFG)

clean_boot:
	@echo "Cleanup out BOOT directory of $(APP_NAME)..."
	rm -f ./$(APP_NAME)/flashmap.mk
	rm -rf $(OUT_TARGET)/$(BUILDCFG)/boot

clean_upgrade:
	@echo "Cleanup out UPGRADE directory of $(APP_NAME)..."
	rm -f ./$(APP_NAME)/flashmap.mk
	rm -rf $(OUT_TARGET)/$(BUILDCFG)/upgrade

run_cppcheck:
	@echo "Performing static code analysis with Cppcheck tool..."
	cppcheck/cppcheck.sh $(APP_NAME) $(PLATFORM) "$(DEFINES)" "$(INCLUDE_DIRS)" "$(C_FILES)" $(CPP_CHECK_SCOPE) $(BUILDCFG)

gen_key_ecc256:
	@echo Generate ECC256 keys: $(SIGN_KEY_FILE).pem and $(SIGN_KEY_FILE).pub
	../../scripts/imgtool.py keygen -k keys/$(SIGN_KEY_FILE).pem -t ecdsa-p256
	../../scripts/imgtool.py getpub -k keys/$(SIGN_KEY_FILE).pem > keys/$(SIGN_KEY_FILE).pub

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### Makefile ####)
$(info APPS <-> $(APPS))
$(info APP_NAME <-> $(APP_NAME))
$(info AS <-- $(AS))
$(info ASM_FILES <-> $(ASM_FILES))
$(info ASM_FILES_APP <-- $(ASM_FILES_APP))
$(info ASM_FILES_LIBS <-- $(ASM_FILES_LIBS))
$(info AS_FLAGS <-> $(AS_FLAGS))
$(info BUILDCFG <-> $(BUILDCFG))
$(info CC <-- $(CC))
$(info CC_DEPEND <-- $(CC_DEPEND))
$(info CC_WARN_IN_ERR_FLAGS <-> $(CC_WARN_IN_ERR_FLAGS))
$(info CFLAGS <-> $(CFLAGS))
$(info CFLAGS_OPTIMIZATION <-- $(CFLAGS_OPTIMIZATION))
$(info COMPILER <-- $(COMPILER))
$(info CPP_CHECK_SCOPE <-- $(CPP_CHECK_SCOPE))
$(info CURDIR <-- $(CURDIR))
$(info CY_SEC_TOOLS_PATH --> $(CY_SEC_TOOLS_PATH))
$(info C_FILES <-> $(C_FILES))
$(info C_LIBS <-> $(C_LIBS))
$(info DEFINES <-> $(DEFINES))
$(info DEFINES_APP <-- $(DEFINES_APP))
$(info DEFINES_LIBS <-- $(DEFINES_LIBS))
$(info ENC_IMG --> $(ENC_IMG))
$(info ENC_KEY_FILE --> $(ENC_KEY_FILE))
$(info GCC_PATH <-- $(GCC_PATH))
$(info HEADER_OFFSET <-> $(HEADER_OFFSET))
$(info INCLUDE_DIRS <-> $(INCLUDE_DIRS))
$(info INCLUDE_DIRS_APP <-- $(INCLUDE_DIRS_APP))
$(info INCLUDE_DIRS_LIBS <-- $(INCLUDE_DIRS_LIBS))
$(info INCLUDE_DIRS_MCUBOOT <-- $(INCLUDE_DIRS_MCUBOOT))
$(info LD <-- $(LD))
$(info LDFLAGS <-> $(LDFLAGS))
$(info LDFLAGS_OPTIMIZATION <-- $(LDFLAGS_OPTIMIZATION))
$(info LINKER_SCRIPT <-- $(LINKER_SCRIPT))
$(info MAKE <-- $(MAKE))
$(info OS <-- $(OS))
$(info OUT <-- $(OUT))
$(info OUT_APP <-> $(OUT_APP))
$(info OUT_CFG <-- $(OUT_CFG))
$(info OUT_FILE_NAME <-> $(OUT_FILE_NAME))
$(info OUT_OBJ <-> $(OUT_OBJ))
$(info OUT_OBJ_LIBS_DIR <-> $(OUT_OBJ_LIBS_DIR))
$(info OUT_TARGET <-- $(OUT_TARGET))
$(info O_FILES <-> $(O_FILES))
$(info O_LIBS <-> $(O_LIBS))
$(info PLATFORM <-- $(PLATFORM))
$(info POST_BUILD_ENABLE --> $(POST_BUILD_ENABLE))
$(info PYTHON_PATH <-> $(PYTHON_PATH))
$(info SIGN_KEY_FILE <-> $(SIGN_KEY_FILE))
$(info SOURCES_APP <-- $(SOURCES_APP))
$(info SOURCES_LIBS <-- $(SOURCES_LIBS))
$(info SOURCES_PLATFORM <-- $(SOURCES_PLATFORM))
$(info THREADS_NUM <-> $(THREADS_NUM))
$(info USE_CRYPTO_HW --> $(USE_CRYPTO_HW))
$(info WARN_AS_ERR <-> $(WARN_AS_ERR))
endif
