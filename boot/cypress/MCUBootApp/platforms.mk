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

# Target platform MCUBootApp is built for. PSOC_064_2M is set by default# Supported:
#   - PSOC_062_2M

# default PLATFORM
PLATFORM ?= PSOC_062_2M
#
PLATFORMS := PSOC_062_2M

# For which core this application is built
CORE := CM0P

# Set paths for related folders
CUR_LIBS_PATH := $(CURDIR)/libs
PLATFORM_PATH := $(CURDIR)/platforms

# MCU device selection, based on target device.
# Default chips are used for supported platforms
# This can be redefined in case of other chip usage
ifeq ($(PLATFORM), PSOC_062_2M)
DEVICE ?= CY8C624ABZI-D44
PLATFORM_SUFFIX := 02
endif

# Additional components supported by the target
COMPONENTS+=COMPONENT_BSP_DESIGN_MODUS
# Use CyHAL
DEFINES:=CY_USING_HAL

# Collect C source files for PLATFORM BSP
SOURCES_PLATFORM += $(wildcard $(PLATFORM_PATH)/*.c)
SOURCES_PLATFORM += $(wildcard $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/source/*.c)
SOURCES_PLATFORM += $(wildcard $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/source/triggers/*.c)
SOURCES_PLATFORM += $(wildcard $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/source/pin_packages/*.c)

SOURCES_PLATFORM := $(filter-out %/system_psoc6_cm4.c, $(SOURCES_PLATFORM))

# Collect dirrectories containing headers for PLATFORM BSP
INCLUDE_DIRS_PLATFORM := $(PLATFORM_PATH)
INCLUDE_DIRS_PLATFORM += $(CUR_LIBS_PATH)/psoc6hal/include
INCLUDE_DIRS_PLATFORM += $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/include
INCLUDE_DIRS_PLATFORM += $(CUR_LIBS_PATH)/psoc6hal/COMPONENT_PSOC6HAL/include/pin_packages
# Collect Assembler files for PLATFORM BSP
# Include _01_, _02_ or _03_ PLATFORM_SUFFIX depending on device family.
STARTUP_FILE := $(PLATFORM_PATH)/$(PLATFORM)/$(CORE)/$(COMPILER)/startup_psoc6_$(PLATFORM_SUFFIX)_cm0plus

ifeq ($(COMPILER), GCC_ARM)
	ASM_FILES_PLATFORM := $(STARTUP_FILE).S
else
$(error Only GCC ARM is supported at this moment)
endif

# Add device name from BSP makefile to defines
DEFINES += $(DEVICE)
DEFINES += $(COMPONENTS)
DEFINES += $(PLATFORM)

# Get defines from BSP makefile and convert it to regular -DMY_NAME style
ifneq ($(DEFINES),)
	DEFINES_PLATFORM :=$(addprefix -D, $(subst -,_,$(DEFINES)))
endif

ifeq ($(COMPILER), GCC_ARM)
LINKER_SCRIPT ?= $(PLATFORM_PATH)/$(PLATFORM)/$(CORE)/$(COMPILER)/*_cm0plus.ld
else
$(error Only GCC ARM is supported at this moment)
endif

ifeq ($(MAKEINFO) , 1)
$(info ==============================================================================)
$(info = BSP files =)
$(info ==============================================================================)
$(info $(SOURCES_PLATFORM))
$(info $(ASM_FILES_PLATFORM))
endif

# TODO: include appropriate BSP linker(s)
# TODO: include appropriate BSP precompiled
