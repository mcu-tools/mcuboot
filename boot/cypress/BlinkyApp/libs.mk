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
SOURCES_RETARGET_IO := $(wildcard $(CUR_LIBS_PATH)/retarget-io/*.c)
SOURCES_WATCHDOG := $(wildcard $(CUR_LIBS_PATH)/watchdog/*.c)

# Collect source files for HAL
SOURCES_HAL_BLINKY := $(wildcard $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/source/*.c)
SOURCES_HAL_BLINKY += $(wildcard $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/COMPONENT_CAT1A/source/*.c)
SOURCES_HAL_BLINKY += $(wildcard $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/COMPONENT_CAT1A/source/triggers/*.c)
SOURCES_HAL_BLINKY += $(wildcard $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/COMPONENT_CAT1A/source/pin_packages/*.c)

# Retarget-io related include directories
INCLUDE_DIRS_RETARGET_IO := $(CUR_LIBS_PATH)/retarget-io
INCLUDE_DIRS_WATCHDOG := $(CUR_LIBS_PATH)/watchdog

# Collect dirrectories containing headers for PSOC6 HAL
INCLUDE_DIRS_HAL_BLINKY := $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/include
INCLUDE_DIRS_HAL_BLINKY += $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/COMPONENT_CAT1A/include
INCLUDE_DIRS_HAL_BLINKY += $(CUR_LIBS_PATH)/mtb-hal-cat1/include
INCLUDE_DIRS_HAL_BLINKY += $(CUR_LIBS_PATH)/mtb-hal-cat1/COMPONENT_PSOC6HAL/COMPONENT_CAT1A/include/pin_packages

# Collected source files for libraries
SOURCES_LIBS += $(SOURCES_WATCHDOG)
SOURCES_LIBS += $(SOURCES_RETARGET_IO)
SOURCES_LIBS += $(SOURCES_HAL_BLINKY)

# Collected include directories for libraries
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_WATCHDOG))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_RETARGET_IO))
INCLUDE_DIRS_LIBS += $(addprefix -I,$(INCLUDE_DIRS_HAL_BLINKY))
