################################################################################
# \file targets.mk
# \version 1.0
#
# \brief
# Makefile to describe supported boards and platforms for Cypress MCUBoot based applications.
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

# Target platform MCUBootApp is built for. PSOC_062_2M is set by default# Supported:
#   - PSOC_062_2M

# default PLATFORM
PLATFORM ?= PSOC_062_2M
#
PLATFORMS := PSOC_062_2M

# For which core this application is built
CORE := CM0P

# Set paths for related folders
PLATFORM_PATH := $(CURDIR)/platforms

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
ifeq ($(PLATFORM), PSOC_062_2M)
DEVICE ?= CY8C624ABZI-D44
PLATFORM_SUFFIX := 02
endif

# Collect C source files for PLATFORM
SOURCES_PLATFORM += $(wildcard $(PLATFORM_PATH)/*.c)
SOURCES_PLATFORM := $(filter-out %/system_psoc6_cm4.c, $(SOURCES_PLATFORM))

# Collect dirrectories containing headers for PLATFORM
INCLUDE_DIRS_PLATFORM := $(PLATFORM_PATH)
# Collect Assembler files for PLATFORM
# Include _01_, _02_ or _03_ PLATFORM_SUFFIX depending on device family.
STARTUP_FILE := $(PLATFORM_PATH)/$(PLATFORM)/$(CORE)/$(COMPILER)/startup_psoc6_$(PLATFORM_SUFFIX)_cm0plus

ifeq ($(COMPILER), GCC_ARM)
	ASM_FILES_PLATFORM := $(STARTUP_FILE).S
else
$(error Only GCC ARM is supported at this moment)
endif

# Add device name to defines
DEFINES += $(DEVICE)
DEFINES += $(PLATFORM)

# Convert defines it to regular -DMY_NAME style
ifneq ($(DEFINES),)
	DEFINES_PLATFORM :=$(addprefix -D, $(subst -,_,$(DEFINES)))
endif

ifeq ($(COMPILER), GCC_ARM)
LINKER_SCRIPT ?= $(PLATFORM_PATH)/$(PLATFORM)/$(CORE)/$(COMPILER)/*_cm0plus.ld
else
$(error Only GCC ARM is supported at this moment)
endif

ifeq ($(MAKEINFO) , 1)
$(info $(SOURCES_PLATFORM))
$(info $(ASM_FILES_PLATFORM))
endif
