################################################################################
# \file common_libs.mk
# \version 1.0
#
# \brief
# Makefile to describe libraries needed for Cypress MCUBoot based applications.
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

include host.mk

################################################################################
# PDL library
################################################################################
PSOC6_LIBS_PATH = $(PRJ_DIR)/libs

ifeq ($(CORE),CM0P)
CORE_SIFFX=m0plus
else
CORE_SIFFX=m4
endif

# Collect source files for PDL
SOURCES_PDL := $(wildcard $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/drivers/source/*.c)
SOURCES_PDL += $(wildcard $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT1A/source/*.c)

# PDL startup related files
SOURCES_PDL_STARTUP := $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT1A/templates/COMPONENT_MTB/COMPONENT_$(CORE)/system_psoc6_c$(CORE_SIFFX).c

# PDL related include directories
INCLUDE_DIRS_PDL := $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/drivers/include
INCLUDE_DIRS_PDL += $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT1A/include/ip
INCLUDE_DIRS_PDL += $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT1A/include
INCLUDE_DIRS_PDL += $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/cmsis/include

# PDL startup related files
INCLUDE_DIRS_PDL_STARTUP := $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT1A/templates/COMPONENT_MTB

# core-libs related include directories
INCLUDE_DIRS_CORE_LIB := $(PSOC6_LIBS_PATH)/core-lib/include

STARTUP_FILE := $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT1A/templates/COMPONENT_MTB/COMPONENT_$(CORE)/TOOLCHAIN_$(COMPILER)/startup_psoc6_$(PLATFORM_SUFFIX)_c$(CORE_SIFFX)

ifeq ($(COMPILER), GCC_ARM)
	ASM_FILES_STARTUP := $(STARTUP_FILE).S
else
$(error Only GCC ARM is supported at this moment)
endif


# Collected source files for libraries
SOURCES_LIBS := $(SOURCES_PDL)
SOURCES_LIBS += $(SOURCES_PDL_STARTUP)

# Collected include directories for libraries
INCLUDE_DIRS_LIBS := $(addprefix -I,$(INCLUDE_DIRS_PDL))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_PDL_STARTUP))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_CORE_LIB))

ASM_FILES_PDL :=
ifeq ($(COMPILER), GCC_ARM)
ASM_FILES_PDL += $(PSOC6_LIBS_PATH)/mtb-pdl-cat1/drivers/source/TOOLCHAIN_GCC_ARM/cy_syslib_gcc.S
else
$(error Only GCC ARM is supported at this moment)
endif

ASM_FILES_LIBS := $(ASM_FILES_PDL)

# Add define for PDL version
DEFINES_PDL += -DPDL_VERSION=$(PDL_VERSION)

DEFINES_LIBS := $(DEFINES_PLATFORM)
DEFINES_LIBS += $(DEFINES_PDL)
