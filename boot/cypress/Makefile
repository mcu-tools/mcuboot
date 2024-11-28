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

################################################################################
# Main settings
################################################################################

# Set of supported applications
APPS := MCUBootApp BlinkyApp

################################################################################
# Includes
################################################################################

# Compiler variables
include ./CC.mk
# Build settings
include ./build_config.mk

SIGN_KEY_FILE ?= cypress-test-ec-p256
SECURE_MODE_KEY_NAME ?= cypress-test-rsa2k
SECURE_MODE_KEY_TYPE ?= RSA2048
ENC_KEY_FILE ?= enc-ec256-pub
ENC_IMG ?= 0

HEADER_OFFSET ?= 0

ifneq ($(filter $(APP_NAME), $(APPS)),)
include ./$(APP_NAME)/$(APP_NAME).mk
include ./$(APP_NAME)/libs.mk
else
$(error Not supported application: '$(APP_NAME)')
endif

O_FILES := $(notdir $(C_FILES:.c=.o)) $(addsuffix .o, $(notdir $(basename $(ASM_FILES))))

DEFINES += -D$(APP_NAME)
AS_FLAGS += $(DEFINES)
INCLUDE_DIRS := $(addprefix -I, $(INCLUDE_DIRS))

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

VPATH = $(dir $(C_FILES) $(ASM_FILES))

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

$(OUT_APP)/$(APP_NAME).elf: $(addprefix $(OUT_OBJ)/, $(O_FILES))
	@echo "LD $@"
ifeq ($(VERBOSE), 1)
	@echo
	@echo $(LD) $(O_FILES) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -T $(LINKER_SCRIPT) -Wl,-Map,$(OUT_FILE_NAME).map
endif
	@$(LD) $(addprefix $(OUT_OBJ)/, $(O_FILES)) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -T $(LINKER_SCRIPT) -Wl,-Map,$(OUT_FILE_NAME).map


$(OUT_OBJ)/%.o: %.c
	@echo "CC $<"
ifeq ($(VERBOSE), 1)
	@echo
	@echo $(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
endif
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@

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
	rm -f ./$(APP_NAME)/memorymap.mk ./platforms/memory/cy_flash_map.h ./platforms/memory/memorymap.h ./platforms/memory/memorymap.c
	rm -rf $(OUT_TARGET)/$(BUILDCFG)

clean_boot:
	@echo "Cleanup out BOOT directory of $(APP_NAME)..."
	rm -f ./$(APP_NAME)/memorymap.mk
	rm -rf $(OUT_TARGET)/$(BUILDCFG)/boot

clean_upgrade:
	@echo "Cleanup out UPGRADE directory of $(APP_NAME)..."
	rm -f ./$(APP_NAME)/memorymap.mk
	rm -rf $(OUT_TARGET)/$(BUILDCFG)/upgrade

run_cppcheck:
	@echo "Performing static code analysis with Cppcheck tool..."
	cppcheck/cppcheck.sh $(APP_NAME) $(PLATFORM) "$(DEFINES)" "$(INCLUDE_DIRS)" "$(C_FILES)" $(CPP_CHECK_SCOPE) $(BUILDCFG)

gen_key_ecc256:
	@echo Generate ECC256 keys: $(SIGN_KEY_FILE).pem and $(SIGN_KEY_FILE).pub
	../../scripts/imgtool.py keygen -k keys/$(SIGN_KEY_FILE).pem -t ecdsa-p256
	../../scripts/imgtool.py getpub -k keys/$(SIGN_KEY_FILE).pem > keys/$(SIGN_KEY_FILE).pub

gen_secure_cfgs:
ifeq ($(SECURE_MODE_KEY_NAME), cypress-test-rsa2k)
	@echo "Generating public $(SECURE_MODE_KEY_TYPE) keys"
	cysecuretools -t $(PLATFORM) create-key --key-type $(SECURE_MODE_KEY_TYPE) -o ./keys/$(SECURE_MODE_KEY_NAME).pem ./keys/$(SECURE_MODE_KEY_NAME).pub --format PEM
endif
	@echo "Generating secure mode key config"
	cysecuretools convert-key -k ./keys/$(SECURE_MODE_KEY_NAME).pub -o ./platforms/utils/$(FAMILY)/cy_si_key.c --fmt secure_boot --endian little
