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

# Target PLATFORM BlinkyApp is built for. PSOC_064_2M is set as default
# Supported:
#	- PSOC_064_2M
#	- PSOC_064_1M
#	- PSOC_064_512K
#	- PSOC_062_2M

# default TARGET
PLATFORM ?= PSOC_064_2M
#
SB_PLATFORMS := PSOC_064_2M PSOC_064_1M PSOC_064_512K
PLATFORMS := PSOC_062_2M $(SB_PLATFORMS)

# For which core this application is built
CORE := CM4

# Set paths for related folders
CUR_LIBS_PATH := $(CURDIR)/libs
PLATFORMS_PATH := $(CURDIR)/platforms
PLATFORM_PATH := $(PLATFORMS_PATH)/$(PLATFORM)

# Target dependent definitions
ifeq ($(PLATFORM), PSOC_064_2M)
DEVICE ?= CYB0644ABZI-S2D44
PLATFORM_SUFFIX := 02
else ifeq ($(PLATFORM), PSOC_064_1M)
DEVICE ?= CYB06447BZI-BLD53
PLATFORM_SUFFIX := 01
else ifeq ($(PLATFORM), PSOC_064_512K)
DEVICE ?= CYB06445LQI-S3D42
PLATFORM_SUFFIX := 03
else ifeq ($(PLATFORM), PSOC_062_2M)
DEVICE ?= CY8C624ABZI-D44
PLATFORM_SUFFIX := 02
endif

# Check if path to cysecuretools is set in case Secure Boot target
ifneq ($(filter $(PLATFORM), $(SB_PLATFORMS)),)
ifeq ($(CY_SEC_TOOLS_PATH), )
$(error Variable CY_SEC_TOOLS_PATH - path to cysecuretools package not set. \
		Use `python -m pip show cysecuretools` to determine intallation folder.` \
		Then set it in Makefile to continue work.)
endif
endif

# Collect C source files for PLATFORM
SOURCES_PLATFORM += $(wildcard $(PLATFORMS_PATH)/*.c)
SOURCES_PLATFORM += $(wildcard $(PLATFORM_PATH)/$(CORE)/*.c)
# Exclude system file for cm4
SOURCES_PLATFORM := $(filter-out %/system_psoc6_cm0plus.c, $(SOURCES_PLATFORM))

# Collect dirrectories containing headers for PLATFORM
INCLUDE_DIRS_PLATFORM := $(PLATFORMS_PATH)
INCLUDE_DIRS_PLATFORM += $(PLATFORM_PATH)/$(CORE)

# Collect Assembler files for PLATFORM
STARTUP_FILE := $(PLATFORM_PATH)/$(CORE)/$(COMPILER)/startup_psoc6_$(PLATFORM_SUFFIX)_cm4

ifeq ($(COMPILER), GCC_ARM)
	ASM_FILES_PLATFORM := $(STARTUP_FILE).S
else
$(error Only GCC ARM is supported at this moment)
endif

# Add device name from PLATFORM makefile to defines
DEFINES += $(DEVICE)

# Get defines from PLATFORM makefile and convert it to regular -DMY_NAME style
ifneq ($(DEFINES),)
	DEFINES_PLATFORM :=$(addprefix -D, $(subst -,_,$(DEFINES)))
endif

DEFINES_PLATFORM += $(addprefix -D, $(PLATFORM))

ifneq ($(COMPILER), GCC_ARM)
$(error Only GCC ARM is supported at this moment)
endif
ifeq ($(MAKEINFO) , 1)
$(info ==============================================================================)
$(info = PLATFORM files =)
$(info ==============================================================================)
$(info $(SOURCES_PLATFORM))
$(info $(ASM_FILES_PLATFORM))
endif
