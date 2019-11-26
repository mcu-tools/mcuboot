################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Main Makefile for building MCUBoot application for Cypress target.
#
################################################################################
# \copyright
# Copyright 2019 Cypress Semiconductor Corporation
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

# Defines whether or not show verbose build output
MAKEINFO ?= 1
# Application name by default
APP_NAME ?= MCUBootApp

SIGN_KEY_FILE ?= cypress-test-ec-p256

BUILDCFG ?= Debug

# Set of supported applications
APPS := MCUBootApp BlinkyApp
# Output folder
OUT := $(APP_NAME)/out
# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(TARGET)

OUT_CFG := $(OUT_TARGET)/$(BUILDCFG)
OUT_OBJ := $(OUT_CFG)/obj
OUT_APP := $(OUT_CFG)

HEADER_OFFSET ?= 0

ifneq ($(filter $(APP_NAME), $(APPS)),)
include ./$(APP_NAME)/$(APP_NAME).mk
else
$(error Not supported application: '$(APP_NAME)')
endif

ASM_FILES := $(ASM_FILES_APP)
ASM_FILES += $(ASM_FILES_LIBS)

C_FILES := $(SOURCES_APP)
C_FILES += $(SOURCES_LIBS)

INCLUDE_DIRS := $(INCLUDE_DIRS_APP)
INCLUDE_DIRS += $(INCLUDES_DIRS_MCUBOOT)
INCLUDE_DIRS += $(INCLUDE_DIRS_LIBS)

O_FILES := $(notdir $(C_FILES:.c=.o)) $(addsuffix .o, $(notdir $(basename $(ASM_FILES))))

DEFINES := $(DEFINES_APP)
DEFINES += $(DEFINES_LIBS)
AS_FLAGS += $(DEFINES)

ifeq ($(MAKEINFO), 1)
$(info ==============================================================================)
$(info = Directories to look for header files: =)
$(info ==============================================================================)
$(info $(INCLUDE_DIRS))

$(info ==============================================================================)
$(info = Collected Defines string: =)
$(info ==============================================================================)
$(info $(DEFINES))
endif
DEFINES +=

# updating CFLAGS at this point as DEFINES are completed
CFLAGS += $(DEFINES)

VPATH = $(dir $(C_FILES) $(ASM_FILES))

#
# STDE: For cygwin, adjust paths for compiler
#
MY_FILES := $(subst /cygdrive/c,c:,$(C_FILES))
#$(info MY_FILES $(MY_FILES))
C_FILES=$(MY_FILES)

MY_DIRS := $(subst /cygdrive/c,c:,$(INCLUDE_DIRS))
#$(info MY_DIRS $(MY_DIRS))
INCLUDE_DIRS=$(MY_DIRS)

MY_ASM_FILES := $(subst /cygdrive/c,c:,$(ASM_FILES))
#$(info MY_ASM_FILES $(MY_ASM_FILES))
ASM_FILES=$(MY_ASM_FILES)

MY_LDFLAGS := $(subst /cygdrive/c,c:,$(LDFLAGS))
#$(info MY_LDFLAGS $(MY_LDFLAGS))
LDFLAGS=$(MY_LDFLAGS)

#
# STDE: For cygwin, adjust paths for compiler
#
MY_FILES := $(subst /cygdrive/c,c:,$(C_FILES))
#$(info MY_FILES $(MY_FILES))
C_FILES=$(MY_FILES)

MY_DIRS := $(subst /cygdrive/c,c:,$(INCLUDE_DIRS))
#$(info MY_DIRS $(MY_DIRS))
INCLUDE_DIRS=$(MY_DIRS)

MY_ASM_FILES := $(subst /cygdrive/c,c:,$(ASM_FILES))
#$(info MY_ASM_FILES $(MY_ASM_FILES))
ASM_FILES=$(MY_ASM_FILES)

MY_LDFLAGS := $(subst /cygdrive/c,c:,$(LDFLAGS))
#$(info MY_LDFLAGS $(MY_LDFLAGS))
LDFLAGS=$(MY_LDFLAGS)

# Default name pattern for output files
# may be modified in %Application%.mk file
OUT_FILE_NAME ?= $(OUT_APP)/$(APP_NAME)

# minimum Python 3.7 is required
# Python path definition
ifeq ($(OS),Windows_NT)
PYTHON_PATH?=python
else
PYTHON_PATH?=python3
endif

.PHONY: all app build clean pre_build post_build load load_boot load_upgrade

all: clean app

app:
	@`mkdir -p ./$(OUT)`
	@`mkdir -p ./$(OUT_TARGET)`
	@`mkdir -p ./$(OUT_CFG)`
	@`mkdir -p ./$(OUT_OBJ)`
	$(MAKE) pre_build
	$(MAKE) build -j8
	$(MAKE) post_build

build: $(OUT_APP)/$(APP_NAME).hex
	$(GCC_PATH)/bin/arm-none-eabi-objdump $(OUT_APP)/$(APP_NAME).hex -s > $(OUT_APP)/$(APP_NAME).lst
	$(GCC_PATH)/bin/arm-none-eabi-size --format=SysV $(OUT_APP)/$(APP_NAME).elf

$(OUT_APP)/$(APP_NAME).hex: $(OUT_APP)/$(APP_NAME).elf
	$(GCC_PATH)/bin/arm-none-eabi-objcopy --change-addresses=$(HEADER_OFFSET) -O ihex $(OUT_APP)/$(APP_NAME).elf $(OUT_APP)/$(APP_NAME).hex

$(OUT_APP)/$(APP_NAME).elf: $(addprefix $(OUT_OBJ)/, $(O_FILES))
	@echo "LD $@"
ifeq ($(MAKEINFO), 1)
	@echo $(LD) $(O_FILES) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -T $(LINKER_SCRIPT) -Wl,-Map,$(OUT_FILE_NAME).map
endif
	@$(LD) $(addprefix $(OUT_OBJ)/, $(O_FILES)) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -T $(LINKER_SCRIPT) -Wl,-Map,$(OUT_FILE_NAME).map


$(OUT_OBJ)/%.o: %.c
	@echo "CC $<"
ifeq ($(MAKEINFO), 1)
	@echo $(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
endif
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
ifeq ($(MAKEINFO), 1)
	@echo
endif

$(OUT_OBJ)/%.o: %.S
	@echo "AS $<"
ifeq ($(COMPILER), GCC_ARM)
ifeq ($(MAKEINFO), 1)
	@echo @$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
endif
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
else
	@echo $(AS) $< -o $@ $(AS_FLAGS)
	@$(AS) $< -o $@ $(AS_FLAGS)
endif
ifeq ($(MAKEINFO), 1)
	@echo
endif

clean:
	@echo "Cleanup out directory..."
	rm -rf ./$(APP_NAME)/out

# TODO: update with generic solution
load:
ifeq ($(HOST_OS), win)
	cp $(OUT)/$(TARGET)/$(BUILDCFG)/$(APP_NAME).hex /d/
else
	cp $(OUT)/$(TARGET)/$(BUILDCFG)/$(APP_NAME).hex /Volumes/DAPLINK
endif

load_boot:
ifeq ($(HOST_OS), win)
	cp $(OUT)/$(TARGET)/$(BUILDCFG)/$(APP_NAME)_signed.hex /d/
else
	cp $(OUT)/$(TARGET)/$(BUILDCFG)/$(APP_NAME)_signed.hex /Volumes/DAPLINK
endif

load_upgrade:
ifeq ($(HOST_OS), win)
	cp $(OUT)/$(TARGET)/$(BUILDCFG)/$(APP_NAME)_signed_upgrade.hex /d/
else
	cp $(OUT)/$(TARGET)/$(BUILDCFG)/$(APP_NAME)_signed_upgrade.hex /Volumes/DAPLINK
endif

gen_key_ecc256:
	@echo Generate ECC256 keys: $(SIGN_KEY_FILE).pem and $(SIGN_KEY_FILE).pub
	../../scripts/imgtool.py keygen -k keys/$(SIGN_KEY_FILE).pem -t ecdsa-p256
	../../scripts/imgtool.py getpub -k keys/$(SIGN_KEY_FILE).pem > keys/$(SIGN_KEY_FILE).pub

ifeq ($(MAKEINFO) , 1)
$(info ASM_FILES: $(ASM_FILES))
$(info C_FILES: $(C_FILES))
$(info INCLUDE_DIRS: $(INCLUDE_DIRS))
$(info DEFINES: $(DEFINES))
$(info CC: $(CC))
endif
