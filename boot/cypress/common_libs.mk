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
C_FILES += $(wildcard $(CY_LIBS_PATH)/mtb-pdl-cat1/drivers/source/*.c)
C_FILES += $(wildcard $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/source/*.c)

COMPONENT_CORE_PATH := $(PRJ_DIR)/platforms/BSP/$(FAMILY)/system/COMPONENT_$(CORE)

# PDL startup related files
SYSTEM_FILE_NAME := $(PLATFORM_SYSTEM_FILE_NAME)
C_FILES += $(wildcard $(COMPONENT_CORE_PATH)/$(SYSTEM_FILE_NAME))
C_FILES += $(wildcard (COMPONENT_CORE_PATH)/$(PLATFORM_SOURCES_PDL_STARTUP))

# Collect source files for Retarget-io
C_FILES += $(wildcard $(PRJ_DIR)/libs/retarget-io/*.c)

# HAL source files
C_FILES += $(wildcard $(CY_LIBS_PATH)/mtb-hal-cat1/source/*.c)
C_FILES += $(wildcard $(CY_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/source/pin_packages/*.c)
C_FILES += $(wildcard $(CY_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/source/triggers/*.c)

C_FILES += $(wildcard $(PRJ_DIR)/platforms/BSP/$(FAMILY)/*.c)
C_FILES += $(wildcard $(PRJ_DIR)/platforms/security_counter/*.c)
C_FILES += $(wildcard $(PRJ_DIR)/platforms/security_counter/$(FAMILY)/*.c)

ifneq ($(APP_NAME), BlinkyApp)
    C_FILES += $(wildcard $(PRJ_DIR)/platforms/memory/*.c)
    C_FILES += $(wildcard $(PRJ_DIR)/platforms/memory/$(FAMILY)/*.c)
endif

ifeq ($(USE_EXTERNAL_FLASH), 1)
    C_FILES += $(wildcard $(PRJ_DIR)/platforms/memory/external_memory/*.c)
    C_FILES += $(wildcard $(PRJ_DIR)/platforms/memory/$(FAMILY)/flash_qspi/*.c)
endif

INCLUDE_DIRS += $(PRJ_DIR)/platforms/boot_rng

C_FILES += $(wildcard $(PRJ_DIR)/platforms/boot_rng/*.c)
C_FILES += $(wildcard $(PRJ_DIR)/platforms/boot_rng/$(FAMILY)/*.c)


# PDL related include directories
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-pdl-cat1/drivers/include
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-pdl-cat1/drivers/third_party/ethernet/include
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/ip
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-pdl-cat1/devices/COMPONENT_CAT$(PDL_CAT_SUFFIX)/templates/COMPONENT_MTB

# HAL related include directories
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-hal-cat1/include
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-hal-cat1/include_pvt
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/pin_packages
INCLUDE_DIRS += $(CY_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT$(PDL_CAT_SUFFIX)/include/triggers

INCLUDE_DIRS += $(CY_LIBS_PATH)/cmsis/Core/Include

# core-libs related include directories
INCLUDE_DIRS += $(CY_LIBS_PATH)/core-lib/include

# PDL startup related files
INCLUDE_DIRS += $(COMPONENT_CORE_PATH)/HEADER_FILES

# Retarget-io related include directories
INCLUDE_DIRS += $(THIS_APP_PATH)/retarget-io

# Include platforms folder
INCLUDE_DIRS += $(PRJ_DIR)/platforms/BSP/$(FAMILY)
INCLUDE_DIRS += $(PRJ_DIR)/platforms/security_counter/$(FAMILY)
INCLUDE_DIRS += $(PRJ_DIR)/platforms/security_counter
INCLUDE_DIRS += $(PRJ_DIR)/platforms/memory
INCLUDE_DIRS += $(PRJ_DIR)/platforms/memory/flash_map_backend
INCLUDE_DIRS += $(PRJ_DIR)/platforms/memory/$(FAMILY)
INCLUDE_DIRS += $(PRJ_DIR)/platforms/memory/$(FAMILY)/include

ifeq ($(USE_EXTERNAL_FLASH), 1)
    INCLUDE_DIRS += $(PRJ_DIR)/platforms/memory/external_memory
    INCLUDE_DIRS += $(PRJ_DIR)/platforms/memory/$(FAMILY)/flash_qspi
endif

# Assembler startup file for platform
ASM_FILES += $(PLATFORM_STARTUP_FILE)

# Syslib files
ASM_FILES += $(CY_LIBS_PATH)/mtb-pdl-cat1/drivers/source/TOOLCHAIN_GCC_ARM/cy_syslib_ext.S

DEFINES += -DCOMPONENT_CAT1
DEFINES += -DCOMPONENT_CAT$(PDL_CAT_SUFFIX)

