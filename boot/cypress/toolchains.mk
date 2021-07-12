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

include common_libs.mk

# Compilers
GCC_ARM	:= 1
IAR		:= 2
ARM		:= 3
OTHER 	:= 4

ifeq ($(MAKEINFO), 1)
$(info $(COMPILER))
endif
# Detect host OS to make resolving compiler pathes easier
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
	HOST_OS = osx
else
	ifeq ($(UNAME_S), Linux)
		HOST_OS = linux
	else
		HOST_OS = win
	endif
endif

# Path to the compiler installation
# NOTE: Absolute pathes for now for the sake of development
ifeq ($(HOST_OS), win)
	ifeq ($(COMPILER), GCC_ARM)
		TOOLCHAIN_PATH ?= c:/Users/$(USERNAME)/ModusToolbox/tools_2.2/gcc
		MY_TOOLCHAIN_PATH:=$(subst \,/,$(TOOLCHAIN_PATH))
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

PDL_ELFTOOL := "hal/tools/$(HOST_OS)/elf/cymcuelftool"

OBJDUMP  := "$(GCC_PATH)/bin/arm-none-eabi-objdump"
OBJCOPY  := "$(GCC_PATH)/bin/arm-none-eabi-objcopy"

# Set flags for toolchain executables
ifeq ($(COMPILER), GCC_ARM)
	# set build-in compiler flags
	CFLAGS_COMMON := -mcpu=cortex-$(CORE_SIFFX) -mthumb -mfloat-abi=soft -fno-stack-protector -ffunction-sections -fdata-sections -ffat-lto-objects -fstrict-aliasing -g -Wall -Wextra
	# preset default level of optimization - can be changed in application.mk
	ifeq ($(BUILDCFG), Debug)
		CFLAGS_OPTIMIZATION ?= -Og -g3
	else ifeq ($(BUILDCFG), Release)
		CFLAGS_OPTIMIZATION ?= -Os -g
	else
$(error BUILDCFG : '$(BUILDCFG)' is not supported)
	endif
	# add defines and includes
	CFLAGS := $(CFLAGS_COMMON) $(INCLUDES)
	CC_DEPEND = -MD -MP -MF

	LDFLAGS_COMMON := -mcpu=cortex-$(CORE_SIFFX) -mthumb -specs=nano.specs -ffunction-sections -fdata-sections  -Wl,--gc-sections -L "$(GCC_PATH)/lib/gcc/arm-none-eabi/7.2.1/thumb/v6-m" -ffat-lto-objects -g --enable-objc-gc
	ifeq ($(BUILDCFG), Debug)
	# preset default level of optimization - can be changed in application.mk
		LDFLAGS_OPTIMIZATION ?= -Og
	else ifeq ($(BUILDCFG), Release)
		LDFLAGS_OPTIMIZATION ?= -Os
	else
$(error BUILDCFG : '$(BUILDCFG)' is not supported)
	endif
	LDFLAGS_NANO := -L "$(GCC_PATH)/arm-none-eabi/lib/thumb/v6-m"
	LDFLAGS := $(LDFLAGS_COMMON) $(LDFLAGS_NANO)
endif
