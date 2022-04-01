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
CY_LIBS_PATH = $(PRJ_DIR)/libs

# Collect common source files for PDL
SOURCES_PDL := $(wildcard $(CY_LIBS_PATH)/mtb-pdl-cat1/drivers/source/*.c)
SOURCES_PDL += $(wildcard $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/source/*.c)

COMPONENT_CORE_PATH := $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system/COMPONENT_$(CORE)

# PDL startup related files
SYSTEM_FILE_NAME := $(PLATFORM_SYSTEM_FILE_NAME)
SOURCES_PDL_SYSTEM := $(COMPONENT_CORE_PATH)/$(SYSTEM_FILE_NAME)
SOURCES_PDL_STARTUP := $(COMPONENT_CORE_PATH)/$(PLATFORM_SOURCES_PDL_STARTUP)

# Collect source files for Retarget-io
SOURCES_RETARGET_IO := $(PLATFORM_SOURCES_RETARGET_IO)

# HAL source files
SOURCES_HAL := $(PLATFORM_SOURCES_HAL)

# Add platform folder to build
SOURCES_PLATFORM := $(wildcard $(PRJ_DIR)/platforms/BSP/$(FAMILY)/*.c)
SOURCES_PLATFORM += $(wildcard $(PRJ_DIR)/platforms/security_counter/*.c)
SOURCES_PLATFORM += $(wildcard $(PRJ_DIR)/platforms/security_counter/$(FAMILY)/*.c)

# PDL related include directories
INCLUDE_DIRS_PDL := $(CY_LIBS_PATH)/mtb-pdl-cat1/drivers/include
INCLUDE_DIRS_PDL += $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/ip
INCLUDE_DIRS_PDL += $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include
INCLUDE_DIRS_PDL += $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/templates/COMPONENT_MTB

INCLUDE_DIRS_CMSIS += $(CY_LIBS_PATH)/cmsis/Core/Include

# core-libs related include directories
INCLUDE_DIRS_CORE_LIB := $(CY_LIBS_PATH)/core-lib/include

# PDL startup related files
INCLUDE_DIRS_PDL_STARTUP += $(COMPONENT_CORE_PATH)/HEADER_FILES

# Retarget-io related include directories
INCLUDE_DIRS_RETARGET_IO := $(PLATFORM_INCLUDE_DIRS_RETARGET_IO)

# HAL include directories files
INCLUDE_DIRS_HAL := $(PLATFORM_INCLUDE_DIRS_HAL)

# Include platforms folder
INCLUDE_DIRS_PLATFORM := $(PRJ_DIR)/platforms//BSP/$(FAMILY)
INCLUDE_DIRS_PLATFORM += $(PRJ_DIR)/platforms/security_counter/$(FAMILY)
INCLUDE_DIRS_PLATFORM += $(PRJ_DIR)/platforms/security_counter
INCLUDE_DIRS_PLATFORM += $(PLATFORM_INCLUDE_DIRS_PDL_STARTUP)

# Assembler startup file for platform
ASM_FILES_STARTUP := $(PLATFORM_STARTUP_FILE)

# Collected source files for libraries
SOURCES_LIBS := $(SOURCES_PDL)
SOURCES_LIBS += $(SOURCES_PDL_SYSTEM)
SOURCES_LIBS += $(SOURCES_PDL_STARTUP)
SOURCES_LIBS += $(SOURCES_PDL_RUNTIME)
SOURCES_LIBS += $(SOURCES_HAL)
SOURCES_LIBS += $(SOURCES_RETARGET_IO)

# Collected include directories for libraries
INCLUDE_DIRS_LIBS := $(addprefix -I,$(INCLUDE_DIRS_PDL))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_CMSIS))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_PDL_STARTUP))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_CORE_LIB))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_HAL))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_PLATFORM))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_RETARGET_IO))

# Syslib files
ASM_FILES_PDL += $(CY_LIBS_PATH)/mtb-pdl-cat1/drivers/source/TOOLCHAIN_GCC_ARM/cy_syslib_ext.S

ASM_FILES_LIBS := $(ASM_FILES_PDL)

# Add define for PDL version
DEFINES_PDL += -DPDL_VERSION=$(PDL_VERSION)

DEFINES_LIBS := $(PLATFORM_DEFINES)
DEFINES_LIBS += $(PLATFORM_DEFINES_LIBS)
DEFINES_LIBS += $(DEFINES_PDL)
DEFINES_LIBS += -DCOMPONENT_CAT1
DEFINES_LIBS += -DCOMPONENT_CAT$(PDL_CAT_SUFFIX)

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### common_libs.mk ####)
$(info ASM_FILES_LIBS --> $(ASM_FILES_LIBS))
$(info ASM_FILES_PDL <-> $(ASM_FILES_PDL))
$(info ASM_FILES_STARTUP --> $(ASM_FILES_STARTUP))
$(info COMPONENT_CORE_PATH <-> $(COMPONENT_CORE_PATH))
$(info CORE <-- $(CORE))
$(info CY_LIBS_PATH <-- $(CY_LIBS_PATH))
$(info DEFINES_LIBS --> $(DEFINES_LIBS))
$(info DEFINES_PDL <-> $(DEFINES_PDL))
$(info FAMILY <-- $(FAMILY))
$(info INCLUDE_DIRS_CORE_LIB <-> $(INCLUDE_DIRS_CORE_LIB))
$(info INCLUDE_DIRS_HAL <-> $(INCLUDE_DIRS_HAL))
$(info INCLUDE_DIRS_LIBS --> $(INCLUDE_DIRS_LIBS))
$(info INCLUDE_DIRS_PDL <-> $(INCLUDE_DIRS_PDL))
$(info INCLUDE_DIRS_PDL_STARTUP <-> $(INCLUDE_DIRS_PDL_STARTUP))
$(info INCLUDE_DIRS_PLATFORM <-> $(INCLUDE_DIRS_PLATFORM))
$(info INCLUDE_DIRS_RETARGET_IO <-> $(INCLUDE_DIRS_RETARGET_IO))
$(info PDL_CAT_SUFFIX <-- $(PDL_CAT_SUFFIX))
$(info PDL_VERSION <-- $(PDL_VERSION))
$(info PLATFORM_DEFINES <-- $(PLATFORM_DEFINES))
$(info PLATFORM_DEFINES_LIBS <-- $(PLATFORM_DEFINES_LIBS))
$(info PLATFORM_INCLUDE_DIRS_HAL <-- $(PLATFORM_INCLUDE_DIRS_HAL))
$(info PLATFORM_INCLUDE_DIRS_RETARGET_IO <-- $(PLATFORM_INCLUDE_DIRS_RETARGET_IO))
$(info PLATFORM_SOURCES_HAL <-- $(PLATFORM_SOURCES_HAL))
$(info PLATFORM_SOURCES_PDL_STARTUP <-- $(PLATFORM_SOURCES_PDL_STARTUP))
$(info PLATFORM_SOURCES_RETARGET_IO <-- $(PLATFORM_SOURCES_RETARGET_IO))
$(info PLATFORM_STARTUP_FILE <-- $(PLATFORM_STARTUP_FILE))
$(info PLATFORM_SYSTEM_FILE_NAME <-- $(PLATFORM_SYSTEM_FILE_NAME))
$(info PRJ_DIR <-- $(PRJ_DIR))
$(info SOURCES_HAL <-> $(SOURCES_HAL))
$(info SOURCES_LIBS --> $(SOURCES_LIBS))
$(info SOURCES_PDL <-> $(SOURCES_PDL))
$(info SOURCES_PDL_RUNTIME <-- $(SOURCES_PDL_RUNTIME))
$(info SOURCES_PDL_STARTUP <-> $(SOURCES_PDL_STARTUP))
$(info SOURCES_PDL_SYSTEM <-> $(SOURCES_PDL_SYSTEM))
$(info SOURCES_PLATFORM --> $(SOURCES_PLATFORM))
$(info SOURCES_RETARGET_IO <-> $(SOURCES_RETARGET_IO))
$(info SYSTEM_FILE_NAME <-> $(SYSTEM_FILE_NAME))
endif
