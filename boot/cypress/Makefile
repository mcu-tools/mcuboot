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
# Set of supported applications
APPS := MCUBootApp BlinkyApp
# Output folder
OUT := $(APP_NAME)/out
# Output folder to contain build artifacts
OUT_TARGET := $(OUT)/$(TARGET)
# Defines whether or not image should be signed
SIGN ?= 0

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

#DEFINES := -D$(CHIP_ID) -DCY_FB_OPT_$(SI_REVISION) -DNDEBUG -DCY_CORE_ID=0 $(DEFINES_CRYPTO) $(DEFINES_PDL) $(DEFINES_OTHER) -DCY_FB_BUILD_NUMBER=$(CY_BUILD_NUMBER)UL $(PSVP_DEFINES) $(DEFINES_JWT)

## helper suffix for versioning support
#SUFFIX := $(SUFFIX_FAMILY)_$(SUFFIX_VERSION)
#
#ifeq ($(CY_BUILD_NUMBER), )
#	# may be useful when building from a command line on a local machine
#	CY_BUILD_NUMBER := 10
#endif
#
#################################################################################
## Application sources
#################################################################################
#SOURCES_APPLICATION =
#
#################################################################################
## Common section
#################################################################################
#
## Project includes path define
#INCLUDES := $(INCLUDES_JWT) $(INCLUDES_CRYPTOLIB) $(INCLUDES_PDL) $(INCLUDES_MBED_CRYPTO) -I"./hal"  -I"./crypto" -I.
#
## Other defines
#DEFINES_OTHER := -DFB_SECURE_ENABLED -DCY_FLASH_RWW_DRV_SUPPORT_DISABLED
#ifneq ($(FAMILY), $(FAMILY_PSOC6A_512K))
#	DEFINES_OTHER := $(DEFINES_OTHER) -DCY_FB_ATTESTATON_ENABLE
#endif
#
#ifneq (, $(findstring _psvp_, $(SUFFIX)))
#	PSVP_DEFINES := -DCY_FB_OPT_PSVP
#endif
#
#DEFINES := -D$(CHIP_ID) -DCY_FB_OPT_$(SI_REVISION) -DNDEBUG -DCY_CORE_ID=0 $(DEFINES_CRYPTO) $(DEFINES_PDL) $(DEFINES_OTHER) -DCY_FB_BUILD_NUMBER=$(CY_BUILD_NUMBER)UL $(PSVP_DEFINES) $(DEFINES_JWT)
#
#DEFINES += -DCY_FB_FAMILY=$(FAMILY) -DCY_FB_SI_REV=$(SI_REV)
#
#
#ASM_FILES += $(ASM_FILES_PDL)
#C_FILES := $(SOURCES_CRYPTOLIB) $(SOURCES_MBED_CRYPTO) $(SOURCES_JWT) $(SOURCES_BSP) $(SOURCES_PDL) $(SOURCES_FB)
#C_FILES += $(wildcard hal/*.c)

VPATH = $(dir $(C_FILES) $(ASM_FILES))

## Set colors for unix compatible terminals, otherwise comment this code
#ifeq ($(JENKINS), yes)
#	COLOR_RESET    = \033[0m
#	COLOR_make_std = \033[3$1m      # defined for 1 through 7
#	COLOR_make     = \033[38;5;$1m  # defined for 1 through 255
#	COLOR_WRN = $(strip $(call COLOR_make,11))
#	COLOR_ERR = $(strip $(call COLOR_make,9))
#	COLOR_STD = $(strip $(call COLOR_make,243))
#
#	COLOR_OUTPUT = 2>&1 |                                         \
#		while IFS='' read -r line; do                             \
#			if  [[ $$line == *Error\[* ]]; then                   \
#				echo -e -n "$(COLOR_ERR)";                        \
#				echo -n $${line};                                 \
#				echo -e "$(COLOR_RESET)";                         \
#			elif [[ $$line == *Warning\[* ]]; then                \
#				echo -e -n "$(COLOR_WRN)";                        \
#				echo -n $${line};                                 \
#				echo -e "$(COLOR_RESET)";                         \
#			else                                                  \
#				echo -e -n "$(COLOR_STD)";                        \
#				echo -n $${line};                                 \
#				echo -e "$(COLOR_RESET)";                         \
#			fi;                                                   \
#		done; exit $${PIPESTATUS[0]};
#else
#	COLOR_RESET    =
#	COLOR_make_std =
#	COLOR_make     =
#	COLOR_WRN =
#	COLOR_ERR =
#	COLOR_STD =
#
#	COLOR_OUTPUT =
#endif
#
#

# Default name pattern for output files
# may be modified in %Application%.mk file
OUT_FILE_NAME ?= $(OUT_TARGET)/$(APP_NAME)

.PHONY: all app build clean

all: clean app

app:
	@`mkdir -p ./$(OUT)`
	@`mkdir -p ./$(OUT)/obj`
	@`mkdir -p ./$(OUT_TARGET)`
	$(MAKE) build -j8

build: $(OUT_FILE_NAME).hex
	$(GCC_PATH)/bin/arm-none-eabi-objdump $(OUT_FILE_NAME).hex -s > $(OUT_FILE_NAME).lst
	$(GCC_PATH)/bin/arm-none-eabi-size --format=berkeley $(OUT_FILE_NAME).elf

$(OUT_FILE_NAME).hex: $(OUT_FILE_NAME).elf
	$(GCC_PATH)/bin/arm-none-eabi-objcopy --change-addresses=$(HEADER_OFFSET) -O ihex $(OUT_FILE_NAME).elf $(OUT_FILE_NAME).hex
ifeq ($(SIGN), 1)
	python3 ../../scripts/imgtool.py sign -k $(APP_NAME)/keys/cypress-test-ec-p256.pem -v "1.0" --align 8 -H 1024 -S 65536 --pad-header --overwrite-only $(OUT_FILE_NAME).hex $(OUT_FILE_NAME)_signed.hex
	$(GCC_PATH)/bin/arm-none-eabi-objdump $(OUT_FILE_NAME)_signed.hex -s > $(OUT_FILE_NAME)_signed.lst
endif

$(OUT_FILE_NAME).elf: $(addprefix $(OUT)/obj/, $(O_FILES))
	@echo "LD $@"
ifeq ($(MAKEINFO), 1)
	@echo $(LD) $(O_FILES) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -Wl,-Map,$(OUT_FILE_NAME).map
endif
	@$(LD) $(addprefix $(OUT)/obj/, $(O_FILES)) $(CC_DEPEND) $(@:.o=.d) -o $@ $(LDFLAGS) -Wl,-Map,$(OUT_FILE_NAME).map


$(OUT)/obj/%.o: %.c
	@echo "CC $<"
ifeq ($(MAKEINFO), 1)
	@echo $(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
endif
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) $(CC_DEPEND) $(@:.o=.d) -c $< -o $@
ifeq ($(MAKEINFO), 1)
	@echo
endif

$(OUT)/obj/%.o: %.S
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
upload:
ifeq ($(HOST_OS), win)
	cp $(OUT_FILE_NAME).hex /d/
else
	cp $(OUT_FILE_NAME).hex /Volumes/DAPLINK 
endif

ifeq ($(MAKEINFO) , 1)
$(info ASM_FILES: $(ASM_FILES))
$(info C_FILES: $(C_FILES))
$(info INCLUDE_DIRS: $(INCLUDE_DIRS))
$(info DEFINES: $(DEFINES))
$(info CC: $(CC))
endif
