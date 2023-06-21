################################################################################
# \file toolchains.mk
# \version 1.0
#
# \brief
# Makefile to describe supported toolchains for Cypress MCUBoot based applications.
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

# Compilers
GCC_ARM	:= 1
IAR		:= 2
ARM		:= 3
OTHER 	:= 4

ifeq ($(VERBOSE), 1)
$(info $(COMPILER))
endif

# Path to the compiler installation
# NOTE: Absolute pathes for now for the sake of development
ifeq ($(HOST_OS), win)
	ifeq ($(COMPILER), GCC_ARM)
		TOOLCHAIN_PATH ?= c:/Users/$(USERNAME)/ModusToolbox/tools_2.4/gcc
		MY_TOOLCHAIN_PATH := $(call get_os_path, $(TOOLCHAIN_PATH))
		TOOLCHAIN_PATH := $(MY_TOOLCHAIN_PATH)
		GCC_PATH := $(TOOLCHAIN_PATH)
		# executables
		CC := "$(GCC_PATH)/bin/arm-none-eabi-gcc"
		LD := $(CC)
	endif

else ifeq ($(HOST_OS), osx)
	TOOLCHAIN_PATH ?= /opt/gcc-arm-none-eabi
	GCC_PATH := $(TOOLCHAIN_PATH)

	CC := "$(GCC_PATH)/bin/arm-none-eabi-gcc"
	LD := $(CC)

else ifeq ($(HOST_OS), linux)
	TOOLCHAIN_PATH ?= /opt/gcc-arm-none-eabi
	GCC_PATH := $(TOOLCHAIN_PATH)
	# executables
	CC := "$(GCC_PATH)/bin/arm-none-eabi-gcc"
	LD := $(CC)
endif

PDL_ELFTOOL := $(TOOLCHAIN_PATH)/../cymcuelftool-1.0/bin/cymcuelftool

OBJDUMP  := "$(GCC_PATH)/bin/arm-none-eabi-objdump"
OBJCOPY  := "$(GCC_PATH)/bin/arm-none-eabi-objcopy"

# Set flags for toolchain executables
ifeq ($(COMPILER), GCC_ARM)
	# set build-in compiler flags
	CFLAGS_COMMON :=  -mthumb -ffunction-sections -fdata-sections  -g -Wall -Wextra
	CFLAGS_COMMON += -Wno-discarded-qualifiers -Wno-ignored-qualifiers # KILLME

	ifeq ($(WARN_AS_ERR), 1)
		CFLAGS_COMMON += -Werror
	endif

	ifeq ($(BUILDCFG), Debug)
		CFLAGS_SPECIAL ?= -Og -g3
		CFLAGS_COMMON += $(CFLAGS_SPECIAL)
	else ifeq ($(BUILDCFG), Release)
		ifeq ($(CFLAGS_OPTIMIZATION), )
			# Blinky upgrade releas XIP WORKAROUND
			CFLAGS_COMMON += -Os -g -DNDEBUG
		endif
	else
$(error BUILDCFG : '$(BUILDCFG)' is not supported)
	endif

	CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_PLATFORM) $(INCLUDES)

	CC_DEPEND = -MD -MP -MF

	LDFLAGS_COMMON := -mcpu=cortex-$(CORE_SUFFIX) -mthumb -specs=nano.specs -ffunction-sections -fdata-sections  -Wl,--gc-sections -ffat-lto-objects -g --enable-objc-gc

	ifeq ($(WARN_AS_ERR), 1)
		LDFLAGS_COMMON += -Wl,--fatal-warnings
	endif

	ifeq ($(BUILDCFG), Debug)
		LDFLAGS_SPECIAL ?= -Og
		LDFLAGS_COMMON += $(LDFLAGS_SPECIAL)
	else ifeq ($(BUILDCFG), Release)
		ifeq ($(CFLAGS_OPTIMIZATION), )
			# Blinky upgrade releas XIP WORKAROUND
			LDFLAGS_OPTIMIZATION ?= -Os
		endif
	else
$(error BUILDCFG : '$(BUILDCFG)' is not supported)
	endif
	LDFLAGS_NANO := -L "$(GCC_PATH)/arm-none-eabi/lib/thumb/v6-m"
	LDFLAGS := $(LDFLAGS_COMMON) $(LDFLAGS_NANO) $(LDFLAGS_PLATFORM)
endif

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### toolchains.mk ####)
$(info ARM --> $(ARM))
$(info BUILDCFG <-- $(BUILDCFG))
$(info CC <-> $(CC))
$(info CFLAGS --> $(CFLAGS))
$(info CFLAGS_COMMON <-> $(CFLAGS_COMMON))
$(info CFLAGS_PLATFORM <-- $(CFLAGS_PLATFORM))
$(info CFLAGS_SPECIAL <-> $(CFLAGS_SPECIAL))
$(info COMPILER <-- $(COMPILER))
$(info CORE_SUFFIX <-- $(CORE_SUFFIX))
$(info GCC_ARM --> $(GCC_ARM))
$(info GCC_PATH <-> $(GCC_PATH))
$(info HOST_OS <-- $(HOST_OS))
$(info IAR --> $(IAR))
$(info INCLUDES <-- $(INCLUDES))
$(info LD --> $(LD))
$(info LDFLAGS --> $(LDFLAGS))
$(info LDFLAGS_COMMON <-> $(LDFLAGS_COMMON))
$(info LDFLAGS_NANO <-> $(LDFLAGS_NANO))
$(info LDFLAGS_OPTIMIZATION --> $(LDFLAGS_OPTIMIZATION))
$(info LDFLAGS_SPECIAL <-> $(LDFLAGS_SPECIAL))
$(info MY_TOOLCHAIN_PATH <-> $(MY_TOOLCHAIN_PATH))
$(info OBJCOPY --> $(OBJCOPY))
$(info OBJDUMP --> $(OBJDUMP))
$(info OTHER --> $(OTHER))
$(info PDL_ELFTOOL --> $(PDL_ELFTOOL))
$(info TOOLCHAIN_PATH <-> $(TOOLCHAIN_PATH))
$(info USERNAME <-- $(USERNAME))
endif
