################################################################################
# \file libs.mk
# \version 1.0
#
# \brief
# Makefile to describe libraries needed for Cypress MCUBoot based applications.
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

################################################################################
# PDL library
################################################################################
PDL_VERSION = 121
#
CUR_LIBS_PATH = $(PRJ_DIR)/libs

# Collect source files for Retarget-io
ifneq ($(PLATFORM), CYW20829)
SOURCES_RETARGET_IO := $(wildcard $(CUR_LIBS_PATH)/retarget-io/*.c)
endif
SOURCES_WATCHDOG := $(wildcard $(CUR_LIBS_PATH)/watchdog/*.c)

# Collect source files for HAL
ifneq ($(PLATFORM), CYW20829)
SOURCES_HAL_BLINKY := $(wildcard $(CUR_LIBS_PATH)/mtb-hal-cat1/source/*.c)
SOURCES_HAL_BLINKY += $(wildcard $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT1A/source/triggers/*.c)
SOURCES_HAL_BLINKY += $(wildcard $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT1A/source/pin_packages/*.c)
endif

# Retarget-io related include directories
INCLUDE_DIRS_RETARGET_IO := $(CUR_LIBS_PATH)/retarget-io
INCLUDE_DIRS_WATCHDOG := $(CUR_LIBS_PATH)/watchdog

# Collect dirrectories containing headers for PSOC6 HAL
ifneq ($(PLATFORM), CYW20829)
INCLUDE_DIRS_HAL_BLINKY := $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT1A
INCLUDE_DIRS_HAL_BLINKY := $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT1A/include
INCLUDE_DIRS_HAL_BLINKY += $(CUR_LIBS_PATH)/mtb-hal-cat1/include
INCLUDE_DIRS_HAL_BLINKY += $(CUR_LIBS_PATH)/mtb-hal-cat1/include_pvt
INCLUDE_DIRS_HAL_BLINKY += $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT1A/include/pin_packages
INCLUDE_DIRS_HAL_BLINKY += $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_CAT1A/include/triggers
endif

# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_WATCHDOG)
ifneq ($(PLATFORM), CYW20829)
SOURCES_LIBS += $(SOURCES_RETARGET_IO)
SOURCES_LIBS += $(SOURCES_HAL_BLINKY)
endif

# Collected include directories for libraries
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_WATCHDOG))
ifneq ($(PLATFORM), CYW20829)
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_RETARGET_IO))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_HAL_BLINKY))
endif

###############################################################################
# Print debug information about all settings used and/or set in this file
ifeq ($(VERBOSE), 1)
$(info #### libs.mk ####)
$(info CUR_LIBS_PATH <-- $(CUR_LIBS_PATH))
$(info INCLUDE_DIRS_HAL_BLINKY <-> $(INCLUDE_DIRS_HAL_BLINKY))
$(info INCLUDE_DIRS_LIBS --> $(INCLUDE_DIRS_LIBS))
$(info INCLUDE_DIRS_RETARGET_IO <-> $(INCLUDE_DIRS_RETARGET_IO))
$(info INCLUDE_DIRS_WATCHDOG <-> $(INCLUDE_DIRS_WATCHDOG))
$(info PLATFORM <-- $(PLATFORM))
$(info PRJ_DIR <-- $(PRJ_DIR))
$(info SOURCES_HAL_BLINKY <-> $(SOURCES_HAL_BLINKY))
$(info SOURCES_LIBS --> $(SOURCES_LIBS))
$(info SOURCES_RETARGET_IO <-> $(SOURCES_RETARGET_IO))
$(info SOURCES_WATCHDOG <-> $(SOURCES_WATCHDOG))
endif
